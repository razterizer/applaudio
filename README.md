# aupplaudio
A simple cross-platform audio library that is not GPL-based.

## Structure

```
AudioLibSwitcher_applaudio // different repo that implements the IAudioLibSwitcher interface.
// --- adapter implementation details in AudioLibSwitcher_applaudio code in that repo ---

   |--> AudioEngine // "front end" of this lib.
           |--> IApplaudio_internal
                  |--> MacOS_Internal / Linux_Internal / Win_Internal // back ends
                         |--> AudioToolBox / ALSA / Windows WASAPI // native level APIs. No external dependencies!
```
