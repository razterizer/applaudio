//
//  Object3D.h
//  applaudio
//
//  Created by Rasmus Anthin on 2025-10-04.
//

#pragma once
#include "LinAlg.h"
#include <vector>

namespace applaudio
{

  namespace a3d
  {
    
    struct Param3D
    {
      float gain = 1.f;
      float doppler_shift = 1.f;
    };
    
    struct State3D
    {
      la::Mtx3 rot_mtx;
      la::Vec3 pos_world;
      la::Vec3 vel_world;
      std::vector<Param3D> listener_ch_params;
    };
    
    class Object3D
    {
      std::vector<State3D> channel_state;
      bool audio_3d_enabled = false;
      
      template <typename VecT>
      static auto get_state_impl(VecT& channel_state, int ch) -> decltype(&channel_state[0])
      {
        if (channel_state.empty())
          return nullptr;
        
        if (channel_state.size() == 1)
          return &channel_state[0];
        
        if (ch < static_cast<int>(channel_state.size()))
          return &channel_state[ch];
        
        return &channel_state.front();
      }
      
    public:
      
      bool set_channel_state(int ch, const la::Mtx3& rot_mtx, const la::Vec3& pos_world, const la::Vec3& vel_world)
      {
        const int n_ch = num_channels();
        if (0 <= ch && ch < n_ch)
        {
          auto* state = get_channel_state(ch);
          state->rot_mtx = rot_mtx;
          state->pos_world = pos_world;
          state->vel_world = vel_world;
          return true;
        }
        return false;
      }
      
      State3D* get_channel_state(int ch)
      {
        return get_state_impl(channel_state, ch);
      }
      
      const State3D* get_channel_state(int ch) const
      {
        return get_state_impl(channel_state, ch);
      }
      
      bool using_3d_audio() const { return audio_3d_enabled; }
      void enable_3d_audio(bool enable) { audio_3d_enabled = enable; }
      
      int num_channels() const { return static_cast<int>(channel_state.size()); }
      void set_num_channels(int num_ch) { channel_state.resize(num_ch); }
      
      const la::Vec3 dir_right(int ch) const
      {
        const int n_ch = num_channels();
        if (0 <= ch && ch < n_ch)
        {
          la::Vec3 vec_right;
          auto* state = get_channel_state(ch);
          state->rot_mtx.get_column_vec(la::X, vec_right);
          return vec_right;
        }
        return la::Vec3_Zero;
      }
      
      const la::Vec3 dir_up(int ch) const
      {
        const int n_ch = num_channels();
        if (0 <= ch && ch < n_ch)
        {
          la::Vec3 vec_up;
          auto* state = get_channel_state(ch);
          state->rot_mtx.get_column_vec(la::Y, vec_up);
          return vec_up;
        }
        return la::Vec3_Zero;
      }
      
      const la::Vec3 dir_forward(int ch) const
      {
        const int n_ch = num_channels();
        if (0 <= ch && ch < n_ch)
        {
          la::Vec3 vec_backward;
          auto* state = get_channel_state(ch);
          state->rot_mtx.get_column_vec(la::Z, vec_backward);
          return -vec_backward;
        }
        return la::Vec3_Zero;
      }
    };
    
  }

}
