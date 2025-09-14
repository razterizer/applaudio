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
      hr = CoInitialize(nullptr);
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
    
    void write_samples(const short* data, size_t frames) override
    {
      if (m_audio_client == nullptr || m_render_client == nullptr) return;
      
      UINT32 padding = 0;
      if (FAILED(m_audio_client->GetCurrentPadding(&padding)))
      {
        std::cerr << "WASAPI: GetCurrentPadding failed\n";
        return;
      }
      
      UINT32 bufferSize = 0;
      if (FAILED(m_audio_client->GetBufferSize(&bufferSize)))
      {
        std::cerr << "WASAPI: GetBufferSize failed\n";
        return;
      }
      
      UINT32 available = bufferSize - padding;
      if (available < frames)
      {
        // Too much data queued; drop samples
        frames = available;
      }
      
      BYTE* pData = nullptr;
      if (FAILED(m_render_client->GetBuffer(frames, &pData)))
      {
        std::cerr << "WASAPI: GetBuffer failed\n";
        return;
      }
      
      // Copy data
      memcpy(pData, data, frames * m_channels * sizeof(short));
      
      if (FAILED(m_render_client->ReleaseBuffer(frames, 0)))
      {
        std::cerr << "WASAPI: ReleaseBuffer failed\n";
        return;
      }
    }
    
    int get_sample_rate() const override
    {
      return m_sample_rate;
    }
    
    virtual int get_buffer_size_frames() const override
    {
      UINT32 buffer_frames = 0;
      if (m_audio_client != nullptr)
        m_audio_client->GetBufferSize(&buffer_frames);
      return static_cast<int>(buffer_frames);
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
