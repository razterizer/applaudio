//
//  Source.h
//  applaudio
//
//  Created by Rasmus Anthin on 2025-10-04.
//

#pragma once
#include "Object3D.h"

namespace applaudio
{

  struct Source
  {
    unsigned int buffer_id = 0;
    bool looping = false;
    float volume = 1.0f;
    float pitch = 1.0f;
    bool playing = false;
    bool paused = false;
    double play_pos = 0;
    
    a3d::Object3D object_3d;
  };

}
