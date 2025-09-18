//
//  Backend_Linux_ALSA.h
//  applaudio
//
//  Created by Rasmus Anthin on 2025-09-14.
//

#pragma once
#include "IBackend.h" // defines IMyLib_internal

#ifdef __linux__

#include <alsa/asoundlib.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <iostream>
#include <cstring>

namespace applaudio
{
  
  class Backend_Linux_ALSA : public IBackend
  {
  public:
    Backend_Linux_ALSA() = default;
    virtual ~Backend_Linux_ALSA() override { shutdown(); }
    
    virtual bool startup(int sample_rate, int channels) override
    {
      int err = snd_pcm_open(&m_pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
      if (err < 0)
      {
        std::cerr << "ALSA: cannot open device: " << snd_strerror(err) << std::endl;
        return false;
      }
      
      snd_pcm_hw_params_alloca(&m_hw_params);
      snd_pcm_hw_params_any(m_pcm_handle, m_hw_params);
      
      // Interleaved
      snd_pcm_hw_params_set_access(m_pcm_handle, m_hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
      
      // Format: signed 16-bit little endian
      snd_pcm_hw_params_set_format(m_pcm_handle, m_hw_params, SND_PCM_FORMAT_S16_LE);
      
      // Channels
      if ((err = snd_pcm_hw_params_set_channels(m_pcm_handle, m_hw_params, channels)) < 0)
      {
        std::cerr << "ALSA: cannot set channels: " << snd_strerror(err) << std::endl;
        return false;
      }
      
      // Sample rate
      unsigned int rate = sample_rate;
      int dir = 0;
      if ((err = snd_pcm_hw_params_set_rate_near(m_pcm_handle, m_hw_params, &rate, &dir)) < 0)
      {
        std::cerr << "ALSA: cannot set rate: " << snd_strerror(err) << std::endl;
        return false;
      }
      
      if ((err = snd_pcm_hw_params(m_pcm_handle, m_hw_params)) < 0)
      {
        std::cerr << "ALSA: cannot set hw params: " << snd_strerror(err) << std::endl;
        return false;
      }
      
      m_sample_rate = rate;
      m_channels = channels;
      
      if ((err = snd_pcm_prepare(m_pcm_handle)) < 0)
      {
        std::cerr << "ALSA: prepare failed: " << snd_strerror(err) << std::endl;
        return false;
      }
      
      m_ring_buffer.resize(sample_rate * channels * 2); // 2s buffer
      std::fill(m_ring_buffer.begin(), m_ring_buffer.end(), 0);
      
      m_running = true;
      m_render_thread = std::thread(&Linux_Internal::render_loop, this);
      
      return true;
    }
    
    virtual void shutdown() override
    {
      m_running = false;
      if (m_render_thread.joinable())
        m_render_thread.join();
      
      if (m_pcm_handle != nullptr)
      {
        snd_pcm_close(m_pcm_handle);
        m_pcm_handle = nullptr;
      }
    }
    
    virtual bool write_samples(const short* data, size_t frames) override
    {
      if (m_pcm_handle == nullptr)
        return false;
    
      std::lock_guard<std::mutex> lock(m_buffer_mutex);
      size_t samples = frames * m_channels;
      for (size_t i = 0; i < samples; ++i)
      {
        m_ring_buffer[m_write_pos] = data[i];
        m_write_pos = (m_write_pos + 1) % m_ring_buffer.size();
        if (m_write_pos == m_read_pos)
          m_read_pos = (m_read_pos + 1) % m_ring_buffer.size(); // prevent overwrite
      }
      return true;
    }
    
    virtual int get_sample_rate() const override
    {
      return m_sample_rate;
    }
    
    virtual int get_buffer_size_frames() const override
    {
      snd_pcm_uframes_t size = 0;
      if (m_pcm_handle != nullptr)
        snd_pcm_get_params(m_pcm_handle, &size, nullptr);
      return static_cast<int>(size);
    }
    
    virtual std::string backend_name() const override { return "Linux : ALSA"; }
    
  private:
    void render_loop()
    {
      const size_t frame_chunk = 512;
      std::vector<short> temp_buffer(frame_chunk * m_channels);
      
      while (m_running)
      {
        {
          std::lock_guard<std::mutex> lock(m_buffer_mutex);
          for (size_t i = 0; i < temp_buffer.size(); ++i)
          {
            if (m_read_pos != m_write_pos)
            {
              temp_buffer[i] = m_ring_buffer[m_read_pos];
              m_read_pos = (m_read_pos + 1) % m_ring_buffer.size();
            }
            else
              temp_buffer[i] = 0; // underrun
          }
        }
        
        snd_pcm_sframes_t written = snd_pcm_writei(m_pcm_handle, temp_buffer.data(), frame_chunk);
        if (written < 0)
          written = snd_pcm_recover(m_pcm_handle, written, 1);
        if (written < 0)
          std::cerr << "ALSA: write error: " << snd_strerror((int)written) << std::endl;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // pacing
      }
    }
    
    snd_pcm_t* m_pcm_handle = nullptr;
    snd_pcm_hw_params_t* m_hw_params = nullptr;
    int m_sample_rate = 0;
    int m_channels = 0;
    
    std::vector<short> m_ring_buffer;
    size_t m_read_pos = 0;
    size_t m_write_pos = 0;
    std::mutex m_buffer_mutex;
    
    std::atomic<bool> m_running{false};
    std::thread m_render_thread;
  };
  
  
}

#endif
