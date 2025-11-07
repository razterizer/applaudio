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
* Cross-platform, so that you can run it on as many farily famous platforms as possible. Temple OS will probably never be supported, but I intend to make a backend for BSD distros / OSS in the future. Perhaps I'll even add mobile OS backends in the future too.
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
* `std::optional<bool> is_source_playing(unsigned int src_id) const` : Checks if a given sound source is already playing and returns true if it plays, false otherwise.
* `void pause_source(unsigned int src_id)` : Pauses the supplied sound source. If it is already paused, then nothing happens.
* `std::optional<bool> is_source_paused(unsigned int src_id) const` : Queries if given source is paused.
* `void stop_source(unsigned int src_id)` : Stops the supplied sound source.
* `void set_source_gain(unsigned int src_id, float vol)` : Sets the gain for the supplied sound source. Value is normally in the range `[0, 1]`.
* `std::optional<float> get_source_gain(unsigned int src_id) const` : Queries source gain.
* `void set_source_volume_dB(unsigned int src_id, float vol_dB)` : Sets the dB volume level for supplied sound source. This function sets a separate gain internally. The idea is that you chain up your gains like this:
```
set_source_gain(g0*g1*g2);
set_source_volume_dB(dB_level);
```
which corresponds to `g_final = g0*g1*g2*vol_gain` where `vol_gain` is an internal variable set via the `set_source_volume_dB()` and `set_source_volume_slider()` functions.
* `std::optional<float> get_source_volume_dB(unsigned int src_id) const` : Queries the dB volume level.
* `void set_source_volume_slider(unsigned int src_id, float vol01, float min_dB = -60.f, std::optional<float> nl_taper = std::nullopt)` : Sets the parametrized (UX) volume level for supplied sound source. Normally the value vol01 should be in the range `[0, 1]`. `min_dB` allows you to adjust the minimum dB level at `vol01 = 0`. At `vol01 = 1` the dB level is always `0`. Optional argument `nl_taper` allows you to tweak the scale nonlinearly. Tweak `min_dB` and `nl_taper` to try to make the volume at `vol01 = 0.5` appear having half the loudness of `vol01 = 1` but twice the loudness of `vol01 = 0.25`. This function sets a separate gain internally. The idea is that you can chain up your gains like this:
```
set_source_gain(g0*g1*g2);
set_source_volume_slider(t_vol);
```
which corresponds to `g_final = g0*g1*g2*vol_gain` where `vol_gain` is an internal variable set via the `set_source_volume_dB()` and `set_source_volume_slider()` functions.
* `std::optional<float> get_source_volume_slider(unsigned int src_id, float min_dB = -60.f, std::optional<float> nl_taper = std::nullopt) const` : Queries the parametrized (UX) volume level.
* `void set_source_pitch(unsigned int src_id, float pitch)` : Sets the pitch for the supplied sound source.
* `std::optional<float> get_source_pitch(unsigned int src_id) const` : Queries source pitch.
* `void set_source_looping(unsigned int src_id, bool loop)` : Tells the sound source if it should be looping or not.
* `std::optional<bool> get_source_looping(unsigned int src_id) const` : Queries whether the source is looping or not.
* `void set_source_panning(unsigned int src_id, std::optional<float> pan)` : Allows you to set panning of a stereo buffer source. If buffer is a mono buffer then nothing will happen. If `std::nullopt` is passed then nothing will happen either. A non-nullopt value will be clamped to the range `[0, 1]`.
* `std::optional<float> get_source_panning(unsigned int src_id) const` : Queries source panning.
* `void print_backend_name() const` : Prints the name of the current backend.
* `void init_3d_scene()` : Initializes positional audio context/scene.
* `void enable_source_3d_audio(unsigned int src_id, bool enable)` : Toggles between positional/spatial and flat/ambient sound for provided source ID.
* `bool set_source_3d_state_channel(unsigned int src_id, int channel, const la::Mtx3& rot_mtx,
                                     const la::Vec3& pos_world, const la::Vec3& vel_world)` : Sets the orientation, world space position and world space linear velocity of specified channel "emitter" for provided source ID. Only mono and stereo channels are supported at the moment.
* `bool get_source_3d_state_channel(unsigned int src_id, int channel, la::Mtx3& rot_mtx,
                                     la::Vec3& pos_world, la::Vec3& vel_world) const` : Gets the 3d spatial state of a source for a specified channel.
