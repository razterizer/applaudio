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

namespace applaudio
{
  
  namespace a3d
  {
    
    enum class LengthUnit { MilliMeter = 0, CentiMeter = 1, DeciMeter = 2, Meter = 3, KiloMeter = 6 };
    
    class PositionalAudio
    {
      float m_speed_of_sound = 343.f;
      LengthUnit m_global_length_unit = LengthUnit::Meter;
      float constant_attenuation = 1.f;
      float linear_attenuation = 0.2f;
      float quadratic_attenuation = 0.08f;
      float min_attenuation_distance = 1.f;
      float max_attenuation_distance = 500.f;
      float attenuation_at_min_dist = 1.f; // Replaced in set_attenuation_min_distance().
      
      static int pow10(int p)
      {
        int r = 1;
        while (p-- > 0)
          r *= 10;
        return r;
      }
      
      float convert_length(float value, std::optional<LengthUnit> lu_from, std::optional<LengthUnit> lu_to)
      {
        if (!lu_from.has_value() || !lu_to.has_value())
          return value; // Nothing to convert from or to.
      
        // e.g. m -> mm : value * 1000
        auto idx_from = static_cast<int>(lu_from.value());
        auto idx_to = static_cast<int>(lu_to.value());
        if (idx_from > idx_to)
        {
          int pot = idx_from - idx_to; // m(3) - mm(0) = 3
          int ratio = pow10(pot);
          return value * static_cast<float>(ratio);
        }
        else if (idx_to > idx_from)
        {
          int pot = idx_to - idx_from; // m(3) - mm(0) = 3
          int ratio = pow10(pot);
          return value / static_cast<float>(ratio);
        }
        //else if (idx_to == idx_from)
        return value;
      }
      
      la::Vec3 convert_length(const la::Vec3& vec, std::optional<LengthUnit> lu_from, std::optional<LengthUnit> lu_to)
      {
        return { convert_length(vec.x(), lu_from, lu_to),
                 convert_length(vec.y(), lu_from, lu_to),
                 convert_length(vec.z(), lu_from, lu_to) };
      }
      
      inline constexpr float attenuate(float d)
      {
        return 1.f / (constant_attenuation + linear_attenuation * d + quadratic_attenuation * d * d);
      }
      
    public:
      PositionalAudio(LengthUnit global_length_unit)
        : m_global_length_unit(global_length_unit) // Don't want to have to rescale variables later.
      {}
    
      LengthUnit get_global_length_unit() const { return m_global_length_unit; }
    
      void set_speed_of_sound(float speed_of_sound, std::optional<LengthUnit> length_unit = std::nullopt)
      {
        m_speed_of_sound = convert_length(speed_of_sound, length_unit, m_global_length_unit);
      }
      
      float get_speed_of_sound(std::optional<LengthUnit> length_unit = std::nullopt)
      {
        return convert_length(m_speed_of_sound, m_global_length_unit, length_unit);
      }
      
      void update_obj(Object3D& obj, const la::Mtx4& new_trf, const la::Vec3& pos_local_left, const la::Vec3& vel_world_left, // mono | stereo left
                      const la::Vec3& pos_local_right = la::Vec3_Zero, const la::Vec3& vel_world_right = la::Vec3_Zero, // stereo right
                      std::optional<LengthUnit> length_unit = std::nullopt)
      {
        auto pos_left_llu = convert_length(pos_local_left, length_unit, m_global_length_unit);
        auto pos_right_llu = convert_length(pos_local_right, length_unit, m_global_length_unit);
        auto vel_left_wlu = convert_length(vel_world_left, length_unit, m_global_length_unit);
        auto vel_right_wlu = convert_length(vel_world_right, length_unit, m_global_length_unit);
        auto trf_lu = new_trf;
        la::Vec3 pos;
        if (trf_lu.get_column_vec(la::W, pos))
        {
          auto pos_lu = convert_length(pos, length_unit, m_global_length_unit);
          if (trf_lu.set_column_vec(la::W, pos_lu))
            obj.update(trf_lu,
                       pos_left_llu, vel_left_wlu,
                       pos_right_llu, vel_right_wlu);
        }
      }
      
