# Ambient sound

The sound of the land itself: sea, shore, rivers, wind, rain, birds and insects, mixed from a map of the land's sound
types around the camera and changing with height, weather, time of day and the player's alignment.

**Progress: 10/11 done, 1 partial — 95%**

## The ambience

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each kind of place has its bank: sea, still water, coast, jungle, arctic, desert, countryside, swamp, running water, high up, night, rain and wind | done | `src/Audio/SoundMap.h`, `AtmosAudio` |
| Each turn the sound map around the camera sets how loud each bank should be | done | `src/Audio/SoundMap.cpp`; test `test_atmos` |
| Banks fade towards their volume by a set step a turn | done | `AtmosAudio`; test `test_atmos` |
| The time of day (day, dusk, night) chooses the banks' samples, night sounds taking over | done | `AtmosAudio::CalculateSkyType` |
| The camera's height above the land fades the ground's banks and brings in the high-up one | done | `SoundMap` (height fade) |
| Rain and wind follow the weather at the camera | done | `AtmosAudio` |
| Over land of a god at or below -0.6 alignment the banks switch to their evil samples | done | `AtmosAudio` |
| Each bank schedules its own samples at random times and places | done | `src/Audio/AtmosPlayer.cpp`; test `test_atmos` against the scheduler emulator |
| Inside the temple the ambience fades to silence | done | `AtmosAudio` |
| The ambience is quiet while a film plays | partial | `AtmosAudio` takes a video flag; no films play yet |
| The sound map is made from the land when it loads | done | `SoundMap` |
