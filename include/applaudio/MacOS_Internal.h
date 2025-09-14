//
//  MacOS_Internal.h
//  applaudio
//
//  Created by Rasmus Anthin on 2025-09-14.
//

#pragma once
#include "IApplaudio_Internal.h"

#ifdef __APPLE__

#include <AudioToolbox/AudioToolbox.h>
#include <vector>
#include <mutex>
#include <cstring>

namespace applaudio
{
  
  class MacOS_Internal : public IApplaudio_Internal
  {
    AudioComponentInstance audio_unit = nullptr;
    int sample_rate = 44100;
    int channels = 2;
    
    std::vector<short> ring_buffer;
    size_t read_pos = 0;
    size_t write_pos = 0;
    std::mutex buffer_mutex;
    
    static OSStatus render_cb(void* user,
                              AudioUnitRenderActionFlags* flags,
                              const AudioTimeStamp* ts,
                              UInt32 bus,
                              UInt32 num_frames,
                              AudioBufferList* io_data)
    {
      return reinterpret_cast<MacOS_Internal*>(user)
        ->render(flags, ts, bus, num_frames, io_data);
    }
    
  public:
    MacOS_Internal() = default;
    ~MacOS_Internal() override { shutdown(); }
    
    bool init(int sr, int ch) override
    {
      sample_rate = sr;
      channels = ch;
      ring_buffer.resize(sr * ch); // 1s buffer
      
      AudioComponentDescription desc = {0};
      desc.componentType = kAudioUnitType_Output;
      desc.componentSubType = kAudioUnitSubType_DefaultOutput;
      desc.componentManufacturer = kAudioUnitManufacturer_Apple;
      
      AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
      if (!comp) return false;
      
      if (AudioComponentInstanceNew(comp, &audio_unit) != noErr) return false;
      
      AURenderCallbackStruct cb = { render_cb, this };
      if (AudioUnitSetProperty(audio_unit,
                               kAudioUnitProperty_SetRenderCallback,
                               kAudioUnitScope_Input,
                               0,
                               &cb,
                               sizeof(cb)) != noErr)
      {
        return false;
      }
      
      if (AudioUnitInitialize(audio_unit) != noErr) return false;
      if (AudioOutputUnitStart(audio_unit) != noErr) return false;
      
      return true;
    }
    
    void shutdown() override
    {
      if (audio_unit)
      {
        AudioOutputUnitStop(audio_unit);
        AudioUnitUninitialize(audio_unit);
        AudioComponentInstanceDispose(audio_unit);
        audio_unit = nullptr;
      }
    }
    
    void write_samples(const short* data, size_t frames) override
    {
      std::lock_guard<std::mutex> lock(buffer_mutex);
      size_t samples = frames * channels;
      for (size_t i = 0; i < samples; i++)
      {
        ring_buffer[write_pos] = data[i];
        write_pos = (write_pos + 1) % ring_buffer.size();
        if (write_pos == read_pos) // overwrite oldest
        {
          read_pos = (read_pos + channels) % ring_buffer.size();
        }
      }
    }
    
    int get_sample_rate() const override { return sample_rate; }
    
  private:
    OSStatus render(AudioUnitRenderActionFlags*,
                    const AudioTimeStamp*,
                    UInt32,
                    UInt32 num_frames,
                    AudioBufferList* io_data)
    {
      std::lock_guard<std::mutex> lock(buffer_mutex);
      
      //size_t samples = num_frames * channels;
      for (UInt32 buf = 0; buf < io_data->mNumberBuffers; buf++)
      {
        short* out = reinterpret_cast<short*>(io_data->mBuffers[buf].mData);
        size_t outSamples = io_data->mBuffers[buf].mDataByteSize / sizeof(short);
        
        for (size_t i = 0; i < outSamples; i++)
        {
          if (read_pos != write_pos)
          {
            out[i] = ring_buffer[read_pos];
            read_pos = (read_pos + 1) % ring_buffer.size();
          }
          else
          {
            out[i] = 0; // underrun -> silence
          }
        }
      }
      return noErr;
    }
  };
  
}

#endif // __APPLE__
