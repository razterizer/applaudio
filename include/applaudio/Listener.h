//
//  Listener.h
//  applaudio
//
//  Created by Rasmus Anthin on 2025-10-04.
//

#pragma once
#include "Object3D.h"

namespace applaudio
{

  struct Listener
  {
    a3d::Object3D object_3d;
    
    float rear_attenuation = 0.8f; // [0, 1].
  };
  
}
