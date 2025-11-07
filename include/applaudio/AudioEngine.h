//
//  AudioEngine.h
//  applaudio
//
//  Created by Rasmus Anthin on 2025-09-14.
//

#pragma once
#include "defines.h"
#include "Buffer.h"
#include "Source.h"
#include "Listener.h"
#include "Backend_NoAudio.h"
#include "Backend_MacOS_CoreAudio.h"
#include "Backend_Linux_ALSA.h"
#include "Backend_Windows_WASAPI.h"
#include "System.h"
#include "PositionalAudio.h"
#include <memory>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <unordered_map>
#include <cmath>


namespace applaudio
{
  
  // //////////////

  class AudioEngine
  {
    std::unique_ptr<IBackend> m_backend;
    
    int m_frame_count = 0;
    int m_output_channels = 0;
    int m_output_sample_rate = 0;
    int m_bits = 32;
    
    std::unique_ptr<a3d::PositionalAudio> scene_3d;
    
    Listener listener;
    
    std::unordered_map<unsigned int, Source> m_sources;
    std::unordered_map<unsigned int, Buffer> m_buffers;
    unsigned int m_next_source_id = 1;
    unsigned int m_next_buffer_id = 1;
    
    std::atomic<bool> m_running { false };
    std::thread m_thread;
    
    mutable std::mutex m_state_mutex;
        
    void enter_audio_thread_loop()
    {
      auto next_frame_time = std::chrono::high_resolution_clock::now();
      
      while (m_running)
      {
        {
          std::scoped_lock lock(m_state_mutex);
          update_3d_scene(); // Generate meta data for 3d audio.
          mix();  // Mix the next chunk.
        }
        
        // advance time by the chunk duration
        next_frame_time += std::chrono::microseconds(
          static_cast<long long>(1e6 * m_frame_count / m_output_sample_rate));
        
        std::this_thread::sleep_until(next_frame_time);
      }
    }
    
    short convert_sample_float_to_short(float sample_32f_in) const
    {
      return static_cast<short>(std::clamp(sample_32f_in * APL_SHORT_LIMIT_F, APL_SHORT_MIN_F, APL_SHORT_MAX_F));
    }
    
    void add_sample(APL_SAMPLE_TYPE& sample_sum, float st_new_sample, const Source& src)
    {
      //auto sample = (1.0 - frac) * s1 + frac * s2;
            
      //here: mix_buffer[f * m_output_channels + c] =
      //here: std::clamp(mix_buffer[f * m_output_channels + c] + sample, APL_SAMPLE_MIN, APL_SAMPLE_MAX);

#ifdef APL_32
      sample_sum += st_new_sample * src.gain;
      sample_sum = std::clamp(sample_sum, APL_SAMPLE_MIN, APL_SAMPLE_MAX);
#else
      auto f_new_sample = st_new_sample * 1/APL_SHORT_LIMIT_F * src.gain;
      auto l_sample_sum = static_cast<long>(sample_sum);
      auto l_new_sample = static_cast<long>(f_new_sample * APL_SHORT_LIMIT_F);
      
      l_sample_sum += l_new_sample;
      l_sample_sum = std::clamp(l_sample_sum, APL_SHORT_MIN_L, APL_SHORT_MAX_L);
      sample_sum = static_cast<short>(l_sample_sum);
#endif
    }
    
    void convert_8u(std::vector<APL_SAMPLE_TYPE>& buf_trg, const std::vector<unsigned char>& buf_src) const
    {
      size_t len = buf_src.size();
      buf_trg.resize(len);
      auto scale = 1/128.f;
#ifdef APL_32
      for (size_t s = 0; s < len; ++s)
        buf_trg[s] = (buf_src[s] - 128)*scale;
#else
      for (size_t s = 0; s < len; ++s)
        buf_trg[s] = convert_sample_float_to_short((buf_src[s] - 128)*scale);
#endif
    }
    
    void convert_8s(std::vector<APL_SAMPLE_TYPE>& buf_trg, const std::vector<char>& buf_src) const
    {
      size_t len = buf_src.size();
      buf_trg.resize(len);
      auto scale = 1/128.f;
#ifdef APL_32
      for (size_t s = 0; s < len; ++s)
        buf_trg[s] = buf_src[s]*scale;
#else
      for (size_t s = 0; s < len; ++s)
        buf_trg[s] = convert_sample_float_to_short(buf_src[s]*scale);
#endif
    }
    
    void convert_16s(std::vector<APL_SAMPLE_TYPE>& buf_trg, const std::vector<short>& buf_src) const
    {
#ifdef APL_32
      size_t len = buf_src.size();
      buf_trg.resize(len);
      auto scale = 1/APL_SHORT_LIMIT_F;
      for (size_t s = 0; s < len; ++s)
        buf_trg[s] = buf_src[s]*scale;
#else
      buf_trg = buf_src;
#endif
    }
    
