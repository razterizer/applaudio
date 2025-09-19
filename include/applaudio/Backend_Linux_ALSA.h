//
//  Backend_Linux_ALSA.h
//  applaudio
//
//  Created by Rasmus Anthin on 2025-09-14.
//

#pragma once
#include "IBackend.h"

#ifdef __linux__

#include <alsa/asoundlib.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <iostream>
#include <cstring>
#include <condition_variable>

namespace applaudio
{
  
  class Backend_Linux_ALSA : public IBackend
  {
  public:
    Backend_Linux_ALSA() = default;
    virtual ~Backend_Linux_ALSA() override { shutdown(); }
    
    virtual bool startup(int sample_rate, int channels) override
    {
      int err = 0;
      
      // Open PCM device
      if ((err = snd_pcm_open(&m_pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0)
      {
        std::cerr << "ALSA: cannot open device: " << snd_strerror(err) << std::endl;
        return false;
      }
      
      // Allocate hardware parameters
      snd_pcm_hw_params_t* hw_params;
      snd_pcm_hw_params_alloca(&hw_params);
      
      // Initialize parameters with default values
      if ((err = snd_pcm_hw_params_any(m_pcm_handle, hw_params)) < 0)
      {
        std::cerr << "ALSA: cannot initialize parameters: " << snd_strerror(err) << std::endl;
        return false;
      }
      
      // Set access type
      if ((err = snd_pcm_hw_params_set_access(m_pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
      {
        std::cerr << "ALSA: cannot set access type: " << snd_strerror(err) << std::endl;
        return false;
      }
      
      // Set sample format based on APL_SAMPLE_TYPE
#ifdef APL_32
      snd_pcm_format_t format = SND_PCM_FORMAT_FLOAT_LE; // 32-bit float
#else
      snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;   // 16-bit integer
#endif
      if ((err = snd_pcm_hw_params_set_format(m_pcm_handle, hw_params, format)) < 0)
      {
        std::cerr << "ALSA: cannot set sample format: " << snd_strerror(err) << std::endl;
        return false;
      }
      
      // Set channels
      if ((err = snd_pcm_hw_params_set_channels(m_pcm_handle, hw_params, channels)) < 0)
      {
        std::cerr << "ALSA: cannot set channels: " << snd_strerror(err) << std::endl;
        return false;
      }
      
      // Set sample rate
      unsigned int rate = sample_rate;
      if ((err = snd_pcm_hw_params_set_rate_near(m_pcm_handle, hw_params, &rate, 0)) < 0)
      {
        std::cerr << "ALSA: cannot set sample rate: " << snd_strerror(err) << std::endl;
        return false;
      }
      
      // Set buffer size (2 periods of 1024 frames each)
      snd_pcm_uframes_t buffer_size = 2048;
      snd_pcm_uframes_t period_size = 1024;
      if ((err = snd_pcm_hw_params_set_buffer_size_near(m_pcm_handle, hw_params, &buffer_size)) < 0 ||
          (err = snd_pcm_hw_params_set_period_size_near(m_pcm_handle, hw_params, &period_size, 0)) < 0)
      {
        std::cerr << "ALSA: cannot set buffer/period size: " << snd_strerror(err) << std::endl;
        return false;
      }
      
      // Apply parameters
      if ((err = snd_pcm_hw_params(m_pcm_handle, hw_params)) < 0)
      {
        std::cerr << "ALSA: cannot set hw parameters: " << snd_strerror(err) << std::endl;
        return false;
      }
      
      m_sample_rate = rate;
      m_channels = channels;
      
      // Prepare PCM
      if ((err = snd_pcm_prepare(m_pcm_handle)) < 0)
      {
        std::cerr << "ALSA: cannot prepare PCM: " << snd_strerror(err) << std::endl;
        return false;
      }
      
      // Initialize ring buffer (4 seconds)
      size_t buffer_samples = sample_rate * channels * 4;
      m_ring_buffer.resize(buffer_samples);
      std::fill(m_ring_buffer.begin(), m_ring_buffer.end(), 0);
      
      m_running = true;
      m_render_thread = std::thread(&Backend_Linux_ALSA::render_loop, this);
      
      return true;
    }
    
    virtual void shutdown() override
    {
      m_running = false;
      m_buffer_cv.notify_all(); // Wake up render thread
      
      if (m_render_thread.joinable())
        m_render_thread.join();
      
      if (m_pcm_handle != nullptr)
      {
        snd_pcm_drain(m_pcm_handle); // Drain remaining samples
        snd_pcm_close(m_pcm_handle);
        m_pcm_handle = nullptr;
      }
    }
    
    virtual bool write_samples(const APL_SAMPLE_TYPE* data, size_t frames) override
    {
      if (m_pcm_handle == nullptr)
        return false;
      
      std::unique_lock<std::mutex> lock(m_buffer_mutex);
      
      size_t samples_to_write = frames * m_channels;
      size_t available_space = (m_read_pos > m_write_pos) ?
      (m_read_pos - m_write_pos - 1) :
      (m_ring_buffer.size() - m_write_pos + m_read_pos - 1);
      
      if (samples_to_write > available_space)
      {
        // Buffer overrun - skip some samples
        samples_to_write = available_space;
      }
      
      for (size_t i = 0; i < samples_to_write; ++i)
      {
        m_ring_buffer[m_write_pos] = data[i];
        m_write_pos = (m_write_pos + 1) % m_ring_buffer.size();
      }
      
      lock.unlock();
      m_buffer_cv.notify_one(); // Notify render thread
      
      return samples_to_write == frames * m_channels;
    }
    
    virtual int get_sample_rate() const override { return m_sample_rate; }
    
    virtual int get_buffer_size_frames() const override
    {
      snd_pcm_uframes_t buffer_size = 0;
      snd_pcm_uframes_t period_size = 0;
      if (m_pcm_handle != nullptr)
        snd_pcm_get_params(m_pcm_handle, &buffer_size, &period_size);
      return static_cast<int>(buffer_size);
    }
    
    virtual std::string backend_name() const override { return "Linux : ALSA"; }
    
  private:
    void render_loop()
    {
      const size_t frames_per_chunk = 512;
      std::vector<APL_SAMPLE_TYPE> temp_buffer(frames_per_chunk * m_channels);
      
      while (m_running)
      {
        {
          std::unique_lock<std::mutex> lock(m_buffer_mutex);
          
          // Wait for data if buffer is empty
          if (m_read_pos == m_write_pos)
          {
            m_buffer_cv.wait_for(lock, std::chrono::milliseconds(100),
                                 [this]() { return m_read_pos != m_write_pos || !m_running; });
            
            if (!m_running) break;
            if (m_read_pos == m_write_pos) continue; // Still no data
          }
          
          // Copy data from ring buffer
          for (size_t i = 0; i < temp_buffer.size(); ++i)
          {
            if (m_read_pos != m_write_pos)
            {
              temp_buffer[i] = m_ring_buffer[m_read_pos];
              m_read_pos = (m_read_pos + 1) % m_ring_buffer.size();
            }
            else
            {
              // Buffer underrun - fill with silence
              temp_buffer[i] = static_cast<APL_SAMPLE_TYPE>(0);
            }
          }
        }
        
        // Write to ALSA
        snd_pcm_sframes_t written = snd_pcm_writei(m_pcm_handle, temp_buffer.data(), frames_per_chunk);
        
        if (written == -EPIPE)
        {
          // Underrun occurred
          snd_pcm_prepare(m_pcm_handle);
        }
        else if (written < 0)
        {
          std::cerr << "ALSA: write error: " << snd_strerror(static_cast<int>(written)) << std::endl;
          snd_pcm_recover(m_pcm_handle, written, 1);
        }
      }
    }
    
    snd_pcm_t* m_pcm_handle = nullptr;
    int m_sample_rate = 0;
    int m_channels = 0;
    
    std::vector<APL_SAMPLE_TYPE> m_ring_buffer;
    size_t m_read_pos = 0;
    size_t m_write_pos = 0;
    std::mutex m_buffer_mutex;
    std::condition_variable m_buffer_cv;
    
    std::atomic<bool> m_running{false};
    std::thread m_render_thread;
  };
  
}

#endif // __linux__
