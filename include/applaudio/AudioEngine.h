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
    
    //mutable std::mutex thread_mutex;
    
    void enter_audio_thread_loop()
    {
      auto next_frame_time = std::chrono::high_resolution_clock::now();
      
      while (m_running)
      {
        //std::scoped_lock lock(thread_mutex);
        update_3d_scene(); // Generate meta data for 3d audio.
        mix();  // Mix the next chunk.
        
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
      sample_sum += st_new_sample * src.volume;
      sample_sum = std::clamp(sample_sum, APL_SAMPLE_MIN, APL_SAMPLE_MAX);
#else
      auto f_new_sample = st_new_sample * 1/APL_SHORT_LIMIT_F * src.volume;
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
          for (int c = 0; c < buf.channels; ++c)
          {
            auto s1 = buf.data[idx_curr + c];
            auto s2 = (idx_next + c < buf_size) ? buf.data[idx_next + c] : s1;
            auto sample = (1.0 - frac) * s1 + frac * s2;
            
            add_sample(mix_buffer[f * m_output_channels + c], sample, src);
          }
        }
        else if (buf.channels == 1 && m_output_channels == 2)
        {
          // Mono → Stereo
          auto s1 = buf.data[idx_curr];
          auto s2 = (idx_next < buf_size) ? buf.data[idx_next] : s1;
          auto sample = (1.0 - frac) * s1 + frac * s2;
          
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
          
          auto mono_sample = (1.0 - frac) * s1 + frac * s2;
          
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
      
      for (int f = 0; f < m_frame_count; ++f)
      {
        size_t i0 = static_cast<size_t>(pos) * src_ch;
        size_t i1 = i0 + src_ch;
        double frac = pos - floor(pos);
        
        // Interpolate samples per source channel
        float src_samples[2] = { 0.f, 0.f };
        for (int ch_s = 0; ch_s < src_ch; ++ch_s)
        {
          float s1 = buf.data[i0 + ch_s];
          float s2 = (i1 + ch_s < buf_size) ? buf.data[i1 + ch_s] : s1;
          src_samples[ch_s] = static_cast<float>((1.0 - frac) * s1 + frac * s2);
        }
        
        // Now project each source channel to each listener channel
        float doppler_shift = 1.f;
        for (int ch_l = 0; ch_l < dst_ch; ++ch_l)
        {
          float sum = 0.f;
          for (int ch_s = 0; ch_s < src_ch; ++ch_s)
          {
            const auto* state_s = src.object_3d.get_state(ch_s);
            if (ch_l >= state_s->listener_ch_params.size())
              continue;
            const auto& p = state_s->listener_ch_params[ch_l];
            
            // Apply doppler shift scaling and attenuation gain
            if (std::abs(doppler_shift - 1.f) < std::abs(p.doppler_shift - 1.f))
              doppler_shift = p.doppler_shift;
            float gain = p.gain;
            sum += src_samples[ch_s] * gain;
          }
          
          // Mix to output
          size_t idx = f * dst_ch + ch_l;
          add_sample(mix_buffer[idx], sum, src);
        }
                        
        pos += pitch_adjusted_step * doppler_shift;
        if (pos * src_ch >= buf_size)
        {
          if (src.looping)
            pos = 0;
          else
          {
            src.playing = false;
            break;
          }
        }
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
          << m_output_channels << " output channels, "
          << m_frame_count << " frames per mix\n";
      }
      
      m_running = true;
      m_thread = std::thread(&AudioEngine::enter_audio_thread_loop, this);
      
      return true;
    }

    
    void shutdown()
    {
      m_running = false;
      if (m_thread.joinable())
        m_thread.join();
      
      if (m_backend != nullptr)
        m_backend->shutdown();
    }

    unsigned int create_source()
    {
      //std::scoped_lock lock(thread_mutex);
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
      //std::scoped_lock lock(thread_mutex);
      m_sources.erase(src_id);
    }
    
    unsigned int create_buffer()
    {
      //std::scoped_lock lock(thread_mutex);
      unsigned int id = m_next_buffer_id++;
      m_buffers[id] = Buffer {};
      return id;
    }
    
    void destroy_buffer(unsigned int buf_id)
    {
      //std::scoped_lock lock(thread_mutex);
      m_buffers.erase(buf_id);
    }
    
    bool set_buffer_data_8u(unsigned int buf_id, const std::vector<unsigned char>& data,
                            int channels, int sample_rate)
    {
      //std::scoped_lock lock(thread_mutex);
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
      //std::scoped_lock lock(thread_mutex);
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
      //std::scoped_lock lock(thread_mutex);
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
      //std::scoped_lock lock(thread_mutex);
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
      //std::scoped_lock lock(thread_mutex);
      auto src_it = m_sources.find(src_id);
      if (src_it != m_sources.end())
      {
        src_it->second.buffer_id = buf_id;
        return true;
      }
      return false;
    }
    
    // 1. Detaches buffer from source.
    // 2. Stops playback.
    // 3. Resets the buffer position.
    bool detach_buffer_from_source(unsigned int src_id)
    {
      //std::scoped_lock lock(thread_mutex);
      auto src_it = m_sources.find(src_id);
      if (src_it != m_sources.end())
      {
        src_it->second.buffer_id = 0; // Detach by setting buffer_id to 0
        src_it->second.playing = false; // Stop playback
        src_it->second.play_pos = 0.0; // Reset position
        return true;
      }
      return false;
    }
    
    // Play the source
    void play_source(unsigned int src_id)
    {
      //std::scoped_lock lock(thread_mutex);
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        Source& src = it->second;
        src.playing = true;
        src.play_pos = 0.0;  // start from beginning
      }
    }
    
    // Check if a source is playing
    bool is_source_playing(unsigned int src_id) const
    {
      //std::scoped_lock lock(thread_mutex);
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
        return it->second.playing;
      return false;
    }
    
    // Pause the source
    void pause_source(unsigned int src_id)
    {
      //std::scoped_lock lock(thread_mutex);
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
        it->second.playing = false;
    }
    
    // Play the source
    void resume_source(unsigned int src_id)
    {
      //std::scoped_lock lock(thread_mutex);
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
        it->second.playing = true;
    }
    
    // Stop the source
    void stop_source(unsigned int src_id)
    {
      //std::scoped_lock lock(thread_mutex);
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        it->second.playing = false;
        it->second.play_pos = 0.0;
      }
    }
    
    // Set volume
    void set_source_volume(unsigned int src_id, float vol)
    {
      //std::scoped_lock lock(thread_mutex);
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
        it->second.volume = vol;
    }
    
    // Set pitch (note: not yet implemented in mixer)
    void set_source_pitch(unsigned int src_id, float pitch)
    {
      //std::scoped_lock lock(thread_mutex);
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
        it->second.pitch = pitch;
    }
    
    // Set looping
    void set_source_looping(unsigned int src_id, bool loop)
    {
      //std::scoped_lock lock(thread_mutex);
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
        it->second.looping = loop;
    }
    
    void print_backend_name() const
    {
      if (m_backend != nullptr)
        std::cout << m_backend->backend_name() << std::endl;
      else
        std::cout << "Unknown backend!" << std::endl;
    }
    
    // ----- Positional Audio Functions -----
    
    void init_3d_scene(a3d::LengthUnit global_length_unit)
    {
      //std::scoped_lock lock(thread_mutex);
      scene_3d = std::make_unique<a3d::PositionalAudio>(global_length_unit);
      listener.object_3d.set_num_channels(m_output_channels);
    }
    
    void enable_source_3d_audio(unsigned int src_id, bool enable)
    {
      //std::scoped_lock lock(thread_mutex);
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
        it->second.object_3d.enable_3d_audio(enable);
    }
    
    bool set_source_pos_vel(unsigned int src_id, const la::Mtx4& new_trf,
                            const la::Vec3& pos_local_left, const la::Vec3& vel_world_left, // mono | stereo left
                            const la::Vec3& pos_local_right = la::Vec3_Zero, const la::Vec3& vel_world_right = la::Vec3_Zero, // stereo right
                            std::optional<a3d::LengthUnit> length_unit = std::nullopt)
    {
      //std::scoped_lock lock(thread_mutex);
      if (scene_3d == nullptr)
        return false;
      auto it = m_sources.find(src_id);
      if (it != m_sources.end() && scene_3d != nullptr)
      {
        auto& src = it->second;
        auto buf_it = m_buffers.find(src.buffer_id);
        if (buf_it == m_buffers.end())
          return false;
        if (src.object_3d.num_channels() != buf_it->second.channels)
          src.object_3d.set_num_channels(buf_it->second.channels);
        scene_3d->update_obj(it->second.object_3d, new_trf, pos_local_left, vel_world_left, pos_local_right, vel_world_right, length_unit);
        return true;
      }
      return false;
    }
    
    bool set_listener_pos_vel(const la::Mtx4& new_trf,
                              const la::Vec3& pos_local_left, const la::Vec3& vel_world_left, // mono | stereo left
                              const la::Vec3& pos_local_right = la::Vec3_Zero, const la::Vec3& vel_world_right = la::Vec3_Zero, // stereo right
                              std::optional<a3d::LengthUnit> length_unit = std::nullopt)
    {
      //std::scoped_lock lock(thread_mutex);
      if (scene_3d == nullptr)
        return false;
      scene_3d->update_obj(listener.object_3d, new_trf, pos_local_left, vel_world_left, pos_local_right, vel_world_right, length_unit);
      return true;
    }
    
    bool set_attenuation_min_distance(float min_dist)
    {
      if (scene_3d == nullptr)
        return false;
      scene_3d->set_attenuation_min_distance(min_dist);
      return true;
    }
    
    bool set_attenuation_max_distance(float max_dist)
    {
      if (scene_3d == nullptr)
        return false;
      scene_3d->set_attenuation_max_distance(max_dist);
      return true;
    }
    
    bool set_attenuation_constant_falloff(float const_falloff)
    {
      if (scene_3d == nullptr)
        return false;
      scene_3d->set_attenuation_constant_falloff(const_falloff);
      return true;
    }
    
    bool set_attenuation_linear_falloff(float lin_falloff)
    {
      if (scene_3d == nullptr)
        return false;
      scene_3d->set_attenuation_linear_falloff(lin_falloff);
      return true;
    }
    
    bool set_attenuation_quadratic_falloff(float sq_falloff)
    {
      if (scene_3d == nullptr)
        return false;
      scene_3d->set_attenuation_quadratic_falloff(sq_falloff);
      return true;
    }
    
  };
  
}