    void convert_32f(std::vector<APL_SAMPLE_TYPE>& buf_trg, const std::vector<float>& buf_src) const
    {
#ifdef APL_32
      buf_trg = buf_src;
#else
      size_t len = buf_src.size();
      buf_trg.resize(len);
      for (size_t s = 0; s < len; ++s)
        buf_trg[s] = convert_sample_float_to_short(buf_src[s]);
#endif
    }
    
    inline void mix_flat(Source& src, const Buffer& buf,
                         double& pos, double pitch_adjusted_step,
                         std::vector<APL_SAMPLE_TYPE>& mix_buffer)
    {
      float pan_left = 1.f;
      float pan_right = 1.f;
      if (src.pan.has_value())
      {
        pan_right = src.pan.value();
        pan_left = 1.f - pan_right;
      }
    
      size_t buf_size = buf.data.size();
      for (int f = 0; f < m_frame_count; ++f)
      {
        size_t idx = static_cast<size_t>(pos) * buf.channels;
        if (idx + buf.channels > buf_size)
        {
          if (src.looping)
          {
            pos = 0.0; // wrap
            idx = 0;
          }
          else
          {
            src.playing = false;
            break;
          }
        }
        
        // Linear interpolation between samples
        //   because pos is not an integer.
        
        size_t idx_curr = idx;
        size_t idx_next = idx_curr + buf.channels;
        double frac = pos - std::floor(pos);
        
        if (buf.channels == m_output_channels)
        {
          // 1→1 or 2→2 (direct copy with interpolation)
          bool do_pan = buf.channels == 2 && src.pan.has_value();
          for (int c = 0; c < buf.channels; ++c)
          {
            auto s1 = buf.data[idx_curr + c];
            auto s2 = (idx_next + c < buf_size) ? buf.data[idx_next + c] : s1;
            auto sample = static_cast<float>((1.0 - frac) * s1 + frac * s2);
            if (do_pan && c == 0)
              sample *= pan_left;
            if (do_pan && c == 1)
              sample *= pan_right;
            
            add_sample(mix_buffer[f * m_output_channels + c], sample, src);
          }
        }
        else if (buf.channels == 1 && m_output_channels == 2)
        {
          // Mono → Stereo
          auto s1 = buf.data[idx_curr];
          auto s2 = (idx_next < buf_size) ? buf.data[idx_next] : s1;
          auto sample = static_cast<float>((1.0 - frac) * s1 + frac * s2);
          
          for (int c = 0; c < 2; ++c)
            add_sample(mix_buffer[f * 2 + c], sample, src);
        }
        else if (buf.channels == 2 && m_output_channels == 1)
        {
          // Stereo → Mono (average channels, then interpolate between frames)
          auto l1 = buf.data[idx_curr + 0];
          auto r1 = buf.data[idx_curr + 1];
          auto l2 = (idx_next + 0 < buf_size) ? buf.data[idx_next + 0] : l1;
          auto r2 = (idx_next + 1 < buf_size) ? buf.data[idx_next + 1] : r1;
          
          auto s1 = (l1 + r1) / 2.0;
          auto s2 = (l2 + r2) / 2.0;
          
          if (buf.channels == 2 && src.pan.has_value())
          {
            s1 *= pan_left;
            s2 *= pan_right;
          }
          
          auto mono_sample = static_cast<float>((1.0 - frac) * s1 + frac * s2);
          
          add_sample(mix_buffer[f], mono_sample, src);
        }
        
        pos += pitch_adjusted_step; // pitch = playback speed
      }
    }
    
    
    inline void mix_3d(const Listener& listener, Source& src, const Buffer& buf,
                       double& pos, double pitch_adjusted_step,
                       std::vector<APL_SAMPLE_TYPE>& mix_buffer)
    {
      const int src_ch = buf.channels;
      const int dst_ch = m_output_channels;
      const size_t buf_size = buf.data.size();
      const bool do_pan = buf.channels == 2 && src.pan.has_value();
      
      float pan_left = 1.f;
      float pan_right = 1.f;
      if (src.pan.has_value())
      {
        pan_right = src.pan.value();
        pan_left = 1.f - pan_right;
      }
      
      // Dynamic temp to avoid overflow for >2ch sources.
      std::vector<float> src_samples(src_ch);
      
      for (int f = 0; f < m_frame_count; ++f)
      {
        size_t i0 = static_cast<size_t>(pos) * src_ch;
        size_t i1 = i0 + src_ch;
        
        // ---- EARLY BOUNDS CHECK (like mix_flat) ----
        if (i0 + src_ch > buf_size)
        {
          if (src.looping)
          {
            pos = 0.0;    // Wrap and restart this frame.
            i0 = 0; i1 = src_ch;
          }
          else
          {
            src.playing = false;
            break;
          }
        }
        
        const double frac = pos - std::floor(pos);
        
        // Interpolate per source channel.
        for (int ch_s = 0; ch_s < src_ch; ++ch_s)
        {
          const float s1 = buf.data[i0 + ch_s];
          const float s2 = (i1 + ch_s < buf_size) ? buf.data[i1 + ch_s] : s1;
          float sample = static_cast<float>((1.0 - frac) * s1 + frac * s2);
          if (do_pan && ch_s == 0) sample *= pan_left;
          if (do_pan && ch_s == 1) sample *= pan_right;
          src_samples[ch_s] = sample;
        }
        
        float doppler_shift = 1.f;
        
        // Project each source channel to each listener channel.
        for (int ch_l = 0; ch_l < dst_ch; ++ch_l)
        {
          float sum = 0.f;
          for (int ch_s = 0; ch_s < src_ch; ++ch_s)
          {
            const auto* state_s = src.object_3d.get_channel_state(ch_s);
            if (!state_s)
              continue;  // guard null
            if (ch_l >= static_cast<int>(state_s->listener_ch_params.size()))
              continue;
            
            // Apply doppler shift scaling and attenuation gain.
            const auto& p = state_s->listener_ch_params[ch_l];
            if (std::abs(doppler_shift - 1.f) < std::abs(p.doppler_shift - 1.f))
              doppler_shift = p.doppler_shift;
            
            sum += src_samples[ch_s] * p.gain;
          }
          
          add_sample(mix_buffer[f * dst_ch + ch_l], sum, src);
        }
        
        pos += pitch_adjusted_step * doppler_shift;
      }
    }
    
