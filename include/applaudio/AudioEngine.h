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


namespace applaudio
{

  struct Buffer
  {
    std::vector<short> data;
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
    int m_channels = 0;
    int m_sample_rate = 0;
    
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
                                                     static_cast<long long>(1e6 * m_frame_count / m_sample_rate)
                                                     );
        
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
    
    bool startup(int sample_rate, int num_channels = 2, bool verbose = false)
    {
      m_sample_rate = sample_rate;
      m_channels = num_channels;
      
      if (m_device == nullptr || !m_device->startup(sample_rate, m_channels))
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
          << "Fs = " << m_sample_rate << " Hz, "
          << m_channels << " channels, "
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
    
    void set_buffer_data(unsigned int buf_id, const std::vector<short>& data)
    {
      auto& buf = m_buffers[buf_id];
      buf.data = data;
    }
    
    void attach_buffer_to_source(unsigned int src_id, unsigned int buf_id)
    {
      m_sources[src_id].buffer_id = buf_id;
    }
    
    
    void mix()
    {
      std::vector<short> mix_buffer(m_frame_count * m_channels, 0);
      
      for (auto& [id, src] : m_sources)
      {
        if (!src.playing) continue;
        
        const auto& buf = m_buffers[src.buffer_id];
        double pos = src.play_pos;
        
        for (int f = 0; f < m_frame_count; ++f)
        {
          size_t idx = static_cast<size_t>(pos) * m_channels;
          if (idx + m_channels > buf.data.size())
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
          
          for (int c = 0; c < m_channels; ++c)
          {
            int sample = mix_buffer[f * m_channels + c] +
            static_cast<int>(buf.data[idx + c] * src.volume);
            mix_buffer[f * m_channels + c] = std::clamp(sample, -32768, 32767);
          }
          
          pos += src.pitch; // pitch = playback speed
        }
        
        src.play_pos = pos;
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