* `bool set_listener_3d_state_channel(int channel, const la::Mtx3& rot_mtx,
                                       const la::Vec3& pos_world, const la::Vec3& vel_world)` : Sets the orientation, world space position and world space linear velocity of specified channel "ear" for the listener in the 3D scene. The API supports only one listener at the moment. As with sources, a listener can be mono or stereo but the channels used are tied to the output channels of the current backend used. Only mono and stereo channels are supported at the moment.
* `bool get_listener_3d_state_channel(int channel, la::Mtx3& rot_mtx,
                                       la::Vec3& pos_world, la::Vec3& vel_world) const` : Gets the 3d spatial state of the listener for a given channel.
* `bool set_source_3d_state(unsigned int src_id, const la::Mtx4& trf,
                             const la::Vec3& vel_world, const la::Vec3& ang_vel_local,
                             const std::vector<la::Vec3>& channel_pos_offsets_local)` : Sets the transform, world space linear velocity and world space angular velocity of a source object. In this case, we can see the source basically as a rigid object. Important to note that the transform `trf` must be positioned at the center of mass if you conceptually have this in your application sound source model. `channel_pos_offsets_local` are per channel (emitter) positional offsets in local space which are then rotated to world space by the transform `trf`. This function uses the more low level function `set_source_3d_state_channel()` internally.
* `bool set_listener_3d_state(const la::Mtx4& trf,
                               const la::Vec3& vel_world, const la::Vec3& ang_vel_local,
                               const std::vector<la::Vec3>& channel_pos_offsets_local)` : Sets the transform, world space linear velocity and world space angular velocity of the listener object (only one supported at the moment). In this case, we can see the listener basically as a rigid object. Important to note that the transform `trf` must be positioned at the center of mass if you conceptually have this in your application sound listener model. `channel_pos_offsets_local` are per channel (ear) positional offsets in local space which are then rotated to world space by the transform `trf`. This function uses the more low level function `set_listener_3d_state_channel()` internally.