    void mix()
    {
      std::vector<APL_SAMPLE_TYPE> mix_buffer(m_frame_count * m_output_channels, 0);
      
      for (auto& [id, src] : m_sources)
      {
        if (!src.playing)
          continue;
        
        if (src.buffer_id == 0) // invalid source id
          continue;
        
        // Safety check: make sure the buffer actually exists
        auto buf_it = m_buffers.find(src.buffer_id);
        if (buf_it == m_buffers.end())
        {
          // Buffer was destroyed but source still references it
          src.buffer_id = 0; // Auto-detach invalid buffer
          src.playing = false;
          continue;
        }
        
        const auto& buf = buf_it->second;
        double pos = src.play_pos;
        
        // Calculate pitch adjustment for sample rate conversion.
        double sample_rate_ratio = static_cast<double>(buf.sample_rate) / m_output_sample_rate;
        double pitch_adjusted_step = src.pitch * sample_rate_ratio;
        
        if (src.object_3d.using_3d_audio())
          mix_3d(listener, src, buf,
                 pos, pitch_adjusted_step,
                 mix_buffer);
        else
          mix_flat(src, buf,
                   pos, pitch_adjusted_step,
                   mix_buffer);
        
        src.play_pos = pos;
      }
      
      m_backend->write_samples(mix_buffer.data(), m_frame_count);
    }
    
    bool update_3d_scene()
    {
      if (scene_3d != nullptr)
      {
        scene_3d->update_scene(listener, m_sources);
        return true;
      }
      return false;
    }
    
  public:
    AudioEngine(bool enable_audio = true)
    {
      if (!enable_audio)
        m_backend = std::make_unique<Backend_NoAudio>();
      else
      {
#if defined(_WIN32)
        m_backend = std::make_unique<Backend_Windows_WASAPI>();
#elif defined(__APPLE__)
        m_backend = std::make_unique<Backend_MacOS_CoreAudio>();
#elif defined(__linux__)
        if (apl_sys::is_wsl())
        {
          std::cout << "WARNING: No audio support for Ubuntu WSL!\n";
          std::cout << "  Reverting to Backend_NoAudio.\n";
          m_backend = std::make_unique<Backend_NoAudio>();
        }
        else
          m_backend = std::make_unique<Backend_Linux_ALSA>();
#endif
      }
    }

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;
    AudioEngine(AudioEngine&&) = delete;
    AudioEngine& operator=(AudioEngine&&) = delete;

    int output_sample_rate() const
    {
      return m_output_sample_rate;
    }
    
    int num_output_channels() const
    {
      return m_output_channels;
    }

    int num_bits_per_sample() const
    {
      return m_bits;
    }

    
    bool startup(int request_out_sample_rate = 48'000, 
                 int request_out_num_channels = 2, 
                 bool request_exclusive_mode_if_supported = false, 
                 bool verbose = false)
    {
      std::scoped_lock lock(m_state_mutex);
      m_output_sample_rate = request_out_sample_rate;
      m_output_channels = request_out_num_channels;

      if (!m_backend)
      {
        std::cerr << "AudioEngine: Failed to assign backend!\n";
        return false;
      }
      
      if (!m_backend->startup(m_output_sample_rate, m_output_channels, request_exclusive_mode_if_supported, verbose))
      {
        std::cerr << "AudioEngine: Failed to initialize backend!\n";
        return false;
      }

      m_output_sample_rate = m_backend->get_sample_rate();
      m_output_channels = m_backend->get_num_channels();
      m_bits = m_backend->get_bit_format();
      
      // Query the backend for its preferred frame count
      m_frame_count = m_backend->get_buffer_size_frames();
      
      if (m_frame_count <= 0)
      {
        // fallback if the backend didn't report a valid size
        m_frame_count = 512;
      }
      
      if (verbose)
      {
        std::cout << "AudioEngine initialized: "
          << "Fs_out = " << m_output_sample_rate << " Hz, "
          << "Bit format out: " << m_bits << " bits, "
          << m_output_channels << " output channels, "
          << m_frame_count << " frames per mix\n";
      }
      
      m_running = true;
      m_thread = std::thread(&AudioEngine::enter_audio_thread_loop, this);
      
      return true;
    }

    
    void shutdown()
    {
      std::scoped_lock lock(m_state_mutex);
      m_running = false;
      if (m_thread.joinable())
        m_thread.join();
      
      if (m_backend != nullptr)
        m_backend->shutdown();
    }

