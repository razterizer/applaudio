//
//  AudioEngine.h
//  applaudio
//
//  Created by Rasmus Anthin on 2025-09-14.
//

#pragma once
#include "MacOS_Internal.h"
#include <memory>


namespace applaudio
{

  class AudioEngine
  {
    std::unique_ptr<IApplaudio_Internal> device;
    
  public:
    AudioEngine()
    {
#if defined(_WIN32)
#elif defined(__APPLE__)
      device = std::make_unique<MacOS_Internal>();
#elif defined(__linux__)
#endif
    }
    
  };

}
