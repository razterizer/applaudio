//
//  Backend_Windows_WASAPI.h
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
#include <stddef.h> // For sizeof
#include <ksmedia.h>

namespace applaudio
{
  
  class Backend_Windows_WASAPI : public IBackend
  {
  public:
    Backend_Windows_WASAPI() = default;
    virtual ~Backend_Windows_WASAPI() override { shutdown(); }
    
    virtual bool startup(int request_sample_rate, int request_channels, bool request_exclusive_mode_if_supported, bool verbose) override
    {
      m_sample_rate = request_sample_rate;
      m_channels = request_channels;

      _AUDCLNT_SHAREMODE share_mode = request_exclusive_mode_if_supported ? AUDCLNT_SHAREMODE_EXCLUSIVE : AUDCLNT_SHAREMODE_SHARED;

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

      WAVEFORMATEX* format = nullptr;
      WAVEFORMATEXTENSIBLE wf = {};
      bool format_from_mix = false;

      if (share_mode == AUDCLNT_SHAREMODE_SHARED)
      {
        hr = m_audio_client->GetMixFormat(&format);
        if (FAILED(hr) || format == nullptr)
        {
          std::cerr << "WASAPI: GetMixFormat failed!\n";
          return false;
        }
        format_from_mix = true;
      }
      else
      {
        // Build requested format
        wf.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        wf.Format.nChannels = request_channels;
        wf.Format.nSamplesPerSec = request_sample_rate;
        wf.Format.wBitsPerSample = 8 * sizeof(APL_SAMPLE_TYPE);
        wf.Format.nBlockAlign = wf.Format.nChannels * wf.Format.wBitsPerSample / 8;
        wf.Format.nAvgBytesPerSec = wf.Format.nSamplesPerSec * wf.Format.nBlockAlign;
        wf.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
#ifdef APL_32
        wf.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
#else
        wf.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
#endif
        wf.Samples.wValidBitsPerSample = wf.Format.wBitsPerSample;
        wf.dwChannelMask = (request_channels == 2) ? (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT) : 0;
        format = (WAVEFORMATEX*)&wf;

        // Check if supported
        WAVEFORMATEX* closest = nullptr;
        hr = m_audio_client->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, format, &closest);
        if (FAILED(hr))
        {
          std::cerr << "WASAPI: Requested exclusive format not supported. Attempting to use a sample rate of 44.1 kHz.\n";
          format->nSamplesPerSec = 44100;
          hr = m_audio_client->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, format, &closest);
          if (FAILED(hr))
          {
            std::cerr << "WASAPI: Requested exclusive format not supported.\n";
            if (closest) CoTaskMemFree(closest);
            return false;
          }
          m_sample_rate = wf.Format.nSamplesPerSec;
        }
        if (closest) CoTaskMemFree(closest);
      }