    unsigned int create_source()
    {
      std::scoped_lock lock(m_state_mutex);
      unsigned int id = m_next_source_id++;
      m_sources[id] = Source {};
      return id;
    }
    
    // 1. Source stops playing immediately.
    // 2. Source is permanently removed from the system.
    // 3. Source ID becomes invalid.
    // 4. Any attached buffer is automatically detached.
    // 5. Resources are freed.
    void destroy_source(unsigned int src_id)
    {
      std::scoped_lock lock(m_state_mutex);
      m_sources.erase(src_id);
    }
    
    unsigned int create_buffer()
    {
      std::scoped_lock lock(m_state_mutex);
      unsigned int id = m_next_buffer_id++;
      m_buffers[id] = Buffer {};
      return id;
    }
    
    void destroy_buffer(unsigned int buf_id)
    {
      std::scoped_lock lock(m_state_mutex);
      m_buffers.erase(buf_id);
    }
    
    bool set_buffer_data_8u(unsigned int buf_id, const std::vector<unsigned char>& data,
                            int channels, int sample_rate)
    {
      std::scoped_lock lock(m_state_mutex);
      auto buf_it = m_buffers.find(buf_id);
      if (buf_it != m_buffers.end())
      {
        convert_8u(buf_it->second.data, data);
        buf_it->second.channels = channels;
        buf_it->second.sample_rate = sample_rate;
        return true;
      }
      return false;
    }
    
    bool set_buffer_data_8s(unsigned int buf_id, const std::vector<char>& data,
                            int channels, int sample_rate)
    {
      std::scoped_lock lock(m_state_mutex);
      auto buf_it = m_buffers.find(buf_id);
      if (buf_it != m_buffers.end())
      {
        convert_8s(buf_it->second.data, data);
        buf_it->second.channels = channels;
        buf_it->second.sample_rate = sample_rate;
        return true;
      }
      return false;
    }
    
    bool set_buffer_data_16s(unsigned int buf_id, const std::vector<short>& data,
                             int channels, int sample_rate)
    {
      std::scoped_lock lock(m_state_mutex);
      auto buf_it = m_buffers.find(buf_id);
      if (buf_it != m_buffers.end())
      {
        convert_16s(buf_it->second.data, data);
        buf_it->second.channels = channels;
        buf_it->second.sample_rate = sample_rate;
        return true;
      }
      return false;
    }
    
    bool set_buffer_data_32f(unsigned int buf_id, const std::vector<float>& data,
                             int channels, int sample_rate)
    {
      std::scoped_lock lock(m_state_mutex);
      auto buf_it = m_buffers.find(buf_id);
      if (buf_it != m_buffers.end())
      {
        convert_32f(buf_it->second.data, data);
        buf_it->second.channels = channels;
        buf_it->second.sample_rate = sample_rate;
        return true;
      }
      return false;
    }
    
    bool attach_buffer_to_source(unsigned int src_id, unsigned int buf_id)
    {
      std::scoped_lock lock(m_state_mutex);
      auto src_it = m_sources.find(src_id);
      if (src_it != m_sources.end())
      {
        Source& src = src_it->second;
        src.buffer_id = buf_id;
        src.playing = false; // Stop playback
        src.play_pos = 0.0; // Reset position
        src.paused = false;
        return true;
      }
      return false;
    }
    
    // 1. Detaches buffer from source.
    // 2. Stops playback.
    // 3. Resets the buffer position.
    bool detach_buffer_from_source(unsigned int src_id)
    {
      std::scoped_lock lock(m_state_mutex);
      auto src_it = m_sources.find(src_id);
      if (src_it != m_sources.end())
      {
        Source& src = src_it->second;
        src.buffer_id = 0; // Detach by setting buffer_id to 0
        src.playing = false; // Stop playback
        src.play_pos = 0.0; // Reset position
        src.paused = false;
        return true;
      }
      return false;
    }
    
    // Play the source
    void play_source(unsigned int src_id)
    {
      std::scoped_lock lock(m_state_mutex);
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        Source& src = it->second;
        src.playing = true;
        if (!src.paused)
          src.play_pos = 0.0;
        src.paused = false;
      }
    }
    
