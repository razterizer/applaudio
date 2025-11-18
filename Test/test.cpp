//
//  main.cpp
//  Test
//
//  Created by Rasmus Anthin on 2025-09-14.
//

#define _USE_MATH_DEFINES
#include <applaudio/applaudio.h>
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
  engine.set_source_gain(src_id, 0.1f);
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
  std::cout << "=== Test 2 : 3D sound : Passing Mono Buffer Source ===" << std::endl;
  
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
  engine.init_3d_scene(); // speed_of_sound ~= 343 m/s.

  // --- 4a. Create a source and attach buffer ---
  unsigned int src_id = engine.create_source();
  engine.attach_buffer_to_source(src_id, buf_id);
  engine.set_source_gain(src_id, 0.8f);
  engine.set_source_looping(src_id, true);
  engine.set_source_pitch(src_id, 1.f);
  
  // --- 4b. Set source 3D properties ---
  engine.enable_source_3d_audio(src_id, true);
  // world coordsys:
  //   x : right.
  //   y : up.
  //   z : towards viewer.
  la::Mtx4 trf_s = la::look_at({ 7.f, 5.5f, -3.2f }, { 0.f, 0.f, 0.f }, { 0.f, 1.f, 0.f }); // Source world position encoded here.
  la::Vec3 pos_l_s = la::Vec3_Zero; // Channel emitter local positions encoded here.
  la::Vec3 vel_w_s { -1.2f, -0.3f, 0.f }; // Channel emitter world velicity encoded here.
  la::Vec3 ang_vel_w_s = la::Vec3_Zero;
  vel_w_s *= 5.f;
  engine.set_source_3d_state(src_id, trf_s, vel_w_s, ang_vel_w_s, { pos_l_s });
  engine.set_source_coordsys_convention(src_id, applaudio::a3d::CoordSysConvention::RH_XLeft_YUp_ZForward);
  
  // --- 4c. Set listener 3D properties ---
  // world coordsys:
  //   x : right.
  //   y : up.
  //   z : towards viewer.
  la::Mtx4 trf_l = la::Mtx4_Identity;
  trf_l.set_column_vec(la::W, la::Vec3_Zero); // Source world position encoded here.
  la::Vec3 pos_l_L_l { -0.12f, 0.05f, -0.05f }; // Channel Left emitter local position encoded here.
  la::Vec3 pos_l_R_l { +0.12f, 0.05f, -0.05f }; // Channel Right emitter local position encoded here.
  la::Vec3 vel_w_l = la::Vec3_Zero; // Lisitener world velocity encoded here.
  la::Vec3 ang_vel_w_l = la::Vec3_Zero;
  engine.set_listener_3d_state(trf_l, vel_w_l, ang_vel_w_l, { pos_l_L_l, pos_l_R_l });
  engine.set_listener_coordsys_convention(applaudio::a3d::CoordSysConvention::RH_XRight_YUp_ZBackward);
  
  // --- 4d. Set doppler and falloff properties ---
  engine.set_source_speed_of_sound(src_id, 343.f);
  engine.set_source_attenuation_constant_falloff(src_id, 1.f);
  engine.set_source_attenuation_linear_falloff(src_id, 0.2f);
  engine.set_source_attenuation_quadratic_falloff(src_id, 0.08f);

  // --- 5. Play the source ---
  engine.play_source(src_id);

  // --- 6. Let the engine run for a few seconds. Move source ---
  std::cout << "Playing 3D sine wave..." << std::endl;
  double animation_duration = 3.0;
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
    engine.set_source_3d_state(src_id, trf_s, vel_w_s, ang_vel_w_s, { pos_l_s });
    next_update += std::chrono::milliseconds(static_cast<int>(dt * 1000));
    std::this_thread::sleep_until(next_update);
  }

  // --- 7. Shutdown the engine ---
  engine.stop_source(src_id);
  engine.shutdown();
  std::cout << "Done." << std::endl;
  return EXIT_SUCCESS;
}

