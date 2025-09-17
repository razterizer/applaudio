//
//  Backend_MacOS_CoreAudio.h
//  applaudio
//
//  Created by Rasmus Anthin on 2025-09-14.
//

#pragma once
#include "IBackend.h"

#ifdef __APPLE__

#include <AudioToolbox/AudioToolbox.h>
#include <vector>
#include <mutex>
#include <cstring>
#include <iostream>

namespace applaudio
{
  
  class Backend_MacOS_CoreAudio : public IBackend
  {
    AudioComponentInstance audio_unit = nullptr;
    int sample_rate = 44100;
    int channels = 2;
    
    std::vector<short> ring_buffer;
    size_t read_pos = 0;
    size_t write_pos = 0;
    std::mutex buffer_mutex;
    
    AudioStreamBasicDescription audio_format;
    
    static OSStatus render_cb(void* user,
                              AudioUnitRenderActionFlags* flags,
                              const AudioTimeStamp* ts,
                              UInt32 bus,
                              UInt32 num_frames,
                              AudioBufferList* io_data)
    {
      return reinterpret_cast<Backend_MacOS_CoreAudio*>(user)
        ->render(flags, ts, bus, num_frames, io_data);
    }
    
  public:
    Backend_MacOS_CoreAudio() = default;
    virtual ~Backend_MacOS_CoreAudio() override { shutdown(); }
    
    virtual bool startup(int sr, int ch) override
    {
      sample_rate = sr;
      channels = ch;
      ring_buffer.resize(sr * ch * 2); // 2s buffer for safety
      
      AudioComponentDescription desc = {0};
      desc.componentType = kAudioUnitType_Output;
      desc.componentSubType = kAudioUnitSubType_DefaultOutput;
      desc.componentManufacturer = kAudioUnitManufacturer_Apple;
      
      AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
      if (comp == nullptr) return false;
      
      if (AudioComponentInstanceNew(comp, &audio_unit) != noErr) return false;
      auto result = AudioComponentInstanceNew(comp, &audio_unit);
      if (result != noErr)
      {
        std::cerr << "AudioComponentInstanceNew failed: " << result << std::endl;
        return false;
      }
      
      // Set up the audio format FIRST
      audio_format.mSampleRate = sample_rate;
      audio_format.mFormatID = kAudioFormatLinearPCM;
      audio_format.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
      audio_format.mFramesPerPacket = 1;
      audio_format.mChannelsPerFrame = channels;
      audio_format.mBytesPerFrame = sizeof(short) * channels;
      audio_format.mBytesPerPacket = sizeof(short) * channels;
      audio_format.mBitsPerChannel = 16;
      
      // Set the format on the audio unit
      if (AudioUnitSetProperty(audio_unit,
                               kAudioUnitProperty_StreamFormat,
                               kAudioUnitScope_Input,
                               0,
                               &audio_format,
                               sizeof(audio_format)) != noErr)
      {
        return false;
      }
      
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
    
    virtual void shutdown() override
    {
      if (audio_unit != nullptr)
      {
        AudioOutputUnitStop(audio_unit);
        AudioUnitUninitialize(audio_unit);
        AudioComponentInstanceDispose(audio_unit);
        audio_unit = nullptr;
      }
    }
    
    virtual bool write_samples(const short* data, size_t frames) override
    {
      std::lock_guard<std::mutex> lock(buffer_mutex);
      size_t samples_to_write = frames * channels;
      
      for (size_t i = 0; i < samples_to_write; i++)
      {
        ring_buffer[write_pos] = data[i];
        write_pos = (write_pos + 1) % ring_buffer.size();
        
        // If buffer is full, move read pointer to avoid overwriting unread data
        if ((write_pos + 1) % ring_buffer.size() == read_pos)
          read_pos = (read_pos + 1) % ring_buffer.size();
      }
      return true;
    }
    
    virtual int get_sample_rate() const override { return sample_rate; }
    
    virtual int get_buffer_size_frames() const override
    {
      UInt32 buffer_size_frames = 0;
      UInt32 size = sizeof(buffer_size_frames);
      
      if (audio_unit != nullptr)
      {
        // Get the actual buffer size from the output scope
        AudioUnitGetProperty(audio_unit,
                             kAudioDevicePropertyBufferFrameSize,
                             kAudioUnitScope_Output,
                             0,
                             &buffer_size_frames,
                             &size);
      }
      
      if (buffer_size_frames == 0)
      {
        // Fallback to a reasonable default
        buffer_size_frames = 512;
      }
      
      return static_cast<int>(buffer_size_frames);
    }
    
    virtual std::string backend_name() const override { return "MacOS : CoreAudio"; }
    
  private:
    OSStatus render(AudioUnitRenderActionFlags*,
                    const AudioTimeStamp*,
                    UInt32,
                    UInt32 num_frames,
                    AudioBufferList* io_data)
    {
      std::lock_guard<std::mutex> lock(buffer_mutex);
      
      // Calculate how many samples we need
      //size_t samples_needed = num_frames * channels;
      
      for (UInt32 buf = 0; buf < io_data->mNumberBuffers; buf++)
      {
        short* out = reinterpret_cast<short*>(io_data->mBuffers[buf].mData);
        size_t out_samples = io_data->mBuffers[buf].mDataByteSize / sizeof(short);
        
        // Fill the buffer
        for (size_t i = 0; i < out_samples; i++)
        {
          if (read_pos != write_pos)
          {
            out[i] = ring_buffer[read_pos];
            read_pos = (read_pos + 1) % ring_buffer.size();
          }
          else
          {
            // Underrun - fill with silence
            out[i] = 0;
          }
        }
      }
      
      return noErr;
    }
  };
  
}

#endif // __APPLE__