    // Check if a source is playing
    std::optional<bool> is_source_playing(unsigned int src_id) const
    {
      std::scoped_lock lock(m_state_mutex);
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
        return it->second.playing;
      return std::nullopt;
    }
    
    // Pause the source
    void pause_source(unsigned int src_id)
    {
      std::scoped_lock lock(m_state_mutex);
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        Source& src = it->second;
        src.playing = false;
        src.paused = true;
      }
    }
    
    std::optional<bool> is_source_paused(unsigned int src_id) const
    {
      std::scoped_lock lock(m_state_mutex);
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
        return it->second.paused;
      return std::nullopt;
    }
    
    // Stop the source
    void stop_source(unsigned int src_id)
    {
      std::scoped_lock lock(m_state_mutex);
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        Source& src = it->second;
        src.playing = false;
        src.paused = false;
        src.play_pos = 0.0;
      }
    }
    
    // Set gain
    void set_source_gain(unsigned int src_id, float gain)
    {
      std::scoped_lock lock(m_state_mutex);
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
        it->second.gain = gain;
    }
    
    std::optional<float> get_source_gain(unsigned int src_id) const
    {
      std::scoped_lock lock(m_state_mutex);
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
        return it->second.gain;
      return std::nullopt;
    }
    
    void set_source_volume_dB(unsigned int src_id, float vol_dB)
    {
      std::scoped_lock lock(m_state_mutex);
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        float gain = std::pow(10.f, vol_dB/20.f);
        it->second.gain = gain;
      }
    }
    
    std::optional<float> get_source_volume_dB(unsigned int src_id) const
    {
      std::scoped_lock lock(m_state_mutex);
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        float vol_dB = 20.f * std::log10(it->second.gain);
        return vol_dB;
      }
      return std::nullopt;
    }
    
    // Perceptually linear mapping: 0 -> min_dB dB, 1 -> 0 dB.
    void set_source_volume_slider(unsigned int src_id, float vol01, float min_dB = -60.f, std::optional<float> nl_taper = std::nullopt)
    {
      float t = vol01;
      if (nl_taper.has_value()) // Non-linear tapering.
        t = std::pow(vol01, std::clamp(nl_taper.value(), 0.f, 1.f)); // nl_taper < 1 : Brightens mid-range.
      float vol_dB = min_dB * (1.f - t);
      set_source_volume_dB(src_id, vol_dB);
    }
    
    // Perceptually linear mapping: 0 -> min_dB dB, 1 -> 0 dB.
    std::optional<float> get_source_volume_slider(unsigned int src_id, float min_dB = -60.f, std::optional<float> nl_taper = std::nullopt) const
    {
      auto vol_dB = get_source_volume_dB(src_id);
      if (!vol_dB.has_value())
        return std::nullopt;
      float t = 1.f - vol_dB.value()/min_dB;
      float vol01 = t;
      if (nl_taper.has_value())
        vol01 = std::pow(t, 1.0f / std::clamp(nl_taper.value(), 0.f, 1.f));
      return vol01;
    }
    
    // Set pitch
    void set_source_pitch(unsigned int src_id, float pitch)
    {
      std::scoped_lock lock(m_state_mutex);
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
        it->second.pitch = pitch;
    }
    
    std::optional<float> get_source_pitch(unsigned int src_id) const
    {
      std::scoped_lock lock(m_state_mutex);
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
        return it->second.pitch;
      return std::nullopt;
    }
    
    // Set looping
    void set_source_looping(unsigned int src_id, bool loop)
    {
      std::scoped_lock lock(m_state_mutex);
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
        it->second.looping = loop;
    }
    
    std::optional<bool> get_source_looping(unsigned int src_id) const
    {
      std::scoped_lock lock(m_state_mutex);
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
        return it->second.looping;
      return std::nullopt;
    }
    
    void set_source_panning(unsigned int src_id, std::optional<float> pan = std::nullopt)
    {
      std::scoped_lock lock(m_state_mutex);
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        auto& src = it->second;
        src.pan = std::nullopt; // Could be tad faster than an else below.
        if (pan.has_value())
          src.pan = std::clamp(pan.value(), 0.f, 1.f);
      }
    }
    
    std::optional<float> get_source_panning(unsigned int src_id) const
    {
      std::scoped_lock lock(m_state_mutex);
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
        return it->second.pan;
      return std::nullopt;
    }
    
    void print_backend_name() const
    {
      if (m_backend != nullptr)
        std::cout << m_backend->backend_name() << std::endl;
      else
        std::cout << "Unknown backend!" << std::endl;
    }
    
    // ----- Positional Audio Functions -----
    
    void init_3d_scene()
    {
      std::scoped_lock lock(m_state_mutex);
      if (scene_3d != nullptr)
        return;
      scene_3d = std::make_unique<a3d::PositionalAudio>();
      listener.object_3d.set_num_channels(m_output_channels);
    }
    
    void enable_source_3d_audio(unsigned int src_id, bool enable)
    {
      std::scoped_lock lock(m_state_mutex);
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
        it->second.object_3d.enable_3d_audio(enable);
    }
    
    bool set_source_3d_state_channel(unsigned int src_id, int channel, const la::Mtx3& rot_mtx,
                                     const la::Vec3& pos_world, const la::Vec3& vel_world)
    {
      std::scoped_lock lock(m_state_mutex);
      if (scene_3d == nullptr)
        return false;
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        auto& src = it->second;
        auto buf_it = m_buffers.find(src.buffer_id);
        if (buf_it == m_buffers.end())
          return false;
        if (src.object_3d.num_channels() != buf_it->second.channels)
          src.object_3d.set_num_channels(buf_it->second.channels);
        if (channel < 0 || channel >= src.object_3d.num_channels())
          return false;
        src.object_3d.set_channel_state(channel, rot_mtx, pos_world, vel_world);
        return true;
      }
      return false;
    }
    
    bool get_source_3d_state_channel(unsigned int src_id, int channel, la::Mtx3& rot_mtx,
                                     la::Vec3& pos_world, la::Vec3& vel_world) const
    {
      std::scoped_lock lock(m_state_mutex);
      if (scene_3d == nullptr)
        return false;
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        auto& src = it->second;
        auto buf_it = m_buffers.find(src.buffer_id);
        if (buf_it == m_buffers.end())
          return false;
        if (channel < 0 || channel >= src.object_3d.num_channels())
          return false;
        src.object_3d.get_channel_state(channel, rot_mtx, pos_world, vel_world);
        return true;
      }
      return false;
    }
    
    // W column of trf should be center of mass of listener.
    bool set_source_3d_state(unsigned int src_id, const la::Mtx4& trf,
                             const la::Vec3& vel_world, const la::Vec3& ang_vel_local,
                             const std::vector<la::Vec3>& channel_pos_offsets_local)
    {
      std::scoped_lock lock(m_state_mutex);
      if (scene_3d == nullptr)
        return false;
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        auto& src = it->second;
        auto buf_it = m_buffers.find(src.buffer_id);
        if (buf_it == m_buffers.end())
          return false;
        if (src.object_3d.num_channels() != buf_it->second.channels)
          src.object_3d.set_num_channels(buf_it->second.channels);
        if (static_cast<int>(channel_pos_offsets_local.size()) != src.object_3d.num_channels())
        {
          std::cerr << "ERROR in set_source_3d_state() : number of channel_pos_offsets_local positions do not match the number of channels registered in the source." << std::endl;
          return false;
        }
        for (int ch = 0; ch < src.object_3d.num_channels(); ++ch)
        {
          const auto& local_pos_ch = channel_pos_offsets_local[ch];
          auto world_pos_ch = trf.transform_pos(local_pos_ch);
          auto world_ang_vel = trf.transform_vec(ang_vel_local);
          la::Vec3 world_pos_cm;
          trf.get_column_vec(la::W, world_pos_cm);
          auto world_vel_ch = vel_world + la::cross(world_ang_vel, world_pos_ch - world_pos_cm);
          src.object_3d.set_channel_state(ch, trf.get_rot_matrix(), world_pos_ch, world_vel_ch);
        }
        return true;
      }
      return false;
    }
    
    bool set_listener_3d_state_channel(int channel, const la::Mtx3& rot_mtx,
                                       const la::Vec3& pos_world, const la::Vec3& vel_world)
    {
      std::scoped_lock lock(m_state_mutex);
      if (scene_3d == nullptr)
        return false;
      if (channel < 0 || channel >= listener.object_3d.num_channels())
        return false;
      listener.object_3d.set_channel_state(channel, rot_mtx, pos_world, vel_world);
      return true;
    }
    
    bool get_listener_3d_state_channel(int channel, la::Mtx3& rot_mtx,
                                       la::Vec3& pos_world, la::Vec3& vel_world) const
    {
      std::scoped_lock lock(m_state_mutex);
      if (scene_3d == nullptr)
        return false;
      if (channel < 0 || channel >= listener.object_3d.num_channels())
        return false;
      listener.object_3d.get_channel_state(channel, rot_mtx, pos_world, vel_world);
      return true;
    }
    
    // W column of trf should be center of mass of listener.
    bool set_listener_3d_state(const la::Mtx4& trf,
                               const la::Vec3& vel_world, const la::Vec3& ang_vel_local,
                               const std::vector<la::Vec3>& channel_pos_offsets_local)
    {
      std::scoped_lock lock(m_state_mutex);
      if (scene_3d == nullptr)
        return false;
      if (static_cast<int>(channel_pos_offsets_local.size()) != listener.object_3d.num_channels())
      {
        std::cerr << "ERROR in set_listener_3d_state() : number of channel_pos_offsets_local positions do not match the number of channels registered in the listener." << std::endl;
        return false;
      }
      for (int ch = 0; ch < listener.object_3d.num_channels(); ++ch)
      {
        const auto& local_pos_ch = channel_pos_offsets_local[ch];
        auto world_pos_ch = trf.transform_pos(local_pos_ch);
        auto world_ang_vel = trf.transform_vec(ang_vel_local);
        la::Vec3 world_pos_cm;
        trf.get_column_vec(la::W, world_pos_cm);
        auto world_vel_ch = vel_world + la::cross(world_ang_vel, world_pos_ch - world_pos_cm);
        listener.object_3d.set_channel_state(ch, trf.get_rot_matrix(), world_pos_ch, world_vel_ch);
      }
      return true;
    }
    
    bool set_source_speed_of_sound(unsigned int src_id, float speed_of_sound)
    {
      std::scoped_lock lock(m_state_mutex);
      if (scene_3d == nullptr)
        return false;
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        auto& src = it->second;
        src.speed_of_sound = speed_of_sound;
        return true;
      }
      return false;
    }
    
    std::optional<float> get_source_speed_of_sound(unsigned int src_id) const
    {
      std::scoped_lock lock(m_state_mutex);
      if (scene_3d == nullptr)
        return std::nullopt;
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        const auto& src = it->second;
        return src.speed_of_sound;
      }
      return std::nullopt;
    }
    
    bool set_source_attenuation_min_distance(unsigned int src_id, float min_dist)
    {
      std::scoped_lock lock(m_state_mutex);
      if (scene_3d == nullptr)
        return false;
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        auto& src = it->second;
        return scene_3d->set_attenuation_min_distance(src, min_dist);
      }
      return false;
    }
    
    std::optional<float> get_source_attenuation_min_distance(unsigned int src_id) const
    {
      std::scoped_lock lock(m_state_mutex);
      if (scene_3d == nullptr)
        return std::nullopt;
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        auto& src = it->second;
        return scene_3d->get_attenuation_min_distance(src);
      }
      return std::nullopt;
    }
    
    bool set_source_attenuation_max_distance(unsigned int src_id, float max_dist)
    {
      std::scoped_lock lock(m_state_mutex);
      if (scene_3d == nullptr)
        return false;
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        auto& src = it->second;
        return scene_3d->set_attenuation_max_distance(src, max_dist);
      }
      return false;
    }
    
    std::optional<float> get_source_attenuation_max_distance(unsigned int src_id) const
    {
      std::scoped_lock lock(m_state_mutex);
      if (scene_3d == nullptr)
        return std::nullopt;
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        auto& src = it->second;
        return scene_3d->get_attenuation_max_distance(src);
      }
      return std::nullopt;
    }
    
    bool set_source_attenuation_constant_falloff(unsigned int src_id, float const_falloff)
    {
      std::scoped_lock lock(m_state_mutex);
      if (scene_3d == nullptr)
        return false;
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        auto& src = it->second;
        return scene_3d->set_attenuation_constant_falloff(src, const_falloff);
      }
      return false;
    }
    
    std::optional<float> get_source_attenuation_constant_falloff(unsigned int src_id) const
    {
      std::scoped_lock lock(m_state_mutex);
      if (scene_3d == nullptr)
        return std::nullopt;
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        auto& src = it->second;
        return scene_3d->get_attenuation_constant_falloff(src);
      }
      return std::nullopt;
    }
    
    bool set_source_attenuation_linear_falloff(unsigned int src_id, float lin_falloff)
    {
      std::scoped_lock lock(m_state_mutex);
      if (scene_3d == nullptr)
        return false;
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        auto& src = it->second;
        return scene_3d->set_attenuation_linear_falloff(src, lin_falloff);
      }
      return false;
    }
    
    std::optional<float> get_source_attenuation_linear_falloff(unsigned int src_id) const
    {
      std::scoped_lock lock(m_state_mutex);
      if (scene_3d == nullptr)
        return std::nullopt;
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        auto& src = it->second;
        return scene_3d->get_attenuation_linear_falloff(src);
      }
      return std::nullopt;
    }
    
    bool set_source_attenuation_quadratic_falloff(unsigned int src_id, float sq_falloff)
    {
      std::scoped_lock lock(m_state_mutex);
      if (scene_3d == nullptr)
        return false;
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        auto& src = it->second;
        return scene_3d->set_attenuation_quadratic_falloff(src, sq_falloff);
      }
      return false;
    }
    
    std::optional<float> get_source_attenuation_quadratic_falloff(unsigned int src_id) const
    {
      std::scoped_lock lock(m_state_mutex);
      if (scene_3d == nullptr)
        return std::nullopt;
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        auto& src = it->second;
        return scene_3d->get_attenuation_quadratic_falloff(src);
      }
      return std::nullopt;
    }
    
    // directivity_alpha = 0 : Omni.
    // directivity_alpha = 1 : Fully Directional.
    // [0, 1].
    bool set_source_directivity_alpha(unsigned int src_id, float directivity_alpha)
    {
      std::scoped_lock lock(m_state_mutex);
      if (scene_3d == nullptr)
        return false;
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        auto& src = it->second;
        src.directivity_alpha = std::clamp(directivity_alpha, 0.f, 1.f);
        return true;
      }
      return false;
    }
    
    // directivity_alpha = 0 : Omni.
    // directivity_alpha = 1 : Fully Directional.
    // [0, 1].
    std::optional<float> get_source_directivity_alpha(unsigned int src_id) const
    {
      std::scoped_lock lock(m_state_mutex);
      if (scene_3d == nullptr)
        return std::nullopt;
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        auto& src = it->second;
        return src.directivity_alpha;
      }
      return std::nullopt;
    }
    
    // [1, 8]. 8 = sharpest.
    bool set_source_directivity_sharpness(unsigned int src_id, float directivity_sharpness)
    {
      std::scoped_lock lock(m_state_mutex);
      if (scene_3d == nullptr)
        return false;
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        auto& src = it->second;
        src.directivity_sharpness = std::clamp(directivity_sharpness, 1.f, 8.f);
        return true;
      }
      return false;
    }
    
    // [1, 8]. 8 = sharpest.
    std::optional<float> get_source_directivity_sharpness(unsigned int src_id) const
    {
      std::scoped_lock lock(m_state_mutex);
      if (scene_3d == nullptr)
        return std::nullopt;
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        auto& src = it->second;
        return src.directivity_sharpness;
      }
      return std::nullopt;
    }
    
    // Cardioid, SuperCardioid, HalfRectifiedDipole, Dipole.
    bool set_source_directivity_type(unsigned int src_id, DirectivityType directivity_type)
    {
      std::scoped_lock lock(m_state_mutex);
      if (scene_3d == nullptr)
        return false;
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        auto& src = it->second;
        src.directivity_type = directivity_type;
        return true;
      }
      return false;
    }
    
    // Cardioid, SuperCardioid, HalfRectifiedDipole, Dipole.
    std::optional<DirectivityType> get_source_directivity_type(unsigned int src_id) const
    {
      std::scoped_lock lock(m_state_mutex);
      if (scene_3d == nullptr)
        return std::nullopt;
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        auto& src = it->second;
        return src.directivity_type;
      }
      return std::nullopt;
    }
    
    // [0.f, 1.f]. 0 = Silence, 1 = No Attenuation.
    bool set_source_rear_attenuation(unsigned int src_id, float rear_attenuation)
    {
      std::scoped_lock lock(m_state_mutex);
      if (scene_3d == nullptr)
        return false;
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        auto& src = it->second;
        src.rear_attenuation = std::clamp(rear_attenuation, 0.f, 1.f);
        return true;
      }
      return false;
    }
    
    // [0.f, 1.f]. 0 = Silence, 1 = No Attenuation.
    std::optional<float> get_source_rear_attenuation(unsigned int src_id) const
    {
      std::scoped_lock lock(m_state_mutex);
      if (scene_3d == nullptr)
        return std::nullopt;
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        auto& src = it->second;
        return src.rear_attenuation;
      }
      return std::nullopt;
    }
    
    // [0.f, 1.f]. 0 = Silence, 1 = No Attenuation.
    bool set_listener_rear_attenuation(float rear_attenuation)
    {
      std::scoped_lock lock(m_state_mutex);
      if (scene_3d == nullptr)
        return false;
      listener.rear_attenuation = std::clamp(rear_attenuation, 0.f, 1.f);
      return true;
    }
    
    // [0.f, 1.f]. 0 = Silence, 1 = No Attenuation.
    std::optional<float> get_listener_rear_attenuation() const
    {
      std::scoped_lock lock(m_state_mutex);
      if (scene_3d == nullptr)
        return std::nullopt;
      return listener.rear_attenuation;
    }
    
    bool set_source_coordsys_convention(unsigned int src_id, a3d::CoordSysConvention cs_conv)
    {
      std::scoped_lock lock(m_state_mutex);
      if (scene_3d == nullptr)
        return false;
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        auto& src = it->second;
        src.object_3d.set_coordsys_convention(cs_conv);
        return true;
      }
      return false;
    }
    
    std::optional<a3d::CoordSysConvention> get_source_coordsys_convention(unsigned int src_id) const
    {
      std::scoped_lock lock(m_state_mutex);
      if (scene_3d == nullptr)
        return a3d::CoordSysConvention::RH_XLeft_YUp_ZForward;
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        auto& src = it->second;
        return src.object_3d.get_coordsys_convention();
      }
      return std::nullopt;
    }
    
    bool set_listener_coordsys_convention(a3d::CoordSysConvention cs_conv)
    {
      std::scoped_lock lock(m_state_mutex);
      if (scene_3d == nullptr)
        return false;
      listener.object_3d.set_coordsys_convention(cs_conv);
      return true;
    }
    
    std::optional<a3d::CoordSysConvention> get_listener_coordsys_convention() const
    {
      std::scoped_lock lock(m_state_mutex);
      if (scene_3d == nullptr)
        return std::nullopt;
      return listener.object_3d.get_coordsys_convention();
    }
    
  };
  
}
