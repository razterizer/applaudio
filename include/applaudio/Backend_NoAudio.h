//
//  Backend_NoAudio.h
//  applaudio
//
//  Created by Rasmus Anthin on 2025-09-18.
//

#pragma once
#include "IBackend.h"

namespace applaudio
{

  class Backend_NoAudio : public IBackend
  {
    virtual bool startup(int /*sample_rate*/, int /*channels*/) override { return true; };
    virtual void shutdown() override {};
    virtual bool write_samples(const short* /*data*/, size_t /*frames*/) override { return true; };
    virtual int get_sample_rate() const override { return 44100; };
    virtual int get_buffer_size_frames() const override { return 0; };
    virtual std::string backend_name() const override { return "NoAudio"; };
  };

}
