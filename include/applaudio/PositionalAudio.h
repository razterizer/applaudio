//
//  PositionalAudio.h
//  applaudio
//
//  Created by Rasmus Anthin on 2025-10-04.
//

#pragma once
#include "Source.h"
#include "Listener.h"
#include <optional>
#include <unordered_map>
#include <algorithm>
#include <cassert>


namespace applaudio
{
  
  namespace a3d
  {
        
    class PositionalAudio
    {
    
      inline constexpr float attenuate(const Source& src, float d)
      {
        return 1.f / (src.constant_attenuation + src.linear_attenuation * d + src.quadratic_attenuation * d * d);
      }
      
      bool reset_attenuation_at_min_dist(Source& src)
      {
        src.attenuation_at_min_dist = attenuate(src, src.min_attenuation_distance);
        
        const float c_min_attenuation = 1e-6f;
        const float c_max_attenuation = 1e6f;
        assert(std::isfinite(src.attenuation_at_min_dist) && "ERROR: Attenuation gain at min distance limit is not finite.");
        if (!std::isfinite(src.attenuation_at_min_dist))
          return false;
        assert(src.attenuation_at_min_dist > c_min_attenuation && "ERROR: Attenuation gain at min distance limit is too small and will lead to precision issues. Consider decreasing the min distance or fall-off params.");
        assert(src.attenuation_at_min_dist < c_max_attenuation && "ERROR: Attenuation gain at min distance limit is too large and will lead to precision issues. Consider increasing the min distance or fall-off params.");
        src.attenuation_at_min_dist = std::clamp(src.attenuation_at_min_dist, c_min_attenuation, c_max_attenuation);
        return true;
      }
      
    public:
      PositionalAudio() = default;
      
      void update_scene(Listener& listener, std::unordered_map<unsigned int, Source>& source_vec)
      {
        const int n_ch_l = listener.object_3d.num_channels();
      
        for (auto& [src_id, src] : source_vec)
        {
          const int n_ch_s = src.object_3d.num_channels();
          for (int ch_s = 0; ch_s < n_ch_s; ++ch_s)
          {
            auto* state_s = src.object_3d.get_channel_state(ch_s);
            state_s->listener_ch_params.resize(n_ch_l);
          }
        
          for (int ch_l = 0; ch_l < n_ch_l; ++ch_l)
          {
            const auto* state_l = listener.object_3d.get_channel_state(ch_l);
            la::Vec3 forward_l = listener.object_3d.dir_forward(ch_l);
            la::Vec3 right_l   = listener.object_3d.dir_right(ch_l);
            for (int ch_s = 0; ch_s < n_ch_s; ++ch_s)
            {
              auto* state_s = src.object_3d.get_channel_state(ch_s);
              
              auto dir = state_s->pos_world - state_l->pos_world;
              if (dir.length_squared() < 1e-9f)
                continue;
              
              auto dir_un = la::normalize(dir);
              // dir_un points FROM listener TO source.
              // But for Doppler, we want the direction FROM source TO listener.
              la::Vec3 dir_source_to_listener = -dir_un;
              float vLs = la::dot(state_l->vel_world, dir_source_to_listener); // Listener’s velocity along LOS.
              float vSs = la::dot(state_s->vel_world, dir_source_to_listener); // Source’s velocity along LOS.
              
              const float c = src.speed_of_sound;
              
              // Doppler
              float doppler_shift = 1.f;
              if (c > 0.f)
              {
                doppler_shift = (c + vLs) / (c - vSs);
                doppler_shift = std::clamp(doppler_shift, 0.25f, 4.f);
              }
              
              // Vector from listener to source
              float dist = dir.length();
              if (dist < 1e-6f)
                dist = 1e-6f;
              
              // Distance attenuation
              float distance_gain = 1.f;
              if (dist < src.min_attenuation_distance)
                distance_gain = 1.f;
              else if (src.min_attenuation_distance <= dist && dist < src.max_attenuation_distance)
                distance_gain = attenuate(src, dist) / src.attenuation_at_min_dist;
              else
                distance_gain = attenuate(src, src.max_attenuation_distance) / src.attenuation_at_min_dist;
              
              // --- Directional Panning (listener ears) ---
              float pan = la::dot(right_l, dir_un); // -1=left, +1=right
              float listener_pan_weight = 1.f;
              if (n_ch_l >= 2)
              {
                if (ch_l == 0)
                  listener_pan_weight = 0.5f*(1.0f - pan); // left ear
                else if (ch_l == 1)
                  listener_pan_weight = 0.5f*(1.0f + pan); // right ear
              }
              
              // --- Optional source directivity ---
              la::Vec3 forward_s = src.object_3d.dir_forward(ch_s);
              float src_cos_angle = la::dot(forward_s, -dir_un);
              float pattern = 1.f; // Function of cos angle.
              switch (src.directivity_type)
              {
                case DirectivityType::Cardioid:
                  // Classic front-lobe cardioid: D(θ) = 0.5 * (1 + cosθ).
                  pattern = 0.5f * (1.0f + src_cos_angle); // ranges [0.5, 1.0].
                  break;
                case DirectivityType::SuperCardioid:
                  // Tighter front lobe, slight rear lobe: D(θ) = 0.25 + 0.75 * cosθ.
                  pattern = 0.25f + 0.75f * src_cos_angle;
                  break;
                case DirectivityType::HalfRectifiedDipole:
                  pattern = std::max(src_cos_angle, 0.f);
                  break;
                case DirectivityType::Dipole:
                  pattern = std::abs(src_cos_angle);
                  break;
              }
              float source_directivity_weight = std::lerp(1.f, pattern, src.directivity_alpha); // a, b, t.
              source_directivity_weight = std::pow(std::clamp(source_directivity_weight, 0.f, 1.f),
                                                   src.directivity_sharpness);
              
              // --- Listener front/rear muffling ---
              float src_rear = src.rear_attenuation;     // 0..1
              float lst_rear = listener.rear_attenuation; // 0..1
              float frontness = la::dot(forward_l, dir_un); // 1 = front, -1 = behind.
              float t_rear = std::clamp(0.5f * (1.0f + frontness), 0.f, 1.f);
              float rear_weight = std::lerp(src_rear * lst_rear, 1.f, std::pow(t_rear, 0.7f)); // a, b, t.
              
              // --- Final gain ---
              float gain = distance_gain * listener_pan_weight * source_directivity_weight * rear_weight;
              gain = std::clamp(gain, 0.f, 1.f); // Perhaps better to compress/normalize at mix-level later instead of clamping here.
              
              state_s->listener_ch_params[ch_l] = { gain, doppler_shift };
            }
          }
        }
      }
      
