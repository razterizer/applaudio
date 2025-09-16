//
//  AudioEngine.h
//  applaudio
//
//  Created by Rasmus Anthin on 2025-09-14.
//

#pragma once
#include "MacOS_Internal.h"
#include "Linux_Internal.h"
#include "Win_Internal.h"
#include <memory>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <unordered_map>


namespace applaudio
{

  struct Buffer
  {
    std::vector<short> data;
    int channels = 0;
    int sample_rate = 0;
  };
  
  struct Source
  {
    unsigned int buffer_id = 0;
    bool looping = false;
    float volume = 1.0f;
    float pitch = 1.0f;
    bool playing = false;
    size_t play_pos = 0;
  };
  
  // //////////////

  class AudioEngine
  {
    std::unique_ptr<IApplaudio_Internal> m_device;
    
    int m_frame_count = 0;
    int m_output_channels = 0;
    int m_output_sample_rate = 0;
    
    std::unordered_map<unsigned int, Source> m_sources;
    std::unordered_map<unsigned int, Buffer> m_buffers;
    unsigned int m_next_source_id = 1;
    unsigned int m_next_buffer_id = 1;
    
    std::atomic<bool> m_running{false};
    std::thread m_thread;
    
    void enter_audio_thread_loop()
    {
      auto next_frame_time = std::chrono::high_resolution_clock::now();
      
      while (m_running)
      {
        mix();  // mix the next chunk
        
        // advance time by the chunk duration
        next_frame_time += std::chrono::microseconds(
          static_cast<long long>(1e6 * m_frame_count / m_output_sample_rate));
        
        std::this_thread::sleep_until(next_frame_time);
      }
    }
    
  public:
    AudioEngine()
    {
#if defined(_WIN32)
      m_device = std::make_unique<Win_Internal>();
#elif defined(__APPLE__)
      m_device = std::make_unique<MacOS_Internal>();
#elif defined(__linux__)
      m_device = std::make_unique<Linux_Internal>();
#endif
    }
    
    int num_output_channels()
    {
      return m_output_channels;
    }
    
    bool startup(int out_sample_rate, int out_num_channels = 2, bool verbose = false)
    {
      m_output_sample_rate = out_sample_rate;
      m_output_channels = out_num_channels;
      
      if (m_device == nullptr || !m_device->startup(m_output_sample_rate, m_output_channels))
      {
        std::cerr << "AudioEngine: Failed to initialize device\n";
        return false;
      }
      
      // Query the backend for its preferred frame count
      m_frame_count = m_device->get_buffer_size_frames();
      
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
      
      if (m_device != nullptr)
        m_device->shutdown();
    }

    unsigned int create_source()
    {
      unsigned int id = m_next_source_id++;
      m_sources[id] = Source{};
      return id;
    }
    
    // 1. Source stops playing immediately.
    // 2. Source is permanently removed from the system.
    // 3. Source ID becomes invalid.
    // 4. Any attached buffer is automatically detached.
    // 5. Resources are freed.
    void destroy_source(unsigned int src_id)
    {
      m_sources.erase(src_id);
    }
    
    unsigned int create_buffer()
    {
      unsigned int id = m_next_buffer_id++;
      m_buffers[id] = Buffer{};
      return id;
    }
    
    void destroy_buffer(unsigned int buf_id)
    {
      m_buffers.erase(buf_id);
    }
    
    bool set_buffer_data(unsigned int buf_id, const std::vector<short>& data,
                         int channels, int sample_rate)
    {
      auto buf_it = m_buffers.find(buf_id);
      if (buf_it != m_buffers.end())
      {
        buf_it->second.data = data;
        buf_it->second.channels = channels;
        buf_it->second.sample_rate = sample_rate;
      }
      return false;
    }
    
    bool attach_buffer_to_source(unsigned int src_id, unsigned int buf_id)
    {
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
      auto src_it = m_sources.find(src_id);
      if (src_it != m_sources.end())
      {
        src_it->second.buffer_id = 0; // Detach by setting buffer_id to 0
        src_it->second.playing = false; // Stop playback
        src_it->second.play_pos = 0; // Reset position
        return true;
      }
      return false;
    }
    
    
    void mix()
    {
      std::vector<short> mix_buffer(m_frame_count * m_output_channels, 0);
      
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
        double pos = static_cast<double>(src.play_pos);
        
        // Calculate pitch adjustment for sample rate conversion.
        double sample_rate_ratio = static_cast<double>(buf.sample_rate) / m_output_sample_rate;
        double pitch_adjusted_step = src.pitch * sample_rate_ratio;
        
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
          
          if (buf.channels == m_output_channels)
          {
            for (int c = 0; c < m_output_channels; ++c)
            {
              int sample = mix_buffer[f * m_output_channels + c] +
                static_cast<int>(buf.data[idx + c] * src.volume);
              mix_buffer[f * m_output_channels + c] = std::clamp(sample, -32768, 32767);
            }
          }
          else if (buf.channels == 1 && m_output_channels == 2)
          {
            // Mono to stereo - duplicate channel
            int mono_sample = static_cast<int>(buf.data[idx] * src.volume);
            mix_buffer[f * m_output_channels] = std::clamp(mix_buffer[f * m_output_channels] + mono_sample, -32768, 32767);
            mix_buffer[f * m_output_channels + 1] = std::clamp(mix_buffer[f * m_output_channels + 1] + mono_sample, -32768, 32767);
          }
          else if (buf.channels == 2 && m_output_channels == 1)
          {
            // Stereo to mono - average channels
            int left = buf.data[idx];
            int right = buf.data[idx + 1];
            int mono_sample = static_cast<int>((left + right) * 0.5f * src.volume);
            mix_buffer[f] = std::clamp(mix_buffer[f] + mono_sample, -32768, 32767);
          }
          
          pos += pitch_adjusted_step; // pitch = playback speed
        }
        
        src.play_pos = static_cast<size_t>(pos);
      }
      
      m_device->write_samples(mix_buffer.data(), m_frame_count);
    }
    
    // Play the source
    void play_source(unsigned int src_id)
    {
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        Source& src = it->second;
        src.playing = true;
        src.play_pos = 0;  // start from beginning
      }
    }
    
    // Check if a source is playing
    bool is_source_playing(unsigned int src_id) const
    {
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
        return it->second.playing;
      return false;
    }
    
    // Pause the source
    void pause_source(unsigned int src_id)
    {
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
        it->second.playing = false;
    }
    
    // Stop the source
    void stop_source(unsigned int src_id)
    {
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
      {
        it->second.playing = false;
        it->second.play_pos = 0;
      }
    }
    
    // Set volume
    void set_source_volume(unsigned int src_id, float vol)
    {
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
        it->second.volume = vol;
    }
    
    // Set pitch (note: not yet implemented in mixer)
    void set_source_pitch(unsigned int src_id, float pitch)
    {
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
        it->second.pitch = pitch;
    }
    
    // Set looping
    void set_source_looping(unsigned int src_id, bool loop)
    {
      auto it = m_sources.find(src_id);
      if (it != m_sources.end())
        it->second.looping = loop;
    }
    
    void print_device_name() const
    {
      if (m_device != nullptr)
        std::cout << m_device->device_name() << std::endl;
      else
        std::cout << "Unknown device" << std::endl;
    }
    
  };
  
}