      void update_scene(Listener& listener, std::unordered_map<unsigned int, Source>& source_vec)
      {
        const int n_ch_l = listener.object_3d.num_channels();
        la::Vec3 forward_l = listener.object_3d.dir_forward();
        la::Vec3 right_l   = listener.object_3d.dir_right();
      
        for (auto& [src_id, src] : source_vec)
        {
          const int n_ch_s = src.object_3d.num_channels();
          for (int ch_s = 0; ch_s < n_ch_s; ++ch_s)
          {
            auto* state_s = src.object_3d.get_state(ch_s);
            state_s->listener_ch_params.resize(n_ch_l);
          }
        
          for (int ch_l = 0; ch_l < n_ch_l; ++ch_l)
          {
            const auto* state_l = listener.object_3d.get_state(ch_l);
            for (int ch_s = 0; ch_s < n_ch_s; ++ch_s)
            {
              auto* state_s = src.object_3d.get_state(ch_s);
              
              auto dir = state_s->pos_world - state_l->pos_world;
              if (dir.length_squared() < 1e-9f)
                continue;
              
              auto dir_un = la::normalize(dir);
              // dir_un points FROM listener TO source.
              // But for Doppler, we want the direction FROM source TO listener.
              la::Vec3 dir_source_to_listener = -dir_un;
              float vLs = la::dot(state_l->vel_world, dir_source_to_listener); // Listener’s velocity along LOS.
              float vSs = la::dot(state_s->vel_world, dir_source_to_listener); // Source’s velocity along LOS.
              
              const float c = m_speed_of_sound;
              
              // Doppler
              float doppler_shift = (c + vLs) / (c - vSs);
              doppler_shift = std::clamp(doppler_shift, 0.25f, 4.f);
              
              // Vector from listener to source
              float dist = dir.length();
              if (dist < 1e-6f)
                dist = 1e-6f;
              
              // Distance attenuation
              float distance_gain = 1.f;
              if (dist < min_attenuation_distance)
                distance_gain = 1.f;
              else if (min_attenuation_distance <= dist && dist < max_attenuation_distance)
                distance_gain = attenuate(dist) / attenuation_at_min_dist;
              else
                distance_gain = attenuate(max_attenuation_distance) / attenuation_at_min_dist;
              
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
              la::Vec3 forward_s = src.object_3d.dir_forward();
              float src_cos_angle = la::dot(forward_s, -dir_un);
              float source_directivity_weight = std::pow(std::clamp(src_cos_angle, 0.f, 1.f), 2.f);
              
              float frontness = la::dot(forward_l, dir_un); // 1 = front, -1 = behind
              float rear_weight = std::clamp(0.5f * (1.0f + frontness), 0.2f, 1.0f); // muffle rear slightly
              
              // Final gain
              float gain = distance_gain * listener_pan_weight * source_directivity_weight * rear_weight;
              gain = std::clamp(gain, 0.f, 1.f);
              
              state_s->listener_ch_params[ch_l] = { gain, doppler_shift };
            }
          }
        }
      }
      
      bool set_attenuation_min_distance(float min_dist)
      {
        assert(std::isfinite(min_dist) && "ERROR: min_dist is not finite.");
        if (!std::isfinite(min_dist))
          return false;
        assert(min_dist > 0.f && "ERROR: min_dist is negative. min_dist must be positive.");
        if (min_dist <= 0.f)
          return false;
        min_dist = std::max(min_dist, 1e-9f);
        min_attenuation_distance = min_dist;
        
        attenuation_at_min_dist = attenuate(min_attenuation_distance);
        
        const float c_min_attenuation = 1e-6f;
        const float c_max_attenuation = 1e6f;
        assert(std::isfinite(attenuation_at_min_dist) && "ERROR: Attenuation gain at min distance limit is not finite.");
        if (!std::isfinite(attenuation_at_min_dist))
          return false;
        assert(attenuation_at_min_dist > c_min_attenuation && "ERROR: Attenuation gain at min distance limit is too small and will lead to precision issues. Consider decreasing the min distance or fall-off params.");
        assert(attenuation_at_min_dist < c_max_attenuation && "ERROR: Attenuation gain at min distance limit is too large and will lead to precision issues. Consider increasing the min distance or fall-off params.");
        attenuation_at_min_dist = std::clamp(attenuation_at_min_dist, c_min_attenuation, c_max_attenuation);
        return true;
      }
      
      void set_attenuation_max_distance(float max_dist)
      {
        max_attenuation_distance = std::max(max_dist, min_attenuation_distance);
      }
      
      void set_attenuation_constant_falloff(float const_falloff)
      {
        constant_attenuation = const_falloff;
      }
      
      void set_attenuation_linear_falloff(float lin_falloff)
      {
        linear_attenuation = lin_falloff;
      }
      
      void set_attenuation_quadratic_falloff(float sq_falloff)
      {
        quadratic_attenuation = sq_falloff;
      }

    };
    
  }

}
