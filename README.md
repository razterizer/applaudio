# aupplaudio
A simple cross-platform audio library that is not GPL-based.

![GitHub License](https://img.shields.io/github/license/razterizer/applaudio?color=blue)
![Static Badge](https://img.shields.io/badge/linkage-header_only-yellow)
![Static Badge](https://img.shields.io/badge/C%2B%2B-20-yellow)

The idea with this library is to have a library that has the following properties:

* Permissive license. Unlike `OpenAL` / `OpenAL_Soft` which is GPL, this library is MIT and gives you much more freedom to use the lib where you want to use it. Just a little credit somewhere is enough.
* A library that has an API similar to `OpenAL` / `OpenAL_Soft` makes it easier to familiarize yourself with the API as `OpenAL` is a very famous audio library.
* Header-only. Thus you don't have to link it into your program except for the native libs that the OS requires. Also, you don't need to compile any static or dynamic lib files.
* A light-weight library that doesn't contribute much to your executable file size. Also less code to read.
* Modern C++ (C++20) that make the code easier to read and understand.
* Cross-platform, so that you can run it on as many farily famous platforms as possibles. Temple OS will probably never be supported, but I intend to make a backend for BSD distros / OSS in the future. Perhaps I'll even add mobile OS backends in the future too.

## Structure

```
AudioLibSwitcher_applaudio // different repo that implements the IAudioLibSwitcher interface.
 |
 +--- adapter implementation details in AudioLibSwitcher_applaudio code in that repo ---
   |
   +--> AudioEngine  ("front end" of this lib)
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