      // Warn if requested format doesn't match what will be used
      if (format->nSamplesPerSec != request_sample_rate || format->nChannels != request_channels)
      {
        std::cerr << "WASAPI: Requested format (" << request_sample_rate << " Hz, " << request_channels
          << " ch) does not match device format (" << format->nSamplesPerSec
          << " Hz, " << format->nChannels << " ch). Using device format.\n";
        m_sample_rate = format->nSamplesPerSec;
        m_channels = format->nChannels;
      }
      m_bits = format->wBitsPerSample;

#ifdef APL_32
      if (m_bits != 32)
      {
        std::cerr << "WASAPI: format->wBitsPerSample must be 32 bits, which is the expected format!\n";
        return false;
      }
      if (format->wFormatTag != WAVE_FORMAT_EXTENSIBLE ||
        reinterpret_cast<WAVEFORMATEXTENSIBLE*>(format)->SubFormat != KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
      {
        std::cerr << "WASAPI: Expected 32-bit float format, but got something else.\n";
        return false;
      }
#else
      if (m_bits != 16)
      {
        std::cerr << "WASAPI: format->wBitsPerSample must be 16 bits, which is the expected format!\n";
        return false;
      }
      if (format->wFormatTag != WAVE_FORMAT_PCM &&
        (format->wFormatTag != WAVE_FORMAT_EXTENSIBLE ||
          reinterpret_cast<WAVEFORMATEXTENSIBLE*>(format)->SubFormat != KSDATAFORMAT_SUBTYPE_PCM))
      {
        std::cerr << "WASAPI: Expected 16-bit PCM format, but got something else.\n";
        return false;
      }
#endif


      m_ring_buffer.resize(m_sample_rate * m_channels * 2); // 2s buffer

      if (verbose)
      {
        std::cout << "WASAPI: Using format: "
          << "Channels=" << format->nChannels << ", "
          << "SampleRate=" << format->nSamplesPerSec << ", "
          << "BitsPerSample=" << format->wBitsPerSample << ", "
          << "BlockAlign=" << format->nBlockAlign << ", "
          << "AvgBytesPerSec=" << format->nAvgBytesPerSec << ", "
          << "FormatTag=" << format->wFormatTag << std::endl;
      }

      m_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
      if (!m_event)
      {
        std::cerr << "WASAPI: Failed to create event!\n";
        if (format_from_mix) CoTaskMemFree(format);
        return false;
      }

      REFERENCE_TIME buffer_duration = (share_mode == AUDCLNT_SHAREMODE_EXCLUSIVE) ? 0 : 20 * 10000; // 20ms for shared, 0 for exclusive

      hr = m_audio_client->Initialize(
        share_mode,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        buffer_duration,
        0,
        format,
        nullptr
      );

      if (format_from_mix) CoTaskMemFree(format);

      if (FAILED(hr))
      {
        if (hr == AUDCLNT_E_DEVICE_IN_USE)
        {
          std::cerr << "WASAPI: Device is in exclusive use by another application.\n";
          return false;
        }
        if (hr == AUDCLNT_E_INVALID_SIZE)
        {
          std::cerr << "WASAPI: Invalid buffer size for this device.\n";
          return false;
        }
        if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED)
        {
          std::cerr << "WASAPI: Buffer size not aligned as required by the device.\n";
          return false;
        }
        if (hr == AUDCLNT_E_BUFFER_SIZE_ERROR)
        {
          std::cerr << "WASAPI: Buffer size error.\n";
          return false;
        }
        if (hr == AUDCLNT_E_UNSUPPORTED_FORMAT)
        {
          std::cerr << "WASAPI: Unsupported audio format for this device.\n";
          return false;
        }

        std::cerr << "WASAPI: Initialize failed! hr = 0x" << std::hex << hr << std::endl;
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
      m_render_thread = std::thread(&Backend_Windows_WASAPI::render_loop, this);

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
    
    virtual bool write_samples(const APL_SAMPLE_TYPE* data, size_t frames) override
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

    virtual int get_num_channels() const override
    {
      return m_channels;
    }

    virtual int get_bit_format() const override
    {
      return m_bits;
    }
    
    virtual int get_buffer_size_frames() const override
    {
      UINT32 buffer_frames = 0;
      if (m_audio_client && SUCCEEDED(m_audio_client->GetBufferSize(&buffer_frames)))
        return static_cast<int>(buffer_frames);
      return 512; // fallback
    }
    
    virtual std::string backend_name() const override { return "Win : WASAPI (Event-Driven)"; }
    
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
        APL_SAMPLE_TYPE* out = reinterpret_cast<APL_SAMPLE_TYPE*>(pData);
        for (UINT32 s = 0; s < available * m_channels; ++s)
        {
          if (m_read_pos != m_write_pos) {
            out[s] = m_ring_buffer[m_read_pos];
            m_read_pos = (m_read_pos + 1) % m_ring_buffer.size();
          }
          else
          {
            out[s] = static_cast<APL_SAMPLE_TYPE>(0); // underrun
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
    int m_bits = 32;
    
    std::vector<APL_SAMPLE_TYPE> m_ring_buffer;
    size_t m_read_pos = 0;
    size_t m_write_pos = 0;
    std::mutex m_buffer_mutex;
    
    std::atomic<bool> m_running{false};
    std::thread m_render_thread;
  };
  
}

#endif // _WIN32