      bool set_attenuation_min_distance(Source& src, float min_dist)
      {
        assert(std::isfinite(min_dist) && "ERROR: min_dist is not finite.");
        if (!std::isfinite(min_dist))
          return false;
        assert(min_dist > 0.f && "ERROR: min_dist is negative. min_dist must be positive.");
        if (min_dist <= 0.f)
          return false;
        min_dist = std::max(min_dist, 1e-9f);
        src.min_attenuation_distance = min_dist;
        
        return reset_attenuation_at_min_dist(src);
      }
      
      float get_attenuation_min_distance(const Source& src) const
      {
        return src.min_attenuation_distance;
      }
      
      bool set_attenuation_max_distance(Source& src, float max_dist)
      {
        src.max_attenuation_distance = std::max(max_dist, src.min_attenuation_distance);
        
        return reset_attenuation_at_min_dist(src);
      }
      
      float get_attenuation_max_distance(const Source& src) const
      {
        return src.max_attenuation_distance;
      }
      
      bool set_attenuation_constant_falloff(Source& src, float const_falloff)
      {
        src.constant_attenuation = const_falloff;
        
        return reset_attenuation_at_min_dist(src);
      }
      
      float get_attenuation_constant_falloff(const Source& src) const
      {
        return src.constant_attenuation;
      }
      
      bool set_attenuation_linear_falloff(Source& src, float lin_falloff)
      {
        src.linear_attenuation = lin_falloff;
        
        return reset_attenuation_at_min_dist(src);
      }
      
      float get_attenuation_linear_falloff(const Source& src) const
      {
        return src.linear_attenuation;
      }
      
      bool set_attenuation_quadratic_falloff(Source& src, float sq_falloff)
      {
        src.quadratic_attenuation = sq_falloff;
        
        return reset_attenuation_at_min_dist(src);
      }
      
      float get_attenuation_quadratic_falloff(const Source& src) const
      {
        return src.quadratic_attenuation;
      }

    };
    
  }

}