* `bool set_speed_of_sound(unsigned int src_id, float speed_of_sound)` : Sets the speed of sound for specified source. This allows you to have per source doppler shifts which allows you to simulate different doppler shifts in different physical materials/mediums.
* `std::optional<float> get_speed_of_sound(unsigned int src_id) const` : Gets the speed of sound for a specified source.
* `bool set_attenuation_min_distance(unsigned int src_id, float min_dist)` : Sets the minimum distance limit between listener and source where any attenuation is possible. Any distance smaller than this distance will be clamped to this minimum distance. This property is set per source which allows you to simulate different physical materials/mediums.
* `std::optional<float> get_source_attenuation_min_distance(unsigned int src_id) const` : Gets the minimum distance limit between listener and this source.
* `bool set_attenuation_max_distance(unsigned int src_id, float max_dist)` : Sets the maximum distance limit between listener and source so that no further attenuation will happen beyond this limit. This property is set per source which allows you to simulate different physical materials/mediums.
* `std::optional<float> get_source_attenuation_max_distance(unsigned int src_id) const` : Gets the maximum distance limit between listener and this source.
* `bool set_attenuation_constant_falloff(unsigned int src_id, float const_falloff)` : Sets the constant falloff factor for the attenuation model. This property is set per source which allows you to simulate different physical materials/mediums.
* `std::optional<float> get_source_attenuation_constant_falloff(unsigned int src_id) const` : Gets the constant falloff factor for this source.
* `bool set_attenuation_linear_falloff(unsigned int src_id, float lin_falloff)` : Sets the linear falloff factor for the attenuation model. This property is set per source which allows you to simulate different physical materials/mediums.
* `std::optional<float> get_source_attenuation_linear_falloff(unsigned int src_id) const` : Gets the linear falloff factor for this source.
* `bool set_attenuation_quadratic_falloff(unsigned int src_id, float sq_falloff)` : Sets the quadratic falloff factor for the attenuation model. This property is set per source which allows you to simulate different physical materials/mediums.
* `std::optional<float> get_source_attenuation_quadratic_falloff(unsigned int src_id) const` : Gets the quadratic falloff factor for this source.
* `bool set_source_directivity_alpha(unsigned int src_id, float directivity_alpha)` : For a given source, this sets the directivity alpha value for its per channel emitters. Valid values are in the range of `[0, 1]`. Any value outside this range will be clamped. `directivity_alpha = 0` : Omni, `directivity_alpha = 1` : Fully Directional.
* `std::optional<float> get_source_directivity_alpha(unsigned int src_id) const` : Gets the directivity alpha value for this source.
* `bool set_source_directivity_sharpness(unsigned int src_id, float directivity_sharpness)` : For a given source, this controls the sharpness of the lobe/lobes of each per channel emitter.
* `std::optional<float> get_source_directivity_sharpness(unsigned int src_id) const` : Gets the directivity sharpness for this source.
* `bool set_source_directivity_type(unsigned int src_id, DirectivityType directivity_type)` : For a given source, this sets the directivity type for each of its per channel emitters. This controls the lobe shape of each emitter lobe. Valid types are `Cardioid`, `SuperCardioid`, `HalfRectifiedDipole`, `Dipole`.
* `std::optional<DirectivityType> get_source_directivity_type(unsigned int src_id) const` : Gets the directivity type for this source.
* `bool set_source_rear_attenuation(unsigned int src_id, float rear_attenuation)` : For a given source, this sets the rear attenuation for each of its per channel emitters. valid values are in the range of `[0, 1]`, but any value outside the range will be clamped to that range. 0 = Silence, 1 = No Attenuation. Each per source rear attenuation will be multiplied with the listener rear attenuation which becomes the final rear attenuation.
* `std::optional<float> get_source_rear_attenuation(unsigned int src_id) const` : Gets the rear attenuation for this source.
* `bool set_listener_rear_attenuation(float rear_attenuation)` : For the single listener, this sets the rear attenuation for each of its per channel ears. valid values are in the range of `[0, 1]`, but any value outside the range will be clamped to that range. 0 = Silence, 1 = No Attenuation. Each per source rear attenuation will be multiplied with the listener rear attenuation which becomes the final rear attenuation.
* `std::optional<float> get_listener_rear_attenuation() const` : Gets the rear attenuation for the listener.
* `bool set_source_coordsys_convention(unsigned int src_id, a3d::CoordSysConvention cs_conv)` : Sets the coordinate system convention for a source. Valid values are `CoordSysConvention::RH_XRight_YUp_ZBackward`, `CoordSysConvention::RH_XLeft_YUp_ZForward`, `CoordSysConvention::RH_XRight_YDown_ZForward`, `CoordSysConvention::RH_XLeft_YDown_ZBackward` and `CoordSysConvention::RH_XRight_YForward_ZUp`. Default setting is `CoorSysConvetion::RH_XLeft_YUp_ZForward`.
* `a3d::CoordSysConvention get_source_coordsys_convention(unsigned int src_id) const` : Gets the coordinate system convention for a source.
* `bool set_listener_coordsys_convention(a3d::CoordSysConvention cs_conv)` : Sets the coordinate system convention for the listener. Valid values are `CoordSysConvention::RH_XRight_YUp_ZBackward`, `CoordSysConvention::RH_XLeft_YUp_ZForward`, `CoordSysConvention::RH_XRight_YDown_ZForward`, `CoordSysConvention::RH_XLeft_YDown_ZBackward` and `CoordSysConvention::RH_XRight_YForward_ZUp`. Default setting is `CoorSysConvetion::RH_XLeft_YUp_ZForward`.
* `a3d::CoordSysConvention get_listener_coordsys_convention() const` : Gets the coordinate system convention for the listener.
    

## Length Units, Speed Units and Stability

This API is not using a fixed length unit or speed unit but instead relies on the user to specify quantities in whatever units he/she sees fit.

Positions (especially the W component in source/listener transforms), velocities, and attenuation min/max distances must be expressed in a consistent unit system (e.g., meters & m/s, or kilometers & km/h). 

For best numerical accuracy, keep positions and distances roughly within `[0.1, 10'000]` units, and ensure the magnitudes of velocities relative to the speed of sound stay in a similar range. 
Extreme values may cause precision issues in Doppler or attenuation calculations.

## Coordinate System Conventions

In applaudio, the forward-direction of a coordinate system (typically along the +Z or -Z axis) controls the direction of the emitter or ear lobes (think antenna lobes) and the right-direction (typically along the +X or -X axis) controls the panning direction of a source in the 3D scene.

