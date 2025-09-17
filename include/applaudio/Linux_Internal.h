//
//  Linux_Internal.h
//  applaudio
//
//  Created by Rasmus Anthin on 2025-09-14.
//

#pragma once
#include "IApplaudio_Internal.h" // defines IMyLib_internal

#ifdef __linux__

#include <alsa/asoundlib.h>
#include <stdexcept>
#include <string>
#include <iostream>

namespace applaudio
{
  
  class Linux_Internal : public IApplaudio_Internal
  {
  public:
    Linux_Internal() = default;
    
    ~Linux_Internal()
    {
      shutdown();
    }
    
    bool startup(int sample_rate, int channels) override
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
      
      return true;
    }
    
    void shutdown() override
    {
      if (m_pcm_handle != nullptr)
      {
        snd_pcm_close(m_pcm_handle);
        m_pcm_handle = nullptr;
      }
    }
    
    bool write_samples(const short* data, size_t frames) override
    {
      if (m_pcm_handle == nullptr) return;
      
      snd_pcm_sframes_t written = snd_pcm_writei(m_pcm_handle, data, frames);
      if (written < 0)
        written = snd_pcm_recover(m_pcm_handle, written, 1);
      if (written < 0)
      {
        std::cerr << "ALSA: write error: " << snd_strerror((int)written) << std::endl;
        return false;
      }
      return true;
    }
    
    int get_sample_rate() const override
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
    
    virtual std::string device_name() const override { return "Linux : ALSA"; }
    
  private:
    snd_pcm_t* m_pcm_handle = nullptr;
    snd_pcm_hw_params_t* m_hw_params = nullptr;
    int m_sample_rate = 0;
    int m_channels = 0;
  };
  
  
}

#endif // __linux__
