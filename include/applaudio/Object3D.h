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
      la::Vec3 pos_world;
      la::Vec3 vel_world;
      std::vector<Param3D> listener_ch_params;
    };
    
    class Object3D
    {
      la::Mtx3 m_rot_mtx;
      
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
      
      void update(const la::Mtx3& rot_mtx,
                  const la::Vec3& pos_world_left, const la::Vec3& vel_world_left, // mono | stereo left
                  const la::Vec3& pos_world_right = la::Vec3_Zero, const la::Vec3& vel_world_right = la::Vec3_Zero) // stereo right
      {
        m_rot_mtx = rot_mtx;
        const int n_ch = num_channels();
        for (int ch = 0; ch < n_ch; ++ch)
        {
          auto* state = get_state(ch);
          state->pos_world = ch == 0 ? pos_world_left : pos_world_right;
          state->vel_world = ch == 0 ? vel_world_left : vel_world_right;
        }
      }
      
      State3D* get_state(int ch)
      {
        return get_state_impl(channel_state, ch);
      }
      
      const State3D* get_state(int ch) const
      {
        return get_state_impl(channel_state, ch);
      }
      
      bool using_3d_audio() const { return audio_3d_enabled; }
      void enable_3d_audio(bool enable) { audio_3d_enabled = enable; }
      
      int num_channels() const { return static_cast<int>(channel_state.size()); }
      void set_num_channels(int num_ch) { channel_state.resize(num_ch); }
      
      const la::Vec3 dir_right() const
      {
        la::Vec3 vec_right;
        m_rot_mtx.get_column_vec(la::X, vec_right);
        return vec_right;
      }
      
      const la::Vec3 dir_up() const
      {
        la::Vec3 vec_up;
        m_rot_mtx.get_column_vec(la::Y, vec_up);
        return vec_up;
      }
      
      const la::Vec3 dir_forward() const
      {
        la::Vec3 vec_backward;
        m_rot_mtx.get_column_vec(la::Z, vec_backward);
        return -vec_backward;
      }
    };
    
  }

}
