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


namespace applaudio
{

  struct Buffer
  {
    std::vector<short> data;
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

  class AudioEngine
  {
    std::unique_ptr<IApplaudio_Internal> m_device;
    
    int frame_count = 0;
    int channels = 0;
    
    std::unordered_map<unsigned int, Source> m_sources;
    std::unordered_map<unsigned int, Buffer> m_buffers;
    unsigned int m_next_source_id = 1;
    unsigned int m_next_buffer_id = 1;
    
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
      channels = num_channels;
      
      if (m_device == nullptr || !m_device->startup(sample_rate, channels))
      {
        std::cerr << "AudioEngine: Failed to initialize device\n";
        return false;
      }
      
      // Query the backend for its preferred frame count
      frame_count = m_device->get_buffer_size_frames();
      
      if (frame_count <= 0)
      {
        // fallback if the backend didn't report a valid size
        frame_count = 512;
      }
      
      if (verbose)
      {
        std::cout << "AudioEngine initialized: "
          << channels << " channels, "
          << frame_count << " frames per mix\n";
      }
      
      return true;
    }

    
    void shutdown()
    {
      if (m_device != nullptr)
      {
        m_device->shutdown();
      }
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
    
    void attach_buffer_to_source(unsigned int src_id, unsigned int buf_id)
    {
      m_sources[src_id].buffer_id = buf_id;
    }
    
    
    void mix()
    {
      std::vector<short> mix_buffer(frame_count * channels, 0);
      
      for (auto& src_pair : m_sources)
      {
        Source& src = src_pair.second;
        if (!src.playing) continue;
        
        const auto& buf = m_buffers[src.buffer_id];
        for (size_t i = 0; i < frame_count * channels; i++)
        {
          int sample = mix_buffer[i] + buf.data[src.play_pos + i] * src.volume;
          mix_buffer[i] = std::clamp(sample, -32768, 32767);
        }
        
        src.play_pos += frame_count * channels;
        if (src.play_pos >= buf.data.size())
        {
          if (src.looping)
            src.play_pos = 0;
          else
            src.playing = false;
        }
      }
      
      m_device->write_samples(mix_buffer.data(), frame_count);
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
