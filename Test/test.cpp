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

int main(int argc, const char* argv[])
{
  applaudio::AudioEngine engine;
  engine.print_device_name();

  // --- 1. Create and start the engine ---
  int sample_rate = 44100;
  int channels = 2;
  bool verbose = true;
  if (!engine.startup(sample_rate, channels, verbose))
  {
    std::cerr << "Failed to start AudioEngine\n";
    return EXIT_FAILURE;
  }

  // --- 2. Create a buffer with a sine wave ---
  const double frequency = 440.0; // A4
  const double duration = 2.0;    // 2 seconds
  size_t frame_count = static_cast<size_t>(duration * sample_rate);

  std::vector<short> pcm_data(frame_count * channels);
  for (size_t i = 0; i < frame_count; ++i)
  {
    short sample = static_cast<short>(std::sin(2.0 * M_PI * frequency * i / sample_rate) * 30000);
    for (int c = 0; c < channels; ++c)
      pcm_data[i * channels + c] = sample; // same sample for all channels
  }

  unsigned int buf_id = engine.create_buffer();
  engine.set_buffer_data(buf_id, pcm_data);

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
