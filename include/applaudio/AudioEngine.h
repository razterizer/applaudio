//
//  AudioEngine.h
//  applaudio
//
//  Created by Rasmus Anthin on 2025-09-14.
//

#pragma once
#include "MacOS_Internal.h"
#include "Linux_Internal.h"
#include "Win_Internal.h"
#include <memory>
#include <iostream>


namespace applaudio
{

  class AudioEngine
  {
    std::unique_ptr<IApplaudio_Internal> device;
    
  public:
    AudioEngine()
    {
#if defined(_WIN32)
      device = std::make_unique<Win_Internal>();
#elif defined(__APPLE__)
      device = std::make_unique<MacOS_Internal>();
#elif defined(__linux__)
      device = std::make_unique<Linux_Internal>();
#endif
    }
    
    void print_device_name() const
    {
      if (device != nullptr)
        std::cout << device->device_name() << std::endl;
      else
        std::cout << "Unknown device" << std::endl;
    }
    
  };

}
