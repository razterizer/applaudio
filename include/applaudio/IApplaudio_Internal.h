//
//  IApplaudio_Internal.h
//  applaudio
//
//  Created by Rasmus Anthin on 2025-09-14.
//

#pragma once

namespace applaudio
{
  
  struct IApplaudio_Internal
  {
    virtual ~IApplaudio_Internal() {}
    virtual bool init(int sample_rate, int channels) = 0;
    virtual void shutdown() = 0;
    virtual void write_samples(const short* data, size_t frames) = 0;
    virtual int get_sample_rate() const = 0;
  };
  
}
