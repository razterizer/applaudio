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

int main(int argc, const char* argv[])
{
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
