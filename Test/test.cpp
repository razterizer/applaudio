//
//  main.cpp
//  Test
//
//  Created by Rasmus Anthin on 2025-09-14.
//

#define _USE_MATH_DEFINES
#include <applaudio/AudioEngine.h>
#include <cmath>
#include <iostream>

//#define USE_INT16_SAMPLES

#ifdef USE_INT16_SAMPLES
#define EX_SAMPLE_TYPE short
#else
#define EX_SAMPLE_TYPE float
#endif

int test_1()
{
  std::cout << "=== Test 1 : Sine Wave ===" << std::endl;

  applaudio::AudioEngine engine;
  engine.print_backend_name();

  // --- 1. Create and start the engine ---
  int sample_rate = 44100;
  int channels = 2;
  bool request_exclusive_mode = false;
  bool verbose = true;
  if (!engine.startup(sample_rate, channels, request_exclusive_mode, verbose))
  {
    std::cerr << "Failed to start AudioEngine\n";
    return EXIT_FAILURE;
  }

  // --- 2. Create a buffer with a sine wave ---
  const double buf_frequency = 440.0; // A4
  const double buf_duration = 2.0;    // 2 seconds
  const int buf_Fs = 25'000;
  const int buf_channels = 1;
#ifdef USE_INT16_SAMPLES
  const auto buf_scale = 30'000;
#else
  const auto buf_scale = 1.f;
#endif
  size_t frame_count = static_cast<size_t>(buf_duration * buf_Fs);
  size_t cycles = static_cast<size_t>(buf_frequency * buf_duration);
  frame_count = static_cast<size_t>(cycles * buf_Fs / buf_frequency); // adjust to full cycles

  std::vector<EX_SAMPLE_TYPE> pcm_data(frame_count * buf_channels);
  for (size_t i = 0; i < frame_count; ++i)
  {
    auto sample = static_cast<EX_SAMPLE_TYPE>(std::sin(2.0 * M_PI * buf_frequency * i / buf_Fs) * buf_scale);
    for (int c = 0; c < buf_channels; ++c)
      pcm_data[i * buf_channels + c] = sample; // same sample for all channels
  }

  unsigned int buf_id = engine.create_buffer();
#ifdef USE_INT16_SAMPLES
  std::cout << "buffer bit format: uint 16 bit." << std::endl;
  engine.set_buffer_data_16s(buf_id, pcm_data, buf_channels, buf_Fs);
#else
  std::cout << "buffer bit format: float 32 bit." << std::endl;
  engine.set_buffer_data_32f(buf_id, pcm_data, buf_channels, buf_Fs);
#endif

  // --- 3. Create a source and attach buffer ---
  unsigned int src_id = engine.create_source();
  engine.attach_buffer_to_source(src_id, buf_id);
  engine.set_source_volume(src_id, 0.1f);
  engine.set_source_looping(src_id, false);
  engine.set_source_pitch(src_id, 1.f);

  // --- 4. Play the source ---
  engine.play_source(src_id);

  // --- 5. Let the engine run for a few seconds ---
  std::cout << "Playing sine wave..." << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(5));

  // --- 6. Shutdown the engine ---
  engine.stop_source(src_id);
  engine.shutdown();
  std::cout << "Done." << std::endl;
  return EXIT_SUCCESS;
}

