# aupplaudio
A simple cross-platform audio library that is not GPL-based.

![GitHub License](https://img.shields.io/github/license/razterizer/applaudio?color=blue)
![Static Badge](https://img.shields.io/badge/linkage-header_only-yellow)
![Static Badge](https://img.shields.io/badge/C%2B%2B-20-yellow)

## Structure

```
AudioLibSwitcher_applaudio // different repo that implements the IAudioLibSwitcher interface.
 |
 +--- adapter implementation details in AudioLibSwitcher_applaudio code in that repo ---
   |
   +--> AudioEngine  ("front end" of this lib)
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