int test_3()
{
  std::cout << "=== Test 3 : 3D sound : Passing Stereo Buffer Source that Pans ===" << std::endl;
  
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
  const int buf_channels = 2;
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
  engine.init_3d_scene(); // speed_of_sound ~= 343 m/s.

  // --- 4a. Create a source and attach buffer ---
  unsigned int src_id = engine.create_source();
  engine.attach_buffer_to_source(src_id, buf_id);
  engine.set_source_gain(src_id, 0.8f);
  engine.set_source_looping(src_id, true);
  engine.set_source_pitch(src_id, 1.f);
  
  // --- 4b. Set source 3D properties ---
  engine.enable_source_3d_audio(src_id, true);
  la::Mtx4 trf_s = la::look_at({ 7.f, 5.5f, -3.2f }, { 0.f, 0.f, 0.f }, { 0.f, 1.f, 0.f }); // Source world position encoded here.
  la::Vec3 pos_l_L_s = { +2.f, 0.f, 0.f }; // Channel emitter local positions encoded here.
  la::Vec3 pos_l_R_s = { -2.f, 0.f, 0.f }; // Channel emitter local positions encoded here.
  la::Vec3 vel_w_s { -1.2f, -0.3f, 0.f }; // Channel emitter world velicity encoded here.
  la::Vec3 ang_vel_w_s = la::Vec3_Zero;
  vel_w_s *= 5.f;
  engine.set_source_3d_state(src_id, trf_s, vel_w_s, ang_vel_w_s, { pos_l_L_s, pos_l_R_s });
  engine.set_source_coordsys_convention(src_id, applaudio::a3d::CoordSysConvention::RH_XLeft_YUp_ZForward);
  
  // --- 4c. Set listener 3D properties ---
  la::Mtx4 trf_l = la::Mtx4_Identity;
  trf_l.set_column_vec(la::W, la::Vec3_Zero); // Source world position encoded here.
  la::Vec3 pos_l_L_l { -0.12f, 0.05f, -0.05f }; // Channel Left emitter local position encoded here.
  la::Vec3 pos_l_R_l { +0.12f, 0.05f, -0.05f }; // Channel Right emitter local position encoded here.
  la::Vec3 vel_w_l = la::Vec3_Zero; // Lisitener world velocity encoded here.
  la::Vec3 ang_vel_w_l = la::Vec3_Zero;
  engine.set_listener_3d_state(trf_l, vel_w_l, ang_vel_w_l, { pos_l_L_l, pos_l_R_l });
  engine.set_listener_coordsys_convention(applaudio::a3d::CoordSysConvention::RH_XRight_YUp_ZBackward);
  
  // --- 4d. Set doppler and falloff properties ---
  engine.set_source_speed_of_sound(src_id, 343.f);
  engine.set_source_attenuation_constant_falloff(src_id, 1.f);
  engine.set_source_attenuation_linear_falloff(src_id, 0.2f);
  engine.set_source_attenuation_quadratic_falloff(src_id, 0.08f);

  // --- 5. Play the source ---
  engine.play_source(src_id);

  // --- 6. Let the engine run for a few seconds. Move source ---
  std::cout << "Playing 3D sine wave..." << std::endl;
  double animation_duration = 3.0;
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
    engine.set_source_panning(src_id, 0.5f*(1.f + std::cos(3.1415926535f*12.f*i/num_iters)));
    engine.set_source_3d_state(src_id, trf_s, vel_w_s, ang_vel_w_s, { pos_l_L_s, pos_l_R_s });
    next_update += std::chrono::milliseconds(static_cast<int>(dt * 1000));
    std::this_thread::sleep_until(next_update);
  }

  // --- 7. Shutdown the engine ---
  engine.stop_source(src_id);
  engine.shutdown();
  std::cout << "Done." << std::endl;
  return EXIT_SUCCESS;
}

