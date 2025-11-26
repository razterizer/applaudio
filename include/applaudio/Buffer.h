//
//  Buffer.h
//  applaudio
//
//  Created by Rasmus Anthin on 2025-10-04.
//

#pragma once
#include "defines.h"
#include <vector>

namespace applaudio
{
  
  struct Buffer
  {
    std::vector<APL_SAMPLE_TYPE> data;
    int channels = 0;
    int sample_rate = 0;
  };
  
}