int test_2()
{
  std::cout << "=== Test 2 : 3D sound ===" << std::endl;
  
  applaudio::AudioEngine engine;
  engine.print_backend_name();

  // --- 1. Create and start the engine ---
  int sample_rate = 44100;
  int channels = 2;
  bool request_exclusive_mode = false;
  bool verbose = true;
  if (!engine.startup(sample_rate, channels, request_exclusive_mode, verbose))
  {
    std::cerr << "Failed to start AudioEngine\n";
    return EXIT_FAILURE;
  }

  // --- 2. Create a buffer with a sine wave ---
  const double buf_frequency = 440.f;
  const double buf_duration = 2.0;    // 2 seconds
  const int buf_Fs = 23'700;
  const int buf_channels = 1;
#ifdef USE_INT16_SAMPLES
  const auto buf_scale = 30'000;
#else
  const auto buf_scale = 1.f;
#endif
  size_t frame_count = static_cast<size_t>(buf_duration * buf_Fs);
  size_t cycles = static_cast<size_t>(buf_frequency * buf_duration);
  frame_count = static_cast<size_t>(cycles * buf_Fs / buf_frequency); // adjust to full cycles

  std::vector<EX_SAMPLE_TYPE> pcm_data(frame_count * buf_channels);
  for (size_t i = 0; i < frame_count; ++i)
  {
    auto sample = static_cast<EX_SAMPLE_TYPE>(std::sin(2.0 * M_PI * buf_frequency * i / buf_Fs) * buf_scale);
    for (int c = 0; c < buf_channels; ++c)
      pcm_data[i * buf_channels + c] = sample; // same sample for all channels
  }

  unsigned int buf_id = engine.create_buffer();
#ifdef USE_INT16_SAMPLES
  std::cout << "buffer bit format: uint 16 bit." << std::endl;
  engine.set_buffer_data_16s(buf_id, pcm_data, buf_channels, buf_Fs);
#else
  std::cout << "buffer bit format: float 32 bit." << std::endl;
  engine.set_buffer_data_32f(buf_id, pcm_data, buf_channels, buf_Fs);
#endif

  // --- 3. Setup 3D environment ---
  engine.init_3d_scene(343.f); // speed_of_sound ~= 343 m/s.

  // --- 4a. Create a source and attach buffer ---
  unsigned int src_id = engine.create_source();
  engine.attach_buffer_to_source(src_id, buf_id);
  engine.set_source_volume(src_id, 1.f);
  engine.set_source_looping(src_id, true);
  engine.set_source_pitch(src_id, 1.f);
  
  // --- 4b. Set source 3D properties ---
  engine.enable_source_3d_audio(src_id, true);
  la::Mtx4 trf_s = la::look_at({ 7.f, 5.5f, -3.2f }, { 0.f, 0.f, 0.f }, { 0.f, 1.f, 0.f }); // Source world position encoded here.
  la::Vec3 pos_l_s = la::Vec3_Zero; // Channel emitter local positions encoded here.
  la::Vec3 vel_w_s { -1.2f, -0.3f, 0.f }; // Channel emitter world velicities encoded here.
  vel_w_s *= 5.f;
  engine.set_source_channel_3d_state(src_id, 0, trf_s.get_rot_matrix(), trf_s.transform_pos(pos_l_s), vel_w_s);
  
  // --- 4c. Set listener 3D properties ---
  la::Mtx4 trf_l = la::Mtx4_Identity;
  trf_l.set_column_vec(la::W, la::Vec3_Zero); // Source world position encoded here.
  la::Vec3 pos_l_L { -0.12f, 0.05f, -0.05f }; // Channel Left emitter local position encoded here.
  la::Vec3 pos_l_R { +0.12f, 0.05f, -0.05f }; // Channel Left emitter local position encoded here.
  la::Vec3 vel_w_L = la::Vec3_Zero; // Channel Right emitter world velocity encoded here.
  la::Vec3 vel_w_R = la::Vec3_Zero; // Channel Right emitter world velocity encoded here.
  engine.set_listener_channel_3d_state(0, trf_l.get_rot_matrix(), trf_l.transform_pos(pos_l_L), vel_w_L);
  engine.set_listener_channel_3d_state(1, trf_l.get_rot_matrix(), trf_l.transform_pos(pos_l_R), vel_w_R);
  
  // --- 4d. Set falloff properties ---
  engine.set_attenuation_constant_falloff(1.f);
  engine.set_attenuation_linear_falloff(0.2f);
  engine.set_attenuation_quadratic_falloff(0.08f);

  // --- 5. Play the source ---
  engine.play_source(src_id);

  // --- 6. Let the engine run for a few seconds. Move source ---
  std::cout << "Playing 3D sine wave..." << std::endl;
  double animation_duration = 10.0;
  int num_iters = 500;
  double dt = animation_duration / num_iters;
  auto start_time = std::chrono::steady_clock::now();
  auto next_update = start_time;
  for (int i = 0; i < num_iters; ++i)
  {
    la::Vec3 trf_pos;
    trf_s.get_column_vec(la::W, trf_pos);
    trf_pos += vel_w_s * dt;
    trf_s.set_column_vec(la::W, trf_pos);
    engine.set_source_channel_3d_state(src_id, 0, trf_s.get_rot_matrix(), trf_s.transform_pos(pos_l_s), vel_w_s);
    next_update += std::chrono::milliseconds(static_cast<int>(dt * 1000));
    std::this_thread::sleep_until(next_update);
  }

  // --- 7. Shutdown the engine ---
  engine.stop_source(src_id);
  engine.shutdown();
  std::cout << "Done." << std::endl;
  return EXIT_SUCCESS;
}

int main(int argc, const char* argv[])
{
  if (test_1() == EXIT_FAILURE)
    return EXIT_FAILURE;
  if (test_2() == EXIT_FAILURE)
    return EXIT_FAILURE;
    
  return EXIT_SUCCESS;
}