int test_4()
{
  std::cout << "=== Test 4 : 3D sound : Rotating Listener with Static Mono Buffer Source ===" << std::endl;
  
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
  engine.init_3d_scene(); // speed_of_sound ~= 343 m/s.

  // --- 4a. Create a source and attach buffer ---
  unsigned int src_id = engine.create_source();
  engine.attach_buffer_to_source(src_id, buf_id);
  engine.set_source_gain(src_id, 0.8f);
  engine.set_source_looping(src_id, true);
  engine.set_source_pitch(src_id, 1.f);
  
  // --- 4b. Set source 3D properties ---
  engine.enable_source_3d_audio(src_id, true);
  la::Mtx4 trf_s = la::look_at({ 2.f, .7f, -3.2f }, { 0.f, 0.f, 0.f }, { 0.f, 1.f, 0.f }); // Source world position encoded here.
  la::Vec3 pos_l_s = la::Vec3_Zero; // Channel emitter local positions encoded here.
  la::Vec3 vel_w_s = la::Vec3_Zero; // Channel emitter world velicity encoded here.
  la::Vec3 ang_vel_w_s = la::Vec3_Zero;
  vel_w_s *= 5.f;
  engine.set_source_3d_state(src_id, trf_s, vel_w_s, ang_vel_w_s, { pos_l_s });
  engine.set_source_coordsys_convention(src_id, applaudio::a3d::CoordSysConvention::RH_XLeft_YUp_ZForward);
  
  // --- 4c. Set listener 3D properties ---
  la::Mtx4 trf_l = la::Mtx4_Identity;
  trf_l.set_column_vec(la::W, la::Vec3_Zero); // Source world position encoded here.
  la::Vec3 pos_l_L_l { -0.12f, 0.05f, -0.05f }; // Channel Left emitter local position encoded here.
  la::Vec3 pos_l_R_l { +0.12f, 0.05f, -0.05f }; // Channel Left emitter local position encoded here.
  la::Vec3 vel_w_l = la::Vec3_Zero; // Lisitener world velocity encoded here.
  la::Vec3 ang_vel_w_l = { 0.f, 0.f, 2.f };
  engine.set_listener_3d_state(trf_l, vel_w_l, ang_vel_w_l, { pos_l_L_l, pos_l_R_l });
  engine.set_listener_coordsys_convention(applaudio::a3d::CoordSysConvention::RH_XRight_YUp_ZBackward);
  
  // --- 4d. Set doppler and falloff properties ---
  engine.set_source_speed_of_sound(src_id, 343.f);
  engine.set_source_attenuation_constant_falloff(src_id, 1.f);
  engine.set_source_attenuation_linear_falloff(src_id, 0.2f);
  engine.set_source_attenuation_quadratic_falloff(src_id, 0.08f);

  // --- 5. Play the source ---
  engine.play_source(src_id);

  // --- 6. Let the engine run for a few seconds. Move source ---
  std::cout << "Playing 3D sine wave..." << std::endl;
  double animation_duration = 3.0;
  int num_iters = 500;
  double dt = animation_duration / num_iters;
  auto start_time = std::chrono::steady_clock::now();
  auto next_update = start_time;
  for (int i = 0; i < num_iters; ++i)
  {
    la::Quat q_l;
    q_l.from_rot_matrix(trf_l.get_rot_matrix());
    auto q_r = quat_from_angle_axis(ang_vel_w_l * dt);
    auto q_rl = q_r * q_l;
    trf_l.set_rot_matrix(q_rl.to_rot_matrix());
    engine.set_listener_3d_state(trf_l, vel_w_l, ang_vel_w_l, { pos_l_L_l, pos_l_R_l });
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
  if (test_3() == EXIT_FAILURE)
    return EXIT_FAILURE;
  if (test_4() == EXIT_FAILURE)
    return EXIT_FAILURE;
    
  return EXIT_SUCCESS;
}
