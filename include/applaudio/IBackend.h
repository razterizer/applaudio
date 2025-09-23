//
//  IBackend.h
//  applaudio
//
//  Created by Rasmus Anthin on 2025-09-14.
//

#pragma once
#include "defines.h"
#include <string>

namespace applaudio
{
  
  struct IBackend
  {
    virtual ~IBackend() {}
    virtual bool startup(int request_sample_rate, int request_channels, bool request_exclusive_mode_if_supported, bool verbose) = 0;
    virtual void shutdown() = 0;
    virtual bool write_samples(const APL_SAMPLE_TYPE* data, size_t frames) = 0;
    virtual int get_sample_rate() const = 0;
    virtual int get_num_channels() const = 0;
    virtual int get_bit_format() const = 0;
    virtual int get_buffer_size_frames() const = 0;
    virtual std::string backend_name() const = 0;
  };
  
}
