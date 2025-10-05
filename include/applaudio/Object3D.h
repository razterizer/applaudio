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
      la::Vec3 pos_local;
      la::Vec3 pos_world; // Cached.
      la::Vec3 vel_world;
      std::vector<Param3D> listener_ch_params;
    };
    
    class Object3D
    {
      la::Mtx4 trf;
      
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
      
      void update(const la::Mtx4& new_trf,
                  const la::Vec3& pos_local_left, const la::Vec3& vel_world_left, // mono | stereo left
                  const la::Vec3& pos_local_right = la::Vec3_Zero, const la::Vec3& vel_world_right = la::Vec3_Zero) // stereo right
      {
        trf = new_trf;
        const int n_ch = num_channels();
        for (int ch = 0; ch < n_ch; ++ch)
        {
          auto* state = get_state(ch);
          state->pos_local = ch == 0 ? pos_local_left : pos_local_right;
          state->pos_world = trf.transform_pos(state->pos_local);
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
    };
    
  }

}
