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
8Beat // (optinoal) different repo / library for sound effects and generation of waveforms, chiptune player etc.
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
* `void play_source(unsigned int src_id)` : Starts playing a sound source. If playing for the first time or if it was stopped, it plays from the beginning, and if it was paused, then it will resume playback from where it was paused.
* `bool is_source_playing(unsigned int src_id) const` : Checks if a given sound source is already playing and returns true if it plays, false otherwise.
* `void pause_source(unsigned int src_id)` : Pauses the supplied sound source. If it is already paused, then nothing happens.
* `void stop_source(unsigned int src_id)` : Stops the supplied sound source.
* `void set_source_volume(unsigned int src_id, float vol)` : Sets the volume for the supplied sound source.
* `void set_source_pitch(unsigned int src_id, float pitch)` : Sets the pitch for the supplied sound source.
* `void set_source_looping(unsigned int src_id, bool loop)` : Tells the sound source if it should be looping or not.
* `void print_backend_name() const` : Prints the name of the current backend.
* `void init_3d_scene(a3d::LengthUnit global_length_unit)` : Initializes positional audio context/scene and allows you to set the length unit used for positions and velocities.
* `void enable_source_3d_audio(unsigned int src_id, bool enable)` : Toggles between positional/spatial and flat/ambient sound for provided source ID.
* `bool set_source_pos_vel(unsigned int src_id, const la::Mtx4& new_trf,
                            const la::Vec3& pos_local_left, const la::Vec3& vel_world_left, // mono | stereo left
                            const la::Vec3& pos_local_right = la::Vec3_Zero, const la::Vec3& vel_world_right = la::Vec3_Zero, // stereo right
                            std::optional<a3d::LengthUnit> length_unit = std::nullopt)` : Sets the orientation, positions of channel emitters in local space and velocities of channel emitters in world space for provided source ID. The function ignores the second position and velocity arguments if the buffer attached to the source has mono sound. Only mono and stereo channels are supported at the moment. Optional argument length_unit is using meters by default and if provided, will rescale the position and velocity arguments to the global length unit set by `init_3d_scene()`.
* `bool set_listener_pos_vel(const la::Mtx4& new_trf,
                              const la::Vec3& pos_local_left, const la::Vec3& vel_world_left, // mono | stereo left
                              const la::Vec3& pos_local_right = la::Vec3_Zero, const la::Vec3& vel_world_right = la::Vec3_Zero, // stereo right
                              std::optional<a3d::LengthUnit> length_unit = std::nullopt)` : Sets the orientation, positions of channel emitters in local space and velocities of channel emitters in world space for the listener in the 3D scene. The API supports only one listener at the moment. As with sources, a listener can be mono or stereo but the channels used are tied to the output channels of the current backend used. The function ignores the second position and velocity arguments if the buffer attached to the source has mono sound. Only mono and stereo channels are supported at the moment. Optional argument length_unit is using meters by default and if provided, will rescale the position and velocity arguments to the global length unit set by `init_3d_scene()`.
* `bool set_attenuation_min_distance(float min_dist)` : Sets the minimum distance limit between listener and source where any attenuation is possible. Any distance smaller than this distance will be clamped to this minimum distance.
* `bool set_attenuation_max_distance(float max_dist)` : Sets the maximum distance limit between listener and source so that no further attenuation will happen beyond this limit.
* `bool set_attenuation_constant_falloff(float const_falloff)` : Sets the constant falloff factor for the attenuation model.
* `bool set_attenuation_linear_falloff(float lin_falloff)` : Sets the linear falloff factor for the attenuation model.
* `bool set_attenuation_quadratic_falloff(float sq_falloff)` : Sets the quadratic falloff factor for the attenuation model.
