//
//  Win_Internal.h
//  applaudio
//
//  Created by Rasmus Anthin on 2025-09-14.
//

#pragma once
#include "IApplaudio_Internal.h"

#ifdef _WIN32

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <iostream>

namespace applaudio
{
  
  class Win_Internal : public IApplaudio_Internal
  {
    std::vector<short> m_pending;
    
  public:
    Win_Internal() = default;
    
    ~Win_Internal()
    {
      shutdown();
    }
    
    bool startup(int sample_rate, int channels) override
    {
      m_sample_rate = sample_rate;
      m_channels = channels;
      
      HRESULT hr;
      
      // Get default audio endpoint
      hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
      if (FAILED(hr))
      {
        std::cerr << "WASAPI: CoInitialize failed\n";
        return false;
      }
      
      IMMDeviceEnumerator* enumerator = nullptr;
      hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                            __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
      if (FAILED(hr))
      {
        std::cerr << "WASAPI: CoCreateInstance failed\n";
        return false;
      }
      
      hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_device);
      enumerator->Release();
      if (FAILED(hr))
      {
        std::cerr << "WASAPI: GetDefaultAudioEndpoint failed\n";
        return false;
      }
      
      hr = m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&m_audio_client);
      if (FAILED(hr))
      {
        std::cerr << "WASAPI: Activate failed\n";
        return false;
      }
      
      // Setup wave format
      WAVEFORMATEX wf = {0};
      wf.wFormatTag = WAVE_FORMAT_PCM;
      wf.nChannels = (WORD)m_channels;
      wf.nSamplesPerSec = m_sample_rate;
      wf.wBitsPerSample = 16;
      wf.nBlockAlign = wf.nChannels * wf.wBitsPerSample / 8;
      wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
      wf.cbSize = 0;
      
      hr = m_audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                      0,
                                      10000000, // buffer duration in 100-ns units (1s)
                                      0,
                                      &wf,
                                      nullptr);
      if (FAILED(hr))
      {
        std::cerr << "WASAPI: Initialize failed\n";
        return false;
      }
      
      hr = m_audio_client->GetService(__uuidof(IAudioRenderClient), (void**)&m_render_client);
      if (FAILED(hr))
      {
        std::cerr << "WASAPI: GetService failed\n";
        return false;
      }
      
      hr = m_audio_client->Start();
      if (FAILED(hr))
      {
        std::cerr << "WASAPI: Start failed\n";
        return false;
      }
      
      return true;
    }
    
    void shutdown() override
    {
      if (m_audio_client != nullptr)
      {
        m_audio_client->Stop();
      }
      
      if (m_render_client != nullptr)
      {
        m_render_client->Release();
        m_render_client = nullptr;
      }
      
      if (m_audio_client != nullptr)
      {
        m_audio_client->Release();
        m_audio_client = nullptr;
      }
      
      if (m_device != nullptr)
      {
        m_device->Release();
        m_device = nullptr;
      }
      
      CoUninitialize();
    }
    
    bool write_samples(const short* data, size_t frames) override
    {
      if (m_audio_client == nullptr || m_render_client == nullptr) return;
      
      // Append new samples to pending
      size_t samples = frames * m_channels;
      m_pending.insert(m_pending.end(), data, data + samples);
      
      while (!m_pending.empty())
      {
        UINT32 padding = 0;
        UINT32 bufferSize = 0;
        
        if (FAILED(m_audio_client->GetCurrentPadding(&padding)))
        {
          std::cerr << "WASAPI: GetCurrentPadding failed!\n";
          return false;
        }
        if (FAILED(m_audio_client->GetBufferSize(&bufferSize)))
        {
          std::cerr << "WASAPI: GetBufferSize failed!n";
          return false;
        }
        
        UINT32 available = bufferSize - padding;
        if (available == 0) break; // no space yet → wait for next call
        
        UINT32 frames_to_write = std::min<UINT32>(available, m_pending.size() / m_channels);
        
        BYTE* pData = nullptr;
        if (FAILED(m_render_client->GetBuffer(frames_to_write, &pData)))
        {
          std::cerr << "WASAPI: GetBuffer failed!\n";
          return false;
        }
        
        memcpy(pData, m_pending.data(), frames_to_write * m_channels * sizeof(short));
        
        if (FAILED(m_render_client->ReleaseBuffer(frames_to_write, 0)))
        {
          std::cerr << "WASAPI: ReleaseBuffer failed!\n";
          return false;
        }
        
        // Remove written samples from pending
        m_pending.erase(m_pending.begin(), m_pending.begin() + frames_to_write * m_channels);
      }
      return true;
    }
    
    int get_sample_rate() const override
    {
      return m_sample_rate;
    }
    
    virtual int get_buffer_size_frames() const override
    {
      UINT32 buffer_frames = 0;
      if (m_audio_client != nullptr && SUCCEEDED(m_audio_client->GetBufferSize(&buffer_frames)))
        return static_cast<int>(buffer_frames);
      return 1024; // Fallback value
    }
    
    virtual std::string device_name() const override { return "Win : WASAPI"; }
    
  private:
    IMMDevice* m_device = nullptr;
    IAudioClient* m_audio_client = nullptr;
    IAudioRenderClient* m_render_client = nullptr;
    int m_sample_rate = 0;
    int m_channels = 0;
  };
  
  
}

#endif // _WIN32
