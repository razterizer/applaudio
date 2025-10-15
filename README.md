# applaudio
A simple cross-platform audio library that is not GPL-based.

![GitHub License](https://img.shields.io/github/license/razterizer/applaudio?color=blue)
![Static Badge](https://img.shields.io/badge/linkage-header_only-yellow)
![Static Badge](https://img.shields.io/badge/C%2B%2B-20-yellow)

[![build test ubuntu](https://github.com/razterizer/applaudio/actions/workflows/build-test-ubuntu.yml/badge.svg)](https://github.com/razterizer/applaudio/actions/workflows/build-and-test-ubuntu.yml)
[![build test macos](https://github.com/razterizer/applaudio/actions/workflows/build-test-macos.yml/badge.svg)](https://github.com/razterizer/applaudio/actions/workflows/build-macos.yml)

![Top Languages](https://img.shields.io/github/languages/top/razterizer/applaudio)
![GitHub repo size](https://img.shields.io/github/repo-size/razterizer/applaudio)
![C++ LOC](https://raw.githubusercontent.com/razterizer/applaudio/badges/loc-badge.svg)
![Commit Activity](https://img.shields.io/github/commit-activity/t/razterizer/applaudio)
![Last Commit](https://img.shields.io/github/last-commit/razterizer/applaudio?color=blue)
![Contributors](https://img.shields.io/github/contributors/razterizer/applaudio?color=blue)

The idea with this library is to have a library that has the following properties:

* Permissive license. Unlike `OpenAL` / `OpenAL_Soft` which is GPL, this library is MIT and gives you much more freedom to use the lib where you want to use it. Just a little credit somewhere is enough.
* A library that has an API similar to `OpenAL` / `OpenAL_Soft` makes it easier to familiarize yourself with the API as `OpenAL` is a very famous audio library.
* Header-only. Thus you don't have to link it into your program except for the native libs that the OS requires. Also, you don't need to compile any static or dynamic lib files.
* A light-weight library that doesn't contribute much to your executable file size. Also less code to read.
* Modern C++ (C++20) that make the code easier to read and understand.
* Cross-platform, so that you can run it on as many farily famous platforms as possibles. Temple OS will probably never be supported, but I intend to make a backend for BSD distros / OSS in the future. Perhaps I'll even add mobile OS backends in the future too.
* Standalone audio lib. No other dependencies except for system headers.

## Structure

```
8Beat // (optional) different repo / library for sound effects and generation of waveforms, chiptune player etc.
|
+--> AudioLibSwitcher_applaudio // (optional) different repo that implements the IAudioLibSwitcher interface.
      |
      +--> AudioEngine  ("front end" of this lib) ============================
              |
              +--> IBackend
                    |--> Backend_NoAudio
                    |--> Backend_MacOS_CoreAudio
                    |       |--> CoreAudio (CoreAudio, AudioToolbox and CoreFoundation frameworks)
                    |--> Backend_Linux_ALSA
                    |       |--> ALSA (export BUILD_PKG_CONFIG_MODULES='alsa')
                    |--> Backend_Windows_WASAPI
                    :       |--> WASAPI (ole32.lib)
                    :       :
                    :       Native level APIs. No external dependencies!
                    Backends (IBackend adapters)
```

## How to build

### MacOS Compiler Flags

```sh
-std=c++20 -I<path to applaudio/include> -framework AudioToolbox -framework CoreAudio -framework CoreFoundation
```

### Linux Compiler Flags

```sh
export BUILD_PKG_CONFIG_MODULES='alsa'
-std=c++20 -I<path to applaudio/include> $(pkg-config --cflags --libs $BUILD_PKG_CONFIG_MODULES)
```

### Windows vcxproj File

```xml
<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
    <ExternalIncludePath><<Path to applaudio\include>>;$(VC_IncludePath);$(WindowsSDK_IncludePath)</ExternalIncludePath>
  </PropertyGroup>
```

```xml
<ClCompile>
<WarningLevel>Level3</WarningLevel>
<FunctionLevelLinking>true</FunctionLevelLinking>
<IntrinsicFunctions>true</IntrinsicFunctions>
<SDLCheck>true</SDLCheck>
<PreprocessorDefinitions>...%(PreprocessorDefinitions)</PreprocessorDefinitions>
<ConformanceMode>true</ConformanceMode>
<LanguageStandard>stdcpp20</LanguageStandard>
<LanguageStandard_C>stdc17</LanguageStandard_C>
</ClCompile>
```
The important part here is c++20.

## The API

`AudioEngine`:

* `AudioEngine(bool enable_audio = true)` : The constructor.
* `int output_sample_rate() const` : Gets the sample rate used internally and that is then used towards the current backend.
* `int num_output_channels() const` : Gets the number of channels (only 1 or 2 are valid values) used internally and that is then used towards the current backend.
* `int num_bits_per_sample() const` : Gets the bit format (8 (int), 16 (int), 32 (float)).
* `bool startup(int request_out_sample_rate = 48'000, 
                int request_out_num_channels = 2, 
                bool request_exclusive_mode_if_supported = false, 
                bool verbose = false)` : Starts the audio engine. `request_out_sample_rate` : Supplied sample reate may not be guaranteed to be accepted by the backend (use `output_sample_rate()` to get the actual sample rate). `request_out_num_channels` : Supplied number of channels may not be guaranteed by the backend (but most likely will be, use `num_output_channels()` to get the actual number of channels used). `request_exclusive_mode_if_supported` : Not working at the moment. Keep it `false` for now. `verbose` : Prints extra info. Function returns false if it failed to startup the engine.
* `void shutdown()`: Shuts down the engine. Mirrors `startup()`.
* `unsigned int create_source()` : Creates a sound source.
* `void destroy_source(unsigned int src_id)` : Destroys a sound source with given id.
* `unsigned int create_buffer()` : Creates a sound buffer.
* `void destroy_buffer(unsigned int buf_id)` : Destroys a sound buffer with given id.
* `bool set_buffer_data_8u(unsigned int buf_id, const std::vector<unsigned char>& data,
                            int channels, int sample_rate)` : Allows you to set 8 bit unsigned integer audio data for the specified sound buffer. Returns false on failure.
* `bool set_buffer_data_8s(unsigned int buf_id, const std::vector<char>& data,
                            int channels, int sample_rate)` : Allows you to set 8 bit signed integer audio data for the specified sound buffer. Returns false on failure.
* `bool set_buffer_data_16s(unsigned int buf_id, const std::vector<short>& data,
                             int channels, int sample_rate)` : Allows you to set 16 bit signed integer audio data for the specified sound buffer. Returns false on failure.
* `bool set_buffer_data_32f(unsigned int buf_id, const std::vector<float>& data,
                             int channels, int sample_rate)` : Allows you to set 32 bit float audio data for the specified sound buffer. Returns false on failure.
* `bool attach_buffer_to_source(unsigned int src_id, unsigned int buf_id)` : Attaches a sound buffer to a sound source. Returns false on failure.
* `bool detach_buffer_from_source(unsigned int src_id)` : Detaches a sound buffer from a sound source. Returns false on failure.
* `void mix()` : Mixes the sound buffers from each respective sound source (depending on the state of the sources that hold each buffer). The mixer is supposed to be called via a thread that is started by the `startup()` function, but mentioning it here for reference. The mixer is capable of handling buffers of different sampling rates (via linear interpolation) and different amounts of channels. This is the heart of the audio engine.
* `void play_source(unsigned int src_id)` : Starts playing a sound source. If not paused then it plays from the beginning, but if it was paused, then it will resume playback from where it was paused.
* `bool is_source_playing(unsigned int src_id) const` : Checks if a given sound source is already playing and returns true if it plays, false otherwise.
* `void pause_source(unsigned int src_id)` : Pauses the supplied sound source. If it is already paused, then nothing happens.
* `void stop_source(unsigned int src_id)` : Stops the supplied sound source.
* `void set_source_volume(unsigned int src_id, float vol)` : Sets the volume for the supplied sound source.
* `void set_source_pitch(unsigned int src_id, float pitch)` : Sets the pitch for the supplied sound source.
* `void set_source_looping(unsigned int src_id, bool loop)` : Tells the sound source if it should be looping or not.
* `void print_backend_name() const` : Prints the name of the current backend.
* `void init_3d_scene(float speed_of_sound)` : Initializes positional audio context/scene and forces you to set the speed of sound in whatever speed unit you are using in your application code.
* `void enable_source_3d_audio(unsigned int src_id, bool enable)` : Toggles between positional/spatial and flat/ambient sound for provided source ID.
* `bool set_source_3d_state_channel(unsigned int src_id, int channel, const la::Mtx3& rot_mtx,
                                     const la::Vec3& pos_world, const la::Vec3& vel_world)` : Sets the orientation, world space position and world space linear velocity of specified channel "emitter" for provided source ID. Only mono and stereo channels are supported at the moment.
* `bool set_listener_3d_state_channel(int channel, const la::Mtx3& rot_mtx,
                                       const la::Vec3& pos_world, const la::Vec3& vel_world)` : Sets the orientation, world space position and world space linear velocity of specified channel "ear" for the listener in the 3D scene. The API supports only one listener at the moment. As with sources, a listener can be mono or stereo but the channels used are tied to the output channels of the current backend used. Only mono and stereo channels are supported at the moment.
* `bool set_source_3d_state(unsigned int src_id, const la::Mtx4& trf,
                             const la::Vec3& vel_world, const la::Vec3& ang_vel_local,
                             const std::vector<la::Vec3>& channel_pos_offsets_local)` : Sets the transform, world space linear velocity and world space angular velocity of a source object. In this case, we can see the source basically as a rigid object. Important to note that the transform `trf` must be positioned at the center of mass if you conceptually have this in your application sound source model. `channel_pos_offsets_local` are per channel (emitter) positional offsets in local space which are then rotated to world space by the transform `trf`. This function uses the more low level function `set_source_3d_state_channel()` internally.
* `bool set_listener_3d_state(const la::Mtx4& trf,
                               const la::Vec3& vel_world, const la::Vec3& ang_vel_local,
                               const std::vector<la::Vec3>& channel_pos_offsets_local)` : Sets the transform, world space linear velocity and world space angular velocity of the listener object (only one supported at the moment). In this case, we can see the listener basically as a rigid object. Important to note that the transform `trf` must be positioned at the center of mass if you conceptually have this in your application sound listener model. `channel_pos_offsets_local` are per channel (ear) positional offsets in local space which are then rotated to world space by the transform `trf`. This function uses the more low level function `set_listener_3d_state_channel()` internally.
* `bool set_attenuation_min_distance(float min_dist)` : Sets the minimum distance limit between listener and source where any attenuation is possible. Any distance smaller than this distance will be clamped to this minimum distance.
* `bool set_attenuation_max_distance(float max_dist)` : Sets the maximum distance limit between listener and source so that no further attenuation will happen beyond this limit.
* `bool set_attenuation_constant_falloff(float const_falloff)` : Sets the constant falloff factor for the attenuation model.
* `bool set_attenuation_linear_falloff(float lin_falloff)` : Sets the linear falloff factor for the attenuation model.
* `bool set_attenuation_quadratic_falloff(float sq_falloff)` : Sets the quadratic falloff factor for the attenuation model.

## Length Units, Speed Units and Stability

This API is not using a fixed length unit or speed unit but instead relies on the user to specify quantities in whatever units he/she sees fit.

Positions (especially the W component in source/listener transforms), velocities, and attenuation min/max distances must be expressed in a consistent unit system (e.g., meters & m/s, or kilometers & km/h). 

For best numerical accuracy, keep positions and distances roughly within `[0.1, 10'000]` units, and ensure the magnitudes of velocities relative to the speed of sound stay in a similar range. 
Extreme values may cause precision issues in Doppler or attenuation calculations.