Applaudio supports four different coordinate system conventions:
```cpp
enum class CoordSysConvention
{
  RH_XRight_YUp_ZBackward,
  RH_XLeft_YUp_ZForward,
  RH_XRight_YDown_ZForward,
  RH_XLeft_YDown_ZBackward,
  RH_XRight_YForward_ZUp,
};
```
These can be set individually for each source and for the listener. Default convention is `CoorSysConvetion::RH_XLeft_YUp_ZForward`, meaning that sound propagates mainly from the +Z direction (and -Z direction if using `DirectivityType::Dipole`).

As this is a post-processing step, all internal calculations rely on right handed coordinate systems no matter if using a left handed or right handed coord-sys convention for a certain object.

## Feature Comparison Chart

| Feature / Library    | applaudio (+8Beat)                  | OpenAL / OpenAL_Soft                | SDL2 Audio            | PortAudio                           | FMOD (Lite)                           | miniaudio                                       | SoLoud                                          |
| -------------------- | ----------------------------------- | ----------------------------------- | --------------------- | ----------------------------------- | ------------------------------------- | ----------------------------------------------- | ----------------------------------------------- | 
| License              | MIT                                 | LGPL / GPL (soft)                   | zlib                  | MIT / custom                        | Proprietary (free for non-commercial) | MIT                                             | ZLib/LibPNG                                     |
| Platform support     | Windows (WASAPI), macOS (CoreAudio), Linux (ALSA)               | Windows, macOS, Linux, Android, iOS | Windows, macOS, Linux | Windows, macOS, Linux, iOS, Android | Windows, macOS, Linux, iOS, Android   | Windows, macOS, Linux, iOS, Android, Emscripten | Windows, macOS, Linux, iOS, Android, Emscripten |
| Flat stereo panning  | ✅ Yes, direct pan control per channel | ⚠️ No (mono sources only via 3D trick) | ⚙️ Manual                | ⚙️ Manual                              | ✅ Yes                                   | ✅ Yes, per-channel pan                            | ✅ Yes, per-channel pan                            |
| 3D spatialization    | ✅ Fully flexible per-channel emitter control; positions, velocities, orientations, distances, falloff-coeffs and speed-of-sound (for doppler); can layer DSP for HRTF-like cues | ⚠️ Built-in for mono sources; stereo sources ignored; limited per-channel control; per source-falloffs but global speed-of-sound (doppler) | ⚙️ Manual / user-implemented | 🚫 None | ✅ Advanced built-in | ⚙️ Basic per-source 3D (mono sources); manual mixing for advanced effects | ⚙️ Built-in 3D for mono sources; simple attenuation, panning, Doppler |
| Listener-based 3D    | ✅ Yes, optional; fully decoupled from per-channel DSP | ✅ Yes, fixed 3D model | ⚙️ Manual | ⚙️ Manual | ✅ Yes | ✅ Yes, optional, supports listener transform | ✅ Yes |
| HRTF / 3D psychoacoustic cues | 🚫 Not yet, since currently all DSP is located in 8Beat (could fix in a future refactoring project) | 🚫 Not included | 🚫 None | 🚫 None | ✅ Built-in optional HRTF / advanced 3D | 🚫 None built-in; can emulate via custom DSP | 🚫 None built-in; can emulate via custom DSP |
| Reverb / environmental effects | ⚙️ None, (available in 8Beat DSP (`reverb()`, `reverb_fast()`) | 🚫 None (requires extension or manual DSP) | 🚫 None | 🚫 None | ✅ Yes, advanced | ⚙️ Minimal built-in; can be implemented via DSP | ✅ Built-in simple effects; DSP chain allows custom reverb |
| SFX / procedural audio generation | ⚙️ None, (available in 8Beat (chip tunes, SFX)) | 🚫 None | ⚙️ Manual | ⚙️ Manual | ⚙️ Some support via FMOD Studio API | ⚙️ Minimal; user-implemented | ✅ Built-in SFX and waveform generation; supports modulators |
| Streaming / low-latency | ✅ High performance, lightweight | ⚙️ Moderate | ⚙️ Moderate | ✅ Excellent | ✅ Excellent | ✅ High performance, lightweight | ✅ High performance; supports streaming |
| Ease of integration | ✅ Modern C++ API, very flexible, header-only | ⚙️ Moderate, C API | ✅ Easy, C API | ⚙️ Moderate, C API, Verbose | ✅ Easy, but large library | ✅ Extremely easy; single-file C API | ✅ Easy; lightweight C++ API, header-only option |
| Header-only | ✅ Yes | 🚫 No | 🚫 No | 🚫 No | 🚫 No | ✅ Yes (single .h) | ✅ Yes |
| Language | C++20 | C | C | C | C/C++ | C | C++ |
| Lines of code | ![C++ LOC](https://raw.githubusercontent.com/razterizer/applaudio/badges/loc-badge.svg) | ~90'000 – 120'000 | ~10'000 – 15'000 | ~25'000 – 35'000 | Unknown (closed source) | ~18'000 | ~25'000 |
| Community / support | 🚫 None yet, early project | ✅ Large | ✅ Large | ⚙️ Medium | ✅ Large, commercial | ✅ Growing, moderate | ⚙️ Moderate; active in indie/game dev |
| Dependencies | ✅ Minimal; just STL/OS API | ⚙️ Platform backend libs | ✅ Minimal | ✅ Minimal | ⚠️ Large, runtime required | ✅ None, single file | ✅ Minimal; just STL/OS API |
| Performance | ✅ High, lightweight | ✅ High | ⚙️ Moderate | ✅ High | ✅ High, optimized for games | ✅ High, very lightweight | ✅ High; designed for games/embedded |
| Use-case fit | Custom engines, games, music/SFX, DSP-heavy applications | Games needing 3D audio for mono sources | Simple games, emulators | Audio recording/playback apps | Commercial-grade game audio with effects | Lightweight engines, games, embedded apps, low-dependency projects | Indie games, lightweight engines, simple 3D audio, SFX generation |

### Flexibility vs. Features Comparison Chart

| Library	| 🔧 Customizability | 🎛️ Built-In DSP & Effects     |	🎧 3D/Spatialization | 🧠 HRTF / Psychoacoustics |	⚡ Performance / Footprint |	🧩 Ease of Integration |	🗃️ Header-only |
| ------- | ------------------ | ----------------------------- | --------------------- | ------------------------- | --------------------------- | ----------------------- | --------------- |
| applaudio (+8Beat) | 🟢🟢🟢🟢🟢 | 🟢🟢🟢 (8Beat DSP) | 🟢🟢🟢🟢 |	🔴 (not yet, since all DSP is now in 8Beat, requires some refactoring to fix) |	🟢🟢🟢🟢	| 🟢🟢🟢🟢	| 🟢 Yes |
| OpenAL | 🟡🟡🟡 |	🟡 | 🟢🟢🟢 | 🔴	| 🟢🟢🟢	| 🟡🟡	| 🔴 No |
| SDL2 Audio | 🟢🟢🟢🟢	| 🔴 | 🔴	| 🔴 | 🟢🟢🟢 | 🟢🟢🟢	| 🔴 No |
| PortAudio	| 🟢🟢🟢🟢 | 🔴 |	🔴 | 🔴	| 🟢🟢🟢🟢	| 🟡🟡 | 🔴 No |
| FMOD (Lite) |	🟡🟡 | 🟢🟢🟢🟢🟢 | 🟢🟢🟢🟢🟢 | 🟢🟢🟢🟢 | 🟢🟢🟢🟢 | 🟢🟢🟢🟢🟢 | 🔴 No |
| miniaudio |	🟢🟢🟢🟢 | 🟡🟡 | 🟢🟢🟢	 | 🔴	| 🟢🟢🟢🟢🟢 |	🟢🟢🟢🟢🟢	| 🟢 Yes |
| SoLoud | 🟢🟢🟢🟢 | 🟢🟢🟢🟢 |	🟢🟢🟢 | 🔴 | 🟢🟢🟢🟢 |	 🟢🟢🟢🟢	| 🟢 Yes |
| Summary | applaudio = Most modular and DSP-extensible? |	FMOD = richest effects | OpenAL = fixed 3D | FMOD only with real HRTF |	miniaudio = fastest |	applaudio/miniaudio = easiest |	applaudio/miniaudio/SoLoud = header-only |

*DISCLAIMER!* The charts have been partially generated with GPT-5 and may therefore not be entirely truthful.
