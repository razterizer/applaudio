//
//  Win_Internal.h
//  applaudio
//
//  Created by Rasmus Anthin on 2025-09-14.
//

#pragma once
#include "IBackend.h"

#ifdef _WIN32

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <cstring>

namespace applaudio
{
  
  class Win_Internal : public IBackend
  {
  public:
    Win_Internal() = default;
    virtual ~Win_Internal() override { shutdown(); }
    
    virtual bool startup(int sample_rate, int channels) override
    {
      m_sample_rate = sample_rate;
      m_channels = channels;
      m_ring_buffer.resize(sample_rate * channels * 2); // 2s buffer
      
      HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
      if (FAILED(hr))
      {
        std::cerr << "WASAPI: CoInitialize failed!\n";
        return false;
      }
      
      IMMDeviceEnumerator* enumerator = nullptr;
      hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                            __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
      if (FAILED(hr))
      {
        std::cerr << "WASAPI: CoCreateInstance failed!\n";
        return false;
      }
      
      hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_device);
      enumerator->Release();
      if (FAILED(hr))
      {
        std::cerr << "WASAPI: GetDefaultAudioEndpoint failed!\n";
        return false;
      }
      
      hr = m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&m_audio_client);
      if (FAILED(hr))
      {
        std::cerr << "WASAPI: Activate failed!\n";
        return false;
      }
      
      WAVEFORMATEX wf = {0};
      wf.wFormatTag = WAVE_FORMAT_PCM;
      wf.nChannels = (WORD)m_channels;
      wf.nSamplesPerSec = m_sample_rate;
      wf.wBitsPerSample = 16;
      wf.nBlockAlign = wf.nChannels * wf.wBitsPerSample / 8;
      wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
      wf.cbSize = 0;
      
      m_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
      if (!m_event)
      {
        std::cerr << "WASAPI: Failed to create event!\n";
        return false;
      }
      
      hr = m_audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                      AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                      200'000, // 20ms buffer
                                      0,
                                      &wf,
                                      nullptr);
      if (FAILED(hr))
      {
        std::cerr << "WASAPI: Initialize failed!\n";
        return false;
      }
      
      hr = m_audio_client->SetEventHandle(m_event);
      if (FAILED(hr))
      {
        std::cerr << "WASAPI: SetEventHandle failed!\n";
        return false;
      }
      
      hr = m_audio_client->GetService(__uuidof(IAudioRenderClient), (void**)&m_render_client);
      if (FAILED(hr))
      {
        std::cerr << "WASAPI: GetService failed!\n";
        return false;
      }
      
      hr = m_audio_client->Start();
      if (FAILED(hr))
      {
        std::cerr << "WASAPI: Start failed!\n";
        return false;
      }
      
      m_running = true;
      m_render_thread = std::thread(&Win_Internal::render_loop, this);
      
      return true;
    }
    
    virtual void shutdown() override
    {
      m_running = false;
      if (m_render_thread.joinable())
        m_render_thread.join();
      
      if (m_audio_client != nullptr)
        m_audio_client->Stop();
      
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
      
      if (m_event != nullptr)
      {
        CloseHandle(m_event);
        m_event = nullptr;
      }
      
      CoUninitialize();
    }
    
    virtual bool write_samples(const short* data, size_t frames) override
    {
      std::lock_guard<std::mutex> lock(m_buffer_mutex);
      size_t samples = frames * m_channels;
      for (size_t s = 0; s < samples; ++s)
      {
        m_ring_buffer[m_write_pos] = data[s];
        m_write_pos = (m_write_pos + 1) % m_ring_buffer.size();
        if ((m_write_pos + 1) % m_ring_buffer.size() == m_read_pos)
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
      UINT32 buffer_frames = 0;
      if (m_audio_client && SUCCEEDED(m_audio_client->GetBufferSize(&buffer_frames)))
        return static_cast<int>(buffer_frames);
      return 512; // fallback
    }
    
    virtual std::string device_name() const override { return "Win : WASAPI (Event-Driven)"; }
    
  private:
    void render_loop()
    {
      while (m_running)
      {
        DWORD wait_result = WaitForSingleObject(m_event, 2000); // timeout to avoid hang
        if (wait_result != WAIT_OBJECT_0)
          continue;
        
        UINT32 padding = 0, bufferSize = 0;
        if (FAILED(m_audio_client->GetCurrentPadding(&padding)) ||
            FAILED(m_audio_client->GetBufferSize(&bufferSize)))
          continue;
        
        UINT32 available = bufferSize - padding;
        if (available == 0)
          continue;
        
        BYTE* pData = nullptr;
        if (FAILED(m_render_client->GetBuffer(available, &pData)))
          continue;
        
        std::lock_guard<std::mutex> lock(m_buffer_mutex);
        short* out = reinterpret_cast<short*>(pData);
        for (UINT32 s = 0; s < available * m_channels; ++s)
        {
          if (m_read_pos != m_write_pos) {
            out[s] = m_ring_buffer[m_read_pos];
            m_read_pos = (m_read_pos + 1) % m_ring_buffer.size();
          }
          else
          {
            out[s] = 0; // underrun
          }
        }
        
        m_render_client->ReleaseBuffer(available, 0);
      }
    }
    
    IMMDevice* m_device = nullptr;
    IAudioClient* m_audio_client = nullptr;
    IAudioRenderClient* m_render_client = nullptr;
    HANDLE m_event = nullptr;
    
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

#endif // _WIN32
