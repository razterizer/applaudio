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

  enum class DirectivityType { Cardioid, SuperCardioid, HalfRectifiedDipole, Dipole };

  struct Source
  {
    unsigned int buffer_id = 0;
    bool looping = false;
    float gain = 1.0f;
    float vol_gain = 1.0f;
    float pitch = 1.0f;
    bool playing = false;
    bool paused = false;
    double play_pos = 0;
    std::optional<float> pan = std::nullopt;
    
    a3d::Object3D object_3d;
    
    float speed_of_sound = 0.f;
    
    float constant_attenuation = 1.f;
    float linear_attenuation = 0.2f;
    float quadratic_attenuation = 0.08f;
    float min_attenuation_distance = 1.f;
    float max_attenuation_distance = 500.f;
    float attenuation_at_min_dist = 1.f; // Replaced in set_attenuation_min_distance().
    
    float directivity_alpha = 0.f; // [0, 1].
    float directivity_sharpness = 1.f; // [1, 8].
    DirectivityType directivity_type = DirectivityType::Cardioid;
    float rear_attenuation = 1.f; // [0, 1].
  };

}
