# Audio: the engine, the banks, the music, the voices and the script

This page explains how Black & White 1 sounds. It covers the original's engine (GAudio in `runblack.exe` on top of
LHaudiodllR and QMixer), the banks and their formats (.sad, .sas and the MP2 music in segments), the music (LHMusic and
the music part of GAudio), the voices and the texts, and the CHL audio functions. For each topic it says what openblack
does; the overall state is in [State in openblack](#state-in-openblack).
The "what sounds and when" of each object, animation or hit is on the page for each topic
([links](#what-sounds-and-when)).

- [Audio engine](#audio-engine)
  - [Original: GAudio, LHaudio and QMixer](#original-gaudio-lhaudio-and-qmixer)
  - [openblack audio architecture](#openblack-audio-architecture)
  - [State of the effects engine in openblack](#state-of-the-effects-engine-in-openblack)
- [Banks and formats](#banks-and-formats)
  - [Table of effect and dialogue banks](#table-of-effect-and-dialogue-banks)
  - [.sad format](#sad-format)
  - [Wave loading and cache](#wave-loading-and-cache)
  - [Channels, priorities and loops](#channels-priorities-and-loops)
  - [Master volumes, focus and reset](#master-volumes-focus-and-reset)
  - [.sas and animation tables](#sas-and-animation-tables)
  - [Ambience: GSoundMap and LHAtmos](#ambience-gsoundmap-and-lhatmos)
- [Music](#music)
  - [Music banks: MP2 segments](#music-banks-mp2-segments)
  - [LHMusic: the 6-track engine](#lhmusic-the-6-track-engine)
  - [MUSIC_TYPE table](#music_type-table)
  - [GameMusic: ProcessMusic and its sources](#gamemusic-processmusic-and-its-sources)
  - [Intro, trailer, outro, videos and menu](#intro-trailer-outro-videos-and-menu)
  - [Music in openblack](#music-in-openblack)
- [Voices and texts](#voices-and-texts)
  - [Text voice table](#text-voice-table)
  - [Narration and reading time](#narration-and-reading-time)
  - [Advisors, Guidance, confirmations and night voices](#advisors-guidance-confirmations-and-night-voices)
  - [Voices and texts in openblack](#voices-and-texts-in-openblack)
- [Script: audio CHL](#script-audio-chl)
  - [GScript switches](#gscript-switches)
  - [Dialogue, widescreen and script camera](#dialogue-widescreen-and-script-camera)
- [Music and texts in openblack: files and tests](#music-and-texts-in-openblack-files-and-tests)
- [The effects core in openblack](#the-effects-core-in-openblack)
- [Anim-effects and SoundTag in openblack](#anim-effects-and-soundtag-in-openblack)
- [The world's callers and the effects CHL in openblack](#the-worlds-callers-and-the-effects-chl-in-openblack)
- [Voices on channels in openblack](#voices-on-channels-in-openblack)
- [The miracles on channels in openblack](#the-miracles-on-channels-in-openblack)
- [Interface and hand in openblack](#interface-and-hand-in-openblack)
- [Guidance and night voices in openblack](#guidance-and-night-voices-in-openblack)
- [A single engine](#a-single-engine)
- [The structure: layers, one generator, one clock](#the-structure-layers-one-generator-one-clock)
- [The shared APIs](#the-shared-apis)
- [Owners, the 24-bit FPU and the double constants](#owners-the-24-bit-fpu-and-the-double-constants)
- [What sounds and when](#what-sounds-and-when)
- [State in openblack](#state-in-openblack)
- [Weather and alignment in openblack](#weather-and-alignment-in-openblack)
- [Chants and heartbeat in openblack](#chants-and-heartbeat-in-openblack)
- [The citadel interior in openblack](#the-citadel-interior-in-openblack)
- [Test hooks](#test-hooks)
- [Pending](#pending)

## Audio engine

### Original: GAudio, LHaudio and QMixer

**Faithful.**

- **Creation.** `pc_main` 0x641D79 calls fn_00590FD0, which creates `GGlobal::Global.audio = new GAudio`
  (0x3D4 bytes, ctor 0x426D40). Before it come `PlayLogoScreens` and `Report3D("InitialiseAudio")`, and after it
  `PlayPreIntroVideo`. The ctor does the following:
  - Creates `LH_AudioSystem` (0x1EC bytes) with GAudio's values: 16 sample channels (sys+0xCC), 22050 Hz
    (0x5622, sys+0xD8), a wave cache of RAM/8 (`dwTotalPhys >> 3`, 0x426DDC) and no hardware.
  - With `LHWaveIsInstalled() == 1` it registers `fn_00427200` as the 3D position function, with 800.0f as the maximum
    distance of the anim-effects (0x426E6B; it is a distance, not a number of sounds).
  - Reads the master volumes (fn_00428250) and registers the 85 music banks of 0x9C9748 and the 11 effect banks of
    0x9CB3F8 (fn_00429CB0 → fn_0042A350 → fn_0042A390).
- **LH_AudioSystem.**
  - Ctor 0x10015290 and `Create` 0x100153F0. `Create` can only be called once ("NO, NO, NO!! - one-time audio
    object only").
  - Reads the registry key `...\Audio\Override` (0x100103E0). Its values are NoAudio, Wave, Midi, Music, Redbook,
    Atmos, Sfx, 3DSpatialisation, UseHardware, SpeakerConfig, HeapSize, MaxSamp, HWRate and Latency. On the user's
    machine the key does not exist.
- **QMixer** (0x10015820).
  - Opens 22 software channels: 16 for samples and 6 for music. It works at 22050 Hz.
  - `QSWaveMixSetPanRate(16, 1, 100)`.
  - The pump runs via `timeSetEvent(20, …, 0x10015800)`, every **20 ms**.
  - The listener velocity is 0 (0x10015C1A): **there is no Doppler, no EAX and no reverb**.
- **All triggers go through a single path**: `GAudio::PlaySoundEffect` 0x429E30 (with its 5 variants) or
  `SamplePlayAnimEffect` 0x42A4B0. The only exceptions are:
  - `HelpDude::PlaySample` 0x5BB530, which calls `LHSamplePlay` directly, without GAudio's filters;
  - `LHAtmosProcess`;
  - 8 calls to `LHMusicPlay`.
- **Stopping and querying** are also done outside GAudio:
  - `LHSampleStop` in FallingSpell (0x526FD6, 0x527181, 0x52719D) and in `HelpDude::StopSentence` 0x5BB875;
  - `LHMusicStop` in FallingSpell 0x5271B0 and in `PlayPreIntroVideo` 0x642907;
  - `LHSampleIsPlaying` and `LHSampleSetVolume` in PSysSound fn_006D1110 (0x6D120A, 0x6D1239);
  - the HelpDude queries (0x5BB797, 0x5BB80D, 0x5BCD66).
- **`PlaySoundEffect` filters** (0x429E37..0x429FD9, 0x42A56E):
  - Nothing sounds if `g_game` or HelpSystem are null.
  - 3D cutoff if dist²(camera, pos + offset) > max². The maximum is the one from the .sad (+0x26C) or, if it is 0, the one
    from opts+0x58 = 9999. Inside the citadel, the distance is measured from `LH3DTech::g_camera` 0xEA1DB8.
  - userParam 1 (script widescreen) does not sound with the black bars on. Inside the citadel only userParam 2
    sounds.
  - With `SET_GAME_SOUND` only banks 6 and 7 sound (GAudio+0x3C0/+0x3C4).
  - userParam 4 depends on interface states 0x10, 0x16 and 0x17.
  - With is3D and track, nothing sounds if the owner is a GameThing with `IsAvailable() == 0`. The symbol
    `GAudio::IsInsideCitadel` 0x429D20 is misnamed: it is actually "owner not available".
- **Inside the citadel** = `g_game+0x205A28 == 1` (0x4282F0; the symbol `GetWideScreenControl` is misnamed).
  It is set to 1 by `GoInsideCitadel` 0x554004 and to 0 by `LeaveInsideCitadel` 0x553B1F.
- **Per-turn order**, `ProcessAudioGameTurn` 0x427080. It is called by `GGame::EndTurn` after turn 5 (0x54E997):
  1. Only if `LHWaveIsActive` (0x427086): the music (0x427DF0), the ambience targets (0x429100), the ambience
     banks (0x428FE0), `UpdateChannels` + the listener (0x4270D0) and `LHAtmosProcess(1)` if `g_game+0x250188 == 0`
     (no video playing: +0x250188 is the LHVideoPlayer).
  2. **Always**: fn_00429700, which purges the ThingMusicInfo list.
- **Game fields the audio reads.**
  - g_game+0x250188 is the LHVideoPlayer: written by fn_0054AB20 (0x54AC23), cleared by `DeleteVideo` 0x54A969 and
    `ClearVariables` 0x54BF28. Only the intro and fall films set it, not the tips or the pre-intro video. It is tested by
    ProcessMusic (0x427DF8) and by ProcessAudioGameTurn (0x4270B1; LHAtmosProcess(1) is skipped, 0x4270AC..0x4270C0).
  - g_game+0x205A08 is the land number (SET_LAND_NUMBER 0x7177A4); ProcessMusic plays nothing on land 6 (0x427E1D).
  - g_game+0x205A0C is the playground-game flag: `GGame::Init` sets it at 0x54F75E, after `ResetAndStartPlaygroundGame`.
  - The turn test of ProcessAlignmentMusic (turn > 20) is unsigned (0x427A2E).
  - The interface state is GInterface+0x44 (`GGame::MyInterface` 0x555850); the user-parameter-4 filter reads it at
    0x429FA5..0x429FB8.
  - The local player's number is `GPlayer::GetPlayerNumber` 0x64A790 (+0xB5); the local player is g_game+0x205A59. A new
    game without a profile starts with alignment 0 (`GGame::Init` 0x54FEA0).
  - The position of a script thing is built from its MapCoords as (x, GetAltitude + height above the land, z)
    (0x42943F..0x429470). The thing is unavailable when `GameThing::IsAvailable` (vt +0x2C) is 0.
  - `GAudio::IsInstalled` 0x426D30 → `LHWaveIsInstalled` 0x10015D20; `LHWaveIsActive` is 0x10015D30.
  - `GAudio::ToBeDeleted` 0x426FE0 → `~LH_AudioSystem` (StopAll first, 0x10015C70): the channels stop before the device
    is closed.
  - Call sites inside ProcessAudioGameTurn: ProcessMusic 0x427092, fn_00429100 0x427099, ProcessAtmosBanks 0x4270A0,
    LHAtmosProcess(1) 0x4270C0, fn_00429700 0x4270C8.
- **`PlaySoundEffect` variants.**
  - In every variant sample 0 plays nothing (0x429DA9, 0x42A04C, 0x42A109).
  - 0x429DA0: a 3D sound with no owner, or with an unavailable owner, plays nothing (0x429DB5..0x429DD7). Otherwise the
    point is the owner's Get3DSoundPos, or (0, 0, 0) for a 2D sound. Then it calls 0x42A040.
  - 0x42A040 fills GAudio's working options +0x240 (ctor call at 0x426D63): bank +0x04, owner +0x20, sample +0x24,
    +0x10 = the 6th argument (0x42A06E), is3D +0x08, track +0x0C = is3D (0x42A087), point +0x30, offset +0x3C = 0
    (0x42A0B5..0x42A0C1), loops +0x4C, mode +0x50. A 3D sound of an unavailable owner plays nothing
    (0x42A0CE..0x42A0DB). Then it calls 0x429E30.
  - 0x42A100 takes the offset into +0x3C (0x42A15C..0x42A174) and track +0x0C from the low byte of its 5th argument
    (0x42A18E..0x42A19C). It tests the owner only when both is3D and track are set (0x42A19A..0x42A1B4).
  - The user parameter the filters compare is the low 16 bits of LHSampleGetUserParam (`cmp di, 1` at 0x429F65; read
    at 0x42A520 in SamplePlayAnimEffect). SET_GAME_SOUND's flag GScript+0x90 is read at 0x429F88 and at 0x42A56E.
  - Inside SamplePlayAnimEffect: the filters 0x42A51B..0x42A5B8, the play `LHSamplePlayAnimEffect` 0x42A5D4. A stop or
    release (action ≠ 0) passes no min and no max (0x42A4C6 / 0x42A4C8).
- **Cyclic sample counters**: the sample is base + counter, then the counter goes up by one and wraps to 0 at its count
  (Abode::InterfaceTap 0x4068F4..0x40690F; the tree mulch adds first and masks with 3, 0x63AA39):
  - [0xC5E3E4] 0..4: G_CitadelSpark 206 + c (fn_004686B0 0x468815);
  - [0xC5E3E8] 0..4: G_CitadelSpark 206 + c (`CitadelHeart::ProcessTransferedDamageEffect` 0x468B32);
  - [0xC6421C] 0..3: G_RockTap 139 + c (fn_0048BCC0 0x48C433);
  - [0xC64220] 0..2: G_SquashAnimal 143 + c (fn_0048BCC0 0x48C510);
  - [0xD95AF8] 0..3: G_ScaffoldCombine 201 + c (`Scaffold::ApplyThisToObject` 0x6E9AC9);
  - [0xD95AFC] 0..3: G_ScaffoldTap 151 + c (`Scaffold::InterfaceTap` 0x6E9E7E).
- **Samples picked with GetTickCount()**: Tree::DropSfx 83 + t % 3 (0x74BCB6), Tree::ApplyWaterSpell **120** + t % 9
  (0x74C4B3), PhysicsObject::GameTurnUpdate 69 + t % 5 (0x645BF8), CameraModeNew3::FlyToPosFoc 46 + (t & 3) (0x458986),
  and the pile fn_0066D1A0.

### openblack audio architecture

One folder per layer of the original, the facade at the root. The game only includes `Audio/Audio.h` (and, for what is
not effects, its service's header: `GameMusic.h`, `Guidance.h`, `Voices.h`...).

> **Code rules.** New state lives in ECS components or Locator services, not in new globals; banks and other files load
> through the resource caches; resources are owned with RAII and standard types (`std::unique_ptr`, `std::optional`,
> `std::span`, `std::array`); the pure logic (the QMixer laws, the music engine, GameMusic, Guidance) is unit tested in
> `test/` with fakes, never through the locator; comments describe behaviour in plain English, with no decompiled
> names or addresses (those belong here). See [the conventions](../refactor/README.md).

```
          game (ECS, hand, camera, Magic, PSys, Worship, CHLApi, Help, menus, Debug)
              │ #include "Audio/Audio.h"                    ▲ GameQueries (std::functions registered by
              ▼                                             │ Game.cpp and ecs::audio_queries)
 4. Audio.h        facade: GAudio's signatures (PlaySoundEffect, StopSoundEffect, PlayAnimationEffect, tags,
                   RegisterObject, Counter, TickCount, Init/ProcessTurn/ClearMap/OnFocus...) · GameQueries.h
 3. Services/      SoundTags · SoundMap + AtmosBanks · GameMusic + ThingMusic · Voices · Advisor · Guidance ·
                   SpookyVoices · ScriptSound · ScriptAudioState · AnimationSounds · LanternSounds · SpellSounds
 2. GAudio/        AudioSystem (filters, owners, masters, ProcessTurn) · GameSfx (the 5 variants of
                   PlaySoundEffect, counters) · Banks (LHBankRegister of every .sad) · BankTables (0x9CB3F8,
                   0x9C9748)
 1. LH/            LHaudio/QMixer emulated: SamplePlay (16 channels, the DLL's generator) · QMixerLaws · AnimEffects
                   (+ AnimEffectBank) · MusicBank · MusicEngine (LHMusic) · MusicStream (its 6 tracks and the thread)
 0. Device/        Device (the only one that calls OpenAL; GetTickCount) · AlSampleOutput · SampleOutput · WaveBuffers ·
                   Sound (the record of a sample) · decoders (audio::codec: WaveFile, MpegAudio)
```

**Who calls whom.** Downwards, except for what the original also does upwards:
- `GAudio/AudioSystem` distributes the turn (GGame::EndTurn 0x54E960 → GAudio::ProcessAudioGameTurn 0x427080) among the
  services: `sound_map::Update`, `lantern_sounds::ProcessTurn`, `tags::ProcessSoundTags`, ProcessMusic, the ambience
  banks. This is what GAudio does in the original.
- The services use the GAudio functions declared in `Audio.h` (PlaySoundEffect and its family) and `AudioSystem.h`
  (`Queries`, `ListenerPoint`, `SurfaceType`, `IslandAltitude`, `OwnerSoundPosition`).
- `LH/` uses the types of `GAudio/` (`BankId`, `SfxBank`, `MusicType`) and GAudio's `OwnerSoundPoint` (was `Get3DSoundPos`; fn_00427200, the
  3D function that the game registers in the DLL, 0x426E6B). The older names in `SamplePlay` (`SetGameSound`,
  `IsInsideCitadel`...) forward to GAudio.
- `Device/` knows nobody above it, except the bank registry that `WaveBuffers` reads (`banks::ReadWave`).

**What the audio asks the game**: `audio::GameQueries` (`src/Audio/GameQueries.h`). No file in `src/Audio`
includes an ECS component or `ECS/*`:
- `Game.cpp` (`MakeMusicQueries`) registers those for the game state: land, turn, camera, position of a thing
  (`thingPosition`, the owner `Owner::Thing`), hand, widescreen, HelpSystem, night...
- `src/ECS/AudioQueries.{h,cpp}` (`ecs::audio_queries::Fill`, called from `MakeMusicQueries`) registers those that read the
  ECS registry and its systems: `surfaceType` (`ecs::sea_cells::GetSurfaceType`, the single GSoundMap::GetSurfaceType
  0x71D8E0), `weatherSmooth` (`weather::atmos::GetWeatherSmooth` 0x835180), `cameraAlignment` (GAudio+0x190 as
  fn_005E2240 writes it from `ecs::effects::alignment::GetInterfaceAlignment`), `animatedThing` (what fn_00516510 reads:
  position, TurnsSinceStateChange and, for a villager, alive/child/woman/house), `animationClipName` (LoadAllAnimations
  0x550180) and `streetLanterns` (the list g_game+0x205C34 with Object::GetHeight 0x638120). Also the test hooks
  that move the camera (`OPENBLACK_AUDIO_TEST_VIEW` / `_ANIM` / `_LANTERN`, `ecs::audio_queries::RunTestHooks`).
- Objects that are not things (ParticleSound, the fire, the hand...) give their point with `RegisterObject` (`Owner::Object`).
- An unregistered query gives the value of a game without that system (each one says so in its comment).

**A single place for each thing**:

| what | where |
|---|---|
| OpenAL (device, sources, buffers, axes) | `Device/Device.{h,cpp}`, `audio::device` |
| GetTickCount (real ms) | `device::TickCount()` → `game_clock::TickCount()`; the public API is `audio::TickCount()` |
| the DLL's generator (rand from its CRT, srand(time(0)), LH_AudioSystem::Rand) | `sample_play::Rand`, `SeedRand`, `AlternatingRandom` (was `AudioSystemRand`), `Random` (`LH/SamplePlay.h`) |
| GRand::LocalRand / LocalFloatRand (the game's, g_game+0x205A3C) | `guidance::LocalRand` / `LocalFloatRand` (`Services/Guidance.h`); `tags::RandomSample` uses it |
| reading a .sad | `GAudio/Banks.cpp`, `audio::banks` |
| the surface at a point | `ecs::sea_cells::GetSurfaceType` (game); inside the audio `audio::SurfaceType` (query) |

**Where each new thing goes**:
- a sound from a game system: in the system's file, calling `Audio.h` with the original caller's arguments
  (no default arguments); no `AudioManager`, `PlaySound` or `CreateEmitter` (they do not exist);
- a new GAudio service (GConfirmation, the creature...): `Services/`, with its header, called per turn
  from `AudioSystem` if the original does it from ProcessAudioGameTurn;
- a piece of game data that the audio needs: a new `GameQueries` field with its neutral value, registered in
  `Game.cpp` or, if it reads the ECS, in `src/ECS/AudioQueries.cpp`;
- a DLL function (LHSample*, LHMusic*, LHAtmos*): `LH/`; something for OpenAL: `Device/`.

**File map**:

| file | layer | original | what it is |
|---|---|---|---|
| `Audio.h` | 4 | GAudio | the public API |
| `GameQueries.h` | 4 | — | what the audio asks the game |
| `Services/SoundTags.{h,cpp}` | 3 | SoundTag 0x71E300..0x71ED90 | sounds tied to a thing or a point (`audio::tags`) |
| `Services/SoundMap.{h,cpp}` | 3 | GSoundMap 0x71D6F0 | ambience zones around the camera |
| `Services/AtmosBanks.{h,cpp}` | 3 | InitAtmos 0x428EF0, LHAtmos | the 14 ambience banks and their mixer |
| `Services/GameMusic.{h,cpp}` | 3 | GAudio (music) 0x427DF0 | ProcessMusic and its sources, script music |
| `Services/ThingMusic.{h,cpp}` | 3 | ThingMusicInfo | ATTACH_MUSIC |
| `Services/Voices.{h,cpp}` | 3 | voice table 0x915D40 | voices for the texts and the script |
| `Services/Advisor.{h,cpp}` | 3 | HelpDude | the advisors and their lip-sync |
| `Services/Guidance.{h,cpp}` | 3 | GGuidance 0x71AB10..0x71D490 | villager and advisor comments; GRand::LocalRand |
| `Services/SpookyVoices.{h,cpp}` | 3 | GSpookyVoices 0x72E130 | night voices |
| `Services/ScriptSound.{h,cpp}` | 3 | effects CHL | PLAY/STOP_SOUND_EFFECT, script tags |
| `Services/ScriptAudioState.{h,cpp}` | 3 | GScript +0x84..+0x9C | script audio switches |
| `Services/AnimationSounds.{h,cpp}` | 3 | fn_00516510, Tree::Draw | sound events of the clips (`Fire`, `PlayFromTable`) |
| `Services/LanternSounds.{h,cpp}` | 3 | GStreetLantern +0x60 | the tag of each street lantern (`SetOn`, `ProcessTurn`, `Clear`) |
| `Services/SpellSounds.{h,cpp}` | 3 | PSysSound fn_006D11A0 | particle sounds (miracles) |
| `GAudio/AudioSystem.{h,cpp}` | 2 | GAudio 0x426D40 | state, filters, owners, masters, turn |
| `GAudio/GameSfx.cpp` | 2 | GAudio::PlaySoundEffect 0x429D60..0x42A330 | the PlaySoundEffect family, counters |
| `GAudio/Banks.{h,cpp}` | 2 | LHBankRegister from fn_00429CB0 | registering and loading banks |
| `GAudio/BankTables.h` | 2 | 0x9CB3F8, 0x9C9748 | effect and music bank tables |
| `LH/SamplePlay.{h,cpp}` | 1 | LHSample* | the 16 channels; the DLL's generator |
| `LH/QMixerLaws.{h,cpp}` | 1 | QMixer | volume, distance, polar, pitch |
| `LH/AnimEffects.{h,cpp}`, `LH/AnimEffectBank.h` | 1 | LHSamplePlayAnimEffect 0x10014A20 | anim-effect tables |
| `LH/MusicBank.{h,cpp}` | 1 | LHBankRegister (music) | MP2 segments of a music bank |
| `LH/MusicEngine.{h,cpp}` | 1 | LHMusic | 6 tracks, fades, queue |
| `LH/MusicStream.{h,cpp}` | 1 | LHMusic + QMixer | the 6 tracks on the device and the 120 ms thread |
| `Device/Device.{h,cpp}` | 0 | LH_AudioSystem::Create, QMixer, GetTickCount | OpenAL |
| `Device/AlSampleOutput.{h,cpp}`, `Device/SampleOutput.h` | 0 | QSWaveMix* | the channels in OpenAL (and their interface) |
| `Device/WaveBuffers.{h,cpp}` | 0 | fn_100032D0, ACM | decoded waves and their buffer |
| `Device/Sound.h` | 0 | LH_SampleInfo (+.sad) | the record of a sample |
| `Device/*AudioDecoder*`, `Codec/` | 0 | ACM / MPEG decoder | audio::codec (WaveFile, MpegAudio) |
| outside: `src/ECS/AudioQueries.{h,cpp}` | — | — | the queries that read the ECS, test hooks |

Rules:
1. **A single entry point.** Outside `src/Audio` nobody calls OpenAL or `CreateEmitter`/`PlaySound`.
2. **Channels, not entities.** Each channel has its own AL source and there is no `AudioEmitter` in the ECS registry.
3. **A sample is (bank, number).** The key `"<file>.sad/<n>"` is case-insensitive.
4. **Everything runs per turn**, except `HelpDude::UpdateSaySentence` (per frame, 0x5BDE48, with a delay in real ms
   0x5BB554), the camera woosh (`GetTickCount & 3`) and the music thread (120 real ms).
5. **The audio does not include ECS components** (from `ECS/*` only the pure maths `MapCoords.h` and
   `Common/GUtilsDistance.h`, without registry or components). What it needs from the game it asks through
   `audio::GameQueries` (`src/Audio/GameQueries.h`), some `std::function`s registered by `Game.cpp` and
   `ecs::audio_queries` (`src/ECS/AudioQueries.cpp`). A query without an owner returns the value
   of a game without that system: no video, land 0, no camera, no widescreen, alignment 0, no towns, outside the citadel and the
   fight and dance music branches as false (the chant, without a worship site: `chantSite` empty).
6. **The logic is pure and is tested without AL**, with fake sinks.
7. **A single engine.** A single OpenAL device (`src/Audio/Device/Device.{h,cpp}`, `audio::device`): the channels
   (`AlSampleOutput`), the buffers (`WaveBuffers`) and the music (`MusicStream`) request sources and buffers from it; nobody else
   includes `AL/al.h`. A single bank load (`src/Audio/Game/Banks.{h,cpp}`, `audio::banks`), called from `audio::Init`.
   There is no `AudioManager`, `AudioPlayer`, `AlCheck`, `SoundGroup` or `Locator::audio`.
8. **Cyclic counters go in an `enum class Counter`** with the address in the comment, not with addresses
   as keys. The API has no invented default arguments.
9. **One generator and one clock.** The DLL's draws go through `sample_play::Rand` / `Random` and GRand's through
   `guidance::LocalRand`; real milliseconds (GetTickCount), through `device::TickCount` (`audio::TickCount` in the
   API). Nobody in `src/Audio` reads `Locator::rng` for the DLL or `std::chrono` for GetTickCount.

### State of the effects engine in openblack

**Done.** There is a single channel engine: the 16 channels of
`audio::sample_play` (LHSamplePlay), each with its own OpenAL source and **outside the ECS registry**, behind
GAudio's filters (`AudioSystem`) and the public API `src/Audio/Audio.h`. The whole world (hand, trees, rocks,
camera, physics, buildings, boat, piles), the effects CHL, the miracles
(`SpellSounds`, `FireSound`, `HandSpellSeed`, `Gesture`, `HandMagicFX`, `SpellSeed`, `WorshipSpellIcon`, `MagicTeleport`,
`Fireball`, `OneOffSpellSeed`, `FireGraphic`) and the debug panel go through there. **There are no emitters** nor a second
engine ([below](#a-single-engine)): `AudioManager`, `AudioPlayer` and `Locator::audio` were removed; the
device is `audio::device` and the banks are `audio::banks`. `src/Audio` ([below](#the-structure-layers-one-generator-one-clock))
is layered, does not include the ECS and has a single DLL generator and a single clock.

## Banks and formats

### Table of effect and dialogue banks

**Faithful.** It is `AUDIO_SFX_BANK_TYPE`, in table 0x9CB3F8 (11 `const char*`), and the bank is stored at GAudio+0x3A8 + 4·type.
They are all registered at startup with `LHBankRegister(path, 0)`. The paths have no language folder: the language is
decided by whatever is installed.

| type | bank | path | typical priority (data) |
|---|---|---|---|
| 0 | — | null | |
| 1 | InGame | `audio/sfx/game/ingame.sad` | 0..2000 |
| 2 | Editor | `audio/sfx/game/editor.sad` | 0..300 |
| 3 | Spells | `audio/sfx/game/spells.sad` | 0..6000 |
| 4 | Creature | `audio/sfx/creature/creature.sad` | 200/400 |
| 5 | ScriptSfx | `audio/sfx/script/scriptsfx.sad` | 200..9999 |
| 6 | HelpSprites | `audio/dialogue/HelpSprites.sad` | 9999 |
| 7 | Villagers | `audio/dialogue/Villagers.sad` (on disk `villagers.sad`) | 9999 |
| 8 | VillagersBanter | `audio/dialogue/VillagersBanter.sad` | 2 |
| 9 | SpellDialogue | `audio/dialogue/SpellDialogue.sad` | 2000 |
| 10 | Guidance | `audio/dialogue/Guidance.sad` | 20 |

The game also registers other banks:
- One **per creature species**: `audio\sfx\creature\%s.sad`, in `LH3DCreature::LoadBinary` 0x4EBD81 →
  `GAudio::RegisterBank` fn_00428620 (+0x5288).
- The 13 ambience ones (`Audio\SFX\Atmos`).
- The one for a video (`PlayFullScreenMovie` 0x54D920) and `audio\music\intro\trailer.sad` (0x6427CE).
- The HelpDude one (fn_005BB1B0, 0x5BB1E6, **(inferred)**).

- Path strings of 0x9CB3F8: 1 0x9CB540, 2 0x9CB524, 3 0x9CB508, 4 0x9CB4E8, 5 0x9CB4C8, 6 0x9CB4A8, 7 0x9CB488,
  8 0x9CB464, 9 0x9CB440, 10 0x9CB424. Entry 84 of 0x9C9748 (`creatureendsequence.sad`) is at 0x9C99E8.
- fn_00429CB0 first clears GAudio+0x3A8[0..10] (0x429CB5..0x429CC0). fn_0042A350 (called at 0x429CC4) loops over types
  0..10 (0x42A356..0x42A362). fn_0042A390 registers a bank only if its path is not null and the slot is still empty
  (0x42A39D..0x42A3BA), and opens the path as it is, with no language folder (0x42A3A3..0x42A3B4).

openblack: `audio::SfxBank` and `k_SfxBankPaths` in `src/Audio/Game/BankTables.h`; the banks are registered by
`audio::banks` ([below](#a-single-engine)).

### .sad format

**Faithful.** A .sad is `"LiOnHeAd"` followed by blocks
`{char name[32]; u32 size; data}`. openblack's `PackFile` already reads them.

| block | contents |
|---|---|
| `LHFileSegmentBankInfo` (532 B) | u32 a, u32 b, u32 **music** and a 520 B title. Music is 1 in the 58 banks of `Music\`, in `Intro\trailer.sad` and in `Dialogue\MissionariesVerse1..3.sad`; 0 in the rest. a/b are 0 except in `ocean.sad` (7, 6), **(inferred)**: editor version |
| `LHAudioWaveData` | the concatenated waves |
| `LHAudioBankSampleTable` | u32 `n | (nAtmos << 16)` + n records of 0x280 B (nAtmos = n in the 13 of `Atmos\`) |
| `LHAudioAnimArrayTable` | u32 rows, u32 width (6), rows × 6 s32: anim-effect tables |
| `LHAudioWaveNumTable` | the {count, samples…} lists that column 6 points to |
| `LHAudioListViewText` | bank editor text, not used |

Sample record (0x280 B):

| off | field |
|---|---|
| +0x000 | path of the original .wav (char[256]). In the dialogues it is `K:\4frosty\Spanish\…\HELP_TEXT_….wav`: it is the basis of the voice table |
| +0x104 | id, 1-based: the sample number the game uses |
| +0x108 | sample whose wave is used (clones: editor 1..6 → 1); the cache compares this |
| +0x10C / +0x110 | size and offset in WaveData. Size 0 = empty sample (InGame 165, spells 31) |
| +0x118 | u16 clone group (mode 3) or, in music, the sync group (only in segment 0) |
| +0x11A | u16 ambience group |
| +0x124 | WAVEFORMATEX (+0x128 = Hz) |
| +0x138 / +0x13C | loop start and end in frames (−1 = none), taken from the .wav's `cue` chunks |
| +0x140 | description (in music, the `!n=label` markers) |
| +0x240 | priority |
| +0x244 | override flags (0x20 volume, 0x40 loops, 0x80 min, 0x100 max, 0x200 scale, 0x400 mode…) |
| +0x248..+0x27C | loops, pan, volume/userParam (+0x25C; userParam = >> 16), pitch and deviation, min/max/scale (+0x268/+0x26C/+0x270), mode (+0x274), +0x278 (if it is 0, `LHSamplePlay` gives an error at 0x100118FD), ambience frequency (+0x27C) |

Wave formats in the data:

| format | where |
|---|---|
| RIFF 16-bit PCM, 22050 Hz (11025 in 1 of InGame; 44100 in 210 of VillagersBanter and 1 of Scriptsfx) | almost all effects |
| RIFF MS-ADPCM (0x0002) | high, rain, wind, stream, 3 of InGame, 27 of spells, 2 of Scriptsfx, 23 of VillagersBanter |
| RIFF **MPEG-2 layer II (0x0050)**, 22050 Hz, 64 kbps mono | all of HelpSprites (1923) and villagers (1328); Guidance 129/227, SpellDialogue 9/35, Scriptsfx 21, country 17, InGame 2 |
| raw MPEG layer II frames | the 58 banks of `Music\` and MissionariesVerse1..3 ([below](#music-banks-mp2-segments)) |

- `LHBankGetNumberOfSamples` 0x10002E90 is the low u16 of the sample table's first u32 (bank+8). The high u16 (bank+0xC)
  counts the atmos samples and is 0 in music banks. `HelpDude::SaySentence` uses it at 0x5BB389.
- `LHBankRegister` opens the file with `LHReleasedFile::Open(path, 2)` (0x1000228A..0x100022FE). It returns 0 if the file
  is missing or has no `LiOnHeAd`, no `LHFileSegmentBankInfo` (0x10002368), no `LHAudioWaveData` or no
  `LHAudioBankSampleTable`. It looks blocks up by name (fn_100183A0). With inMemory 0, bank+0 = 1 and the file stays
  open at bank+0x18 (0x10002792).
- It reads the sample table as n × 0x280 bytes without checking the block size (0x100026AA..0x10002733).
- bank+4 is the 3rd u32 of `LHFileSegmentBankInfo` (the music mark, 0x10002357..0x100023BD).
- Anim tables: u32 rows (+0x130), u32 width (+0x12C), then rows × width s32 (+0x124), read at 0x100027DD / 0x100027F8.
  The wave-number lists are read by `LHFileSegmentAnimArrayWaveNumbers` 0x100028E8. Every bank of the game has width 6.
- `LHFindAttribRow` 0x10014420: a row matches when each of its first width − 1 columns is the wildcard or equal to the
  key's (0x100145A1..0x100145B6).
- Music-bank getters: `LHGetFirstBankSample` 0x10002EB0 gives segment 0 (bank+0x14). `LHBankGetMusicGroupId`
  0x10002F30 gives −1 for a bank that is not music (0x10002F4A). The distance getters' "no distance" value is −1.0f
  ([0x10030304], 0x10002F05 / 0x10002F25).
- In an ambience bank, a sample whose ambience frequency (+0x27C) is 0 is a loop sample (fn_100011B0).

openblack: `AudioBankInfo` (the 3 u32) and `PackFile::IsAudioMusicBank()` in `components/pack` (**faithful**: 0x10002240,
`LHIsMusicBank` 0x10002EE0). `Game.cpp` still recognises music by the ".mpg" extension of the first name.

### Wave loading and cache

**Faithful.**
- `LHBankRegister(path, inMemory)` 0x10002240. The game **always** passes `inMemory = 0` (0x426EEE, 0x428F30, 0x42A3B1,
  0x4EBDB9, 0x54D991, 0x5BB1DE, 0x6427C1). On registration only the headers are read and the file stays open.
  `LHMusicGetTotalGroups` (sys+0x40) is the maximum of the u16 +0x118 of the music banks.
- Samples are **not streamed**: they are loaded whole on first use (0x10011420 → fn_100032D0) into a **FIFO cache**
  (list 0x10042F80) with a budget of RAM/8 ([0x100383B8]). The victim is the oldest wave that no channel
  is using.
- QMixer converts ADPCM and MPEG with ACM. With opts+0x164 (`keepPcm`), the DLL leaves the PCM in SampleInfo+0x80/+0x84
  (fn_10010910, 0x10011CB3/0x10011E25), and HelpDude copies it for the lip-sync (0x5BB57B..0x5BB5C9).
- openblack (`src/Audio/Device/WaveBuffers.*`): the .sad files are read whole at startup, except the dialogue ones
  (types 6..10, `Audio\Dialogue`): of those only the headers are read (`PackFile::ReadAudioHeaders`) and each
  wave is read from the file when it is decoded (`Sound::waveFile`, `banks::ReadWave`), like `LHBankRegister(path, 0)`.
  That the rest of the banks are read whole is **(approximate)**: the original registers them all this way (0x426EEE). Each sample is
  **decoded only once, on first use**, into an AL buffer that is kept (`Sound::bufferId`) until the audio is closed.
  **(approximate)**: one buffer per sample record, not per wave +0x108 (clones are each decoded), and with no
  budget or FIFO eviction (RAM/8 only matters with less than 1 GB). RIFF with wFormatTag 0x50 (or 0x55) → the `data`
  block to the MPEG decoder (layer II), like ACM; any other RIFF → the wave decoder; anything that is not RIFF → raw
  MPEG, then a wave file ([the decoders](#the-decoders-audiocodec)). The +0x138/+0x13C span goes to the
  buffer as `AL_LOOP_POINTS_SOFT`.

### The decoders (audio::codec)

**Faithful (checked bit by bit).** openblack's own decoders, `src/Audio/Codec/`, give the samples the earlier libraries
gave (dr_wav 0.14 and dr_mp3 0.7, which they replaced) for every sample of every bank of the game, `Audio\` and
`CreatureIsle\Audio\` (81 850 checks: 6208 PCM, 219 MS-ADPCM, 4919 MPEG waves, every music segment in order and from
a fresh decoder).

- `WaveFile` reads what dr_wav read, so that nothing that played stops playing: RIFF, RIFX, RF64, Wave64, AIFF and
  AIFC; integer PCM of 1 to 8 bytes (8-bit unsigned; the top 16 bits kept), IEEE float 32/64, A-law, mu-law, MS-ADPCM,
  IMA ADPCM, WAVE_FORMAT_EXTENSIBLE. Checked against dr_wav with 450 000 random files of every container and format,
  broken ones included: 0 differences.
- `MpegAudio` is MPEG-1/2 layer II (the voices, the music): the frame search, the layer II tables and the
  single-precision synthesis in the order of minimp3's SSE path, with its two roundings (half away from zero for two
  samples of each 64, half to even for the rest). GCC and Clang compile it with `-ffp-contract=off`.
- Kept as before, quirks included: a wave of an unknown format opens as silence of its data's length.
- **Fixed (the user's decision):** an MS-ADPCM wave ends at its "fact" length. The old library ignored the chunk and
  played the padding of the last block: the 219 MS-ADPCM waves of the game lasted 5 to 1009 frames (550 on average)
  too long. Their samples up to that length are unchanged. Windows' own MS-ADPCM codec (msadp32.acm, the codec QMixer
  calls) gives the same samples, but it decodes whole blocks, so it does not tell whether QMixer stopped at the
  "fact" length.
- **Different from before:** a header that claims more than 2^28 samples is refused as broken (dr_wav tried to
  allocate it, which threw); no file of the game comes near it.

### The Audio Player and Audio banks windows (debug bar)

A debug tool of openblack's, not in the original. The **Audio Player** window (*Debug → Windows*, `src/Debug/Audio.{h,cpp}`)
has two switches at the top, on by default, above its Sound, Music and Channels tabs. The **Audio banks** window
(`src/Debug/AudioBanks.{h,cpp}`) is listed beside it:

- **Game sounds** and **Game music** (`audio::OutputSwitches`, kept in `Locator::audioState`): off, the 16 sample
  channels (`AlSampleOutput::ApplyGain`) or the 6 music channels (`MusicStream::ApplyGain`) get gain 0 at the output.
  Nothing else changes: the channels keep playing, the music keeps decoding and calling back, and turning a switch on
  gives each channel its own gain again (at the next frame for the samples, the next pump for the music). On, the gains
  are the same as without the switches. They stay as they were set while the window is closed.
- **Audio banks** lists every `.sad` under `Audio\` and `CreatureIsle\Audio\`, and a bank's samples (or music segments) with
  their size and format. Each one decodes as the game decodes it (`wave_buffers::Decode`), showing its channels, rate,
  length and decode time; "Decode all" times the whole bank. Play uses a source of its own, a 2D one at full gain
  outside the 16 channels, so the game's channels and the two switches do not touch it.

### Channels, priorities and loops

**Faithful:**
- **16 channels** shared by 2D, 3D, ambience and dialogue. Modes 1/2/3 and a clone group.
  - A new sample steals the lowest-priority channel if that priority is lower than its own.
  - Because of the priorities in the data, help dialogue is never lost and ambience yields first.
- **3D**: `QSWaveMixEnableChannel(…, 0x20, f | 0x100)`, `SetDistanceMapping {min, max, scale}`.
  - Distance law: d ≤ min → 1; d > max → 0; otherwise, `min / ((d − min)·scale + min)` (0x1802CE50).
  - Volume: `floor(m·v/127)·258/32767` (0x100133C1).
  - `LHSampleUpdate3DChannels` 0x10014310 only moves the 3D channels with track. It stops the channel if its owner is gone
    (0x1001439D) or if it exceeds its maximum distance (0x100143BC).
- **Loops**: `QSWaveMixPlayEx(…, 0x421, wave, loops, &p)` at 0x10012949, with `lStart..lEnd` if both are ≠ −1
  (+0x138/+0x13C).
  - Loops: 0 = once, −1 = forever, N = N repetitions of the span and then it continues to the end. Whether that is N or N+1 passes
    remains **(inferred)**.
  - Examples: G_VillageBell InGame 30 (5 loops, 0..27400 of 57855), G_PickUpFood 44 (−1, 47743..110078), the editor's
    doves 342/343 (5), jungle (3..5), swamp (2..6), country bird15 (6).
- **`LHSampleStop`** (0x10012C50 by sample or owner, 0x10012DF0 by channel, and the cutoff in
  `LHSampleUpdate3DChannels` 0x1001439D/0x100143BC): `QSWaveMixSetPanRate(20 ms)`, `SetVolume(0)`, `Sleep(20)`,
  `SetPanRate(100)` and `Flush`: a 20 ms fade, and the game waits those 20 ms. openblack fades the same way
  (`SampleOutput::StopRamped`: four 5 ms gain steps, **(approximate)**: OpenAL has no pan rate), but **asynchronously**:
  the channel is stopped and free for the logic as soon as the call returns, as it was when the wait ended, and its source
  fades and is flushed on a fader thread of `AlSampleOutput` (`RampedStops`) while the game goes on. It sounds the same;
  only the instant of the cut may move by a few ms. A new start on that channel takes another source, so it neither
  fades nor is cut; `StopAll` cuts a fade still sounding at once.
  `LHSampleStopAll` 0x10012BF0 and resetting a channel cut abruptly.
- **`ReleaseLoop`** (`LHSampleReleaseLoop` 0x10012F20): acts on the **first** matching channel that is in use, with
  `StopChannel(0x1000)`. It sets the remaining loops to 0 and the current pass finishes. If that first channel is inactive,
  it returns 0 without searching further.
- **No fade API**: each caller does its own.
- GAudio's ctor writes the 16 channels at 0x426DF5. The game never sets the registry override "MaxSamp".
- Channel record: bank +0x04, owner +0x18, sample +0x1C, clone group +0x28 (.sad +0x118), the priority it started with
  +0x70 (.sad +0x240), in use +0x8C. The end callback 0x100108C0 clears +0x8C.
- Channel choice in LHSamplePlay 0x100113B0 (allocation 0x10011020, then start 0x10011420). It depends on the play
  mode: options +0x50, 3 by default, or the .sad's +0x274 when its flag 0x400 is set.
  - Mode 1: a free channel.
  - Mode 2: nothing at all, not even a new position, while a channel of the same bank and owner is playing the same
    sample, or one of the same clone group (> 0) is playing. Otherwise a free channel.
  - Mode 3: the channel of the same bank, owner and sample (restarted); else one of the same clone group > 0
    (restarted); else a free channel.
  - No free channel (any mode): the channel with the lowest priority, if it is lower than the sample's, is restarted.
    A channel at priority 0 is taken at once (0x100112CA). Otherwise the sample does not play.
- The "restarted" flag is passed on to the start (0x10011350). The channel gets the sample's priority and clone group
  at 0x10012AAA.
- `LH_SamplePlayOptions` (ctor 0x10010E90):
  - +0x04 bank, +0x08 is3D, +0x0C track, +0x10 the GAudio variants' 6th argument;
  - +0x1C caller mask, +0x20 owner, +0x24 sample, +0x28 volume, +0x2C (stored but not used by LHSamplePlay);
  - +0x30 point, +0x3C offset added to it (0x10012159), +0x48 pitch, +0x4C loops, +0x50 mode;
  - +0x54 / +0x58 / +0x5C min / max / scale (defaults 1, 9999, 0.3), +0x164 keep the PCM.
- +0x10 makes LHSamplePlay use 3D flags 3 instead of 0x11 (0x10012050); its meaning is unknown.
- The 3D cutoff of 0x429E6F measures the distance to point + offset.
- Caller mask +0x1C: 0x1 pitch, 0x20 volume, 0x40 loops, 0x80 min, 0x100 max, 0x200 scale, 0x400 mode. A .sad field
  applies only where its +0x244 flag is set and the caller's bit is clear (0x100119D4..0x10011B84). The mode follows
  this rule too (0x10011B5C).
- Start pitch (0x1001278B..0x1001283B, unsigned integers): p = the pitch (0 → 100); d = deviation · p / 100;
  p = p − d + rand · 2d / 32767 (the DLL's rand, 0..32767); 0 → 100 again. The pitch is the .sad's when its flag 0x1 is
  set, otherwise the options' +0x48.
- A 3D channel is `QSWaveMixEnableChannel` 0x20 with the 3D flags (0x10012050; flags 0x103 / 0x111, 0x10012065). A 2D
  channel (0x100125AB) has no position and no distance mapping.
- QMixer's distance law (0x1800ACDF..0x1800AE1A) has two more flags, 0x800 "clamp at max" and 0x1000 "linear". Neither
  LHSamplePlay nor LHMusic uses them. Beyond max, QMixer mutes the channel but keeps it playing (0x1800ADDF).
- `QSWaveMixSetVolume` takes 0..32766 and QMixer stores vol / 32767 (0x18007AE5 / 0x18007AE8). The division by 127 is
  the magic 0x2040811 (× 129 × 2 = 258).
- `LHSampleReleaseLoop` = `QSWaveMixStopChannel(0x1000)` → QMixer 0x18007520.
- `LHSampleStop` on one channel (0x10012DF0) stops the first channel of that channel's (bank, owner, sample). While the
  audio is off it does nothing, except on an atmos channel (0x10012E18..0x10012E22; by sample: 0x10012D5B).
- `LHSampleGetPlayPosition` 0x10014C00: −1 while off (0x10014C09). It looks at the first channel of the bank and owner
  without comparing the sample (0x10014C29..0x10014C31). A channel in use gives `QSWaveMixGetPlayPosition(…, 2)` in ms;
  otherwise −1 (0x10014C6E).
- `LHSampleGetPercentageDone` 0x10015180: it gives 1 when no channel matches; it walks every channel of (bank, owner,
  sample) (0x100151AB..0x10015207); a non-atmos channel met while the audio is off ends it with 1
  (0x100151C6..0x100151D0); the first channel in use gives position_ms / (bytes · 1000 / (channels · rate · 2))
  (0x10015243..0x1001527D).
- `LHSampleSetVolume` clamps to 0..127 (0x10013473..0x10013487) and changes the first channel of the triple only if it
  is in use (0x100134BE).
- `LHSampleSetPitch` does nothing on a channel that is not in use (0x1001357E) or when the pitch is unchanged
  (0x10013588).
- `LHSampleGetInfo` 0x10013F60 is the first channel of the triple, in use or not; its +0x40 is the loops (GAudio's
  fn_0042A460).
- `LHSampleClearInfoList` clears +0x1C, +0x18 and +0x04 of each channel (0x100142EB..0x100142F0).

### Master volumes, focus and reset

**Faithful.**
- **Master volumes**:
  - There are two, from 0 to 127: `AudioSampleMasterVolume` and `AudioMusicMasterVolume`, in
    `HKCU\Software\Lionhead Studios Ltd\Black & White\BWSetup`.
  - They are read in fn_00428250 (at startup and when cancelling the options, 0x5158B8) and saved in fn_004282B0
    (`ToBeDeleted` and accepting the options, 0x5158EE).
  - The options slider (`DialogBoxOptions` 0x5145A3/0x5145AE) does `LHMusicSetMasterVolume(ftol(slider·127))`.
  - Without the registry key, they are 127 (`[0x10056280] = 0x7F`, 0x1000DE08).
- **Alt-Tab** (0x7DE6D0 → 0x642470 → 0x428720):
  - It is not focus, it is **minimising**: `GameWindowProc` sets state 0x8002 with wParam 1 (0x7DBFF6..0x7DC009,
    **inferred** WM_SIZE SIZE_MINIMIZED) and sub_7DE8D0 (0x7DE8DC) calls `AltTabDeactivate` 0x7DE6D0. On restore
    (0xF120 sets the flag [0xE8C0FB], 0x7DC23A..0x7DC245, **inferred** WM_SYSCOMMAND SC_RESTORE),
    `ProcessWindowMessages` 0x7DB9DB calls `AltTabReactivate` 0x7DE6F0. While it is minimised,
    `ProcessWindowMessages` does not leave the message loop (`AltTabbedAway`, 0x7DB9E0): the whole game stops.
  - On minimising, `LHGlobalSwitch(0)`: `LHWaveSwitch(0)` (StopAll) and `LHMusicSwitch(0)` (`LHMusicStop(0)`).
  - On returning, `LHGlobalSwitch(1)` only reactivates the flags: **nothing is resumed**.
  - Alt-Tab without minimising does not touch the audio (WM_ACTIVATEAPP 0x7DC073 only clears keyboard and mouse): the
    music continues (confirmed by the user); a real minimise cuts it, as openblack does.
- **`GAudio::Reset`** 0x426CA0 (from `GGame::Init`). In order:
  1. Zeroes +0x18C, pos[group] (fn_00428190) and +0x28/+0x24/+0x180/+0x190, and +0x1C = −1.
  2. `LHMusicStop(0)`, `LHAtmosProcess(0)` and `StopAll`.
  3. `LHGlobalSwitch(0)`, waits until nothing is playing, `ClearInfoList` and `LHGlobalSwitch(1)`.
  4. `ReleaseAllThingMusicInfo` 0x4291B0.
- **Pause**: `PauseGame` 0x54AE20 does not touch the audio; it toggles bit 4 of g_game+0x14 (0x54AE2F). `EndTurn` with that bit
  does GSoundMap::Update (0x54E96F) and ProcessSoundTags (0x54E989) as always and then `AtmosProcess(0)` (0x54E993,
  0x4286C0) instead of ProcessAudioGameTurn.
  Who calls `LHMusicPause` 0x1000EA10 has not been read.
- The options slider 0x64 applies the effects master at once through fn_00428600 (DialogBoxOptions 0x5145A3).
  `MiniDialogBoxOptions::InitControls` 0x515690 sets the sliders to the master volume / 127 (fn_00428680 / fn_00428690).
- `LHSampleSetMasterVolume` ignores the current value and anything above 127 (0x100150F2..0x10015100). It does not
  clamp.
- `LHMusicSetMasterVolume` ignores the current value (0x1000E8C0) and applies the new one to the channels in status 1
  or 4 (0x1000E8ED..0x1000E8F5).
- `GAudio::Reset` writes its fields at these sites: +0x18C = 0 (0x426CA6), fn_00428190 (0x426CAC), +0x28 (0x426CB6),
  +0x24 (0x426CB9), +0x180 (0x426CBC), +0x1C = −1 (0x426CC8).
- The audio-system part of Reset runs only when the system exists (0x426CCF), in this order: LHMusicStop(0) 0x426CD3,
  LHAtmosProcess(0) 0x426CDD, LHSampleStopAll 0x426CE6, LHGlobalSwitch(0) 0x426CF6, a wait until
  LHSampleGetNumberPlaying() == 0 (0x426D01..0x426D13), ClearInfoList 0x426D18, LHGlobalSwitch(1) 0x426D23,
  ReleaseAllThingMusicInfo 0x426D28.
- LHGlobalSwitch includes LHMusicSwitch: (0) stops the music without a fade and makes it inactive; (1) makes it active
  again and restarts nothing.

- openblack:
  - Music volume: `EngineConfig::audioMusicMainVolume` (was `audioMusicMasterVolume`) (127) in the "Music" debug panel. It is not yet
    saved to disk.
  - The music part of `GAudio::Reset` and `GScript::Reset` goes in `Game::LoadMap`.
  - Effects volume: `EngineConfig::audioSampleMainVolume` (was `audioSampleMasterVolume`) (127), applied live with
    `LHSampleSetMasterVolume` (reapplies to the channels in use) and a `ftol(slider·127)` slider in the "Channels" tab
    of the audio panel. It is not saved.
  - Alt-Tab: `SDL_WINDOWEVENT_MINIMIZED/RESTORED` → `audio::OnFocus` → `LHWaveSwitch` + `LHMusicSwitch`. Losing
    focus does not turn anything off, as in the original. The ambience channels are not stopped by `LHSampleStopAll` (0x10012C13);
    in the original the whole game stops when minimised, in openblack it keeps running and the ambience continues
    **(approximate)**.
  - Pause: `audio::Paused` = `AtmosProcess(0)`. openblack's pause has no turn clock (it is called every
    frame), so GSoundMap::Update and ProcessSoundTags do not run while paused ([Pending](#pending)).

### .sas and animation tables

`Data\SmallSounds.SAS` is the text of the sound events per animation clip (`LoadAllAnimations` 0x550180). Together
with the `LHAudioAnimArrayTable` tables of each .sad, it feeds `SamplePlayAnimEffect` 0x42A4B0 → 0x10014A20. The key
has 5 columns and there are 3 actions (0 play, 1 stop, 2 release). The details of the clips and the banter are in
[animation.md](animation.md#clip-sounds). The original has no other audio format. In openblack the tables
of each bank are read once when it is registered ([below](#the-anim-effects-in-the-core)).

- Tree::Draw keys: the bend {c, *, *, 10, 75} (0x74B009, track 0, `push ebp = 0` at 0x74AFE1) and the ambient rustle
  {*, *, 20, *, 70} (0x74B25C, track 1, `push 1` at 0x74B1FA). Both measure |tree − camera| as the distance
  (0x74AFFD / 0x74B250).

### Ambience: GSoundMap and LHAtmos

- The only GSoundMap is at g_game+0x250058. `GSoundMap::Update` 0x71D6F0 runs CalculateRadiusPointAndDistance 0x71D800,
  UpdateFromMap 0x71D720 and CalculateVolumes fn_0071DD60 (the 14 volumes), then Dump 0x71D990.
- UpdateFromMap scans the (2r/10 + 1)² cells around the receiver (11 × 11 with r = 50). It subtracts and adds r to the
  MapCoords (fn_00605490 / fn_00605400, 0x71D77A / 0x71D784). The cells are the high words, read signed, both ends
  included (0x71D790..0x71D7E9).
- `GSoundMap::AddAtmosType` 0x71E260 + `AtmosMapTypeInfo::Add` 0x71D510: the squared distance from the receiver to the
  cell's centre. A cell replaces the kept one only when strictly nearer (0x71D568..0x71D570). The nearest point is
  stored as words by __ftol (0x71D575..0x71D5A4). The starting squared distance is 65535 (0x477FFF00).
- HeightFade fn_0071DAC0: 1 while the camera is below normalAtmosFadeStartHeight above the land at p, 0 above the end
  height.
- The sea's share is fn_0071E210(SEA): sea cells over all typed cells.
- The wind volume is clamped after its formula: above 1 → 1, below 0.01f (dword 0x8C5840) → 0.
- The table of surface names is at 0xC22270 (9 names).
- LHAtmos loop sample (LHSamplePlay at 0x10001A18): AtmosInfo 1, is3D 0, owner −1, loops −1, mode 2, caller mask 0x20,
  volume 0.
- LHAtmos one-shot (0x10001BD7): AtmosInfo = its queue entry, is3D 1, +0x0C 1, relative (+0x14 = 1) at (x, y, 0),
  owner −1, caller mask 0x20. The mode (3), pitch, deviation and distance mapping come from the .sad or the defaults.
- One-shot queue (fn_100011B0, 0x10001304..0x100013A9): a new entry goes before the first one that is due at the same
  time or later. On a reinsertion the walk starts at the head's next, and fn_10001170 pops the head afterwards.

## Music

### Music banks: MP2 segments

**Faithful.**
- There is no LHStream and no loose files. Each music .sad is a normal `LiOnHeAd` bank with the music mark.
- Its sample table consists of **segments** `c:\windows\temp\sectNNNN.mpg` of MPEG-1/2 Audio Layer II, at 22050 Hz:
  128 kbps stereo, 96/64 kbps mono, and 160 kbps in the Outro.
- Each segment has **21 frames = 24192 samples (0x5E80) = 1.0971 s**. They are contiguous and frame-aligned:
  13457 segments were checked without a single failure.
- From **segment 0** come:
  - the sync group (u16 +0x118);
  - the flags (+0x244) that `LHMusicPlay` applies: 0x20 volume (u16 +0x25C), 0x40 loops (+0x248, −1 = forever),
    0x80/0x100/0x200 3D min/max/scale (+0x268/+0x26C/+0x270);
  - the Hz (+0x128).
- **Markers**: fn_1000D9E0 reads the description (+0x140) of each segment, of the form `"!<sample>=<label>!…"`, and
  builds nodes {chunk i+1, sample, label}. It walks the segments from last to first and inserts each node at the
  head. Only `MissionariesVerse1..3.sad` use them (L1..L12 are the verses, P the hits).
- Banks:

  | banks | group | other data |
  |---|---|---|
  | the 24 of `align` | **1** | 430 segments (471.1 s, 7 814 232 B), parallel versions of the same melody |
  | chants | 6..13 | 0x3C0, loop −1, 3D 30/120/2 |
  | citadel | 4 | |
  | CreatureFight | 5 | |
  | the rest | 0 | |

  .sad volumes: Script01 60, Script02..04 65, Epic04 80, Sleg 65, Khazar 65, Gregorian 40, Circus 60,
  CreatureGuide 70, Gregorian3D 50, Circus3D 60; the others 127.
  3D distances: PiperTune_M 15/100/4, Pipercave_M 25/80/4, SingingStonesA 100/200/4, MissionariesBackground
  30/100/4, Whistle* 30/80/2, Circus3D 30/120/3, Gregorian3D 60/120/4.

- A segment's 0x5E80 samples (21 Layer II frames of 1152) is the constant the marker clock uses (0x1000DBF7).
- Marker parsing: after the digits (isdigit, atoi) exactly one character is skipped, the '=' (0x1000DA87). The label
  runs to '\0' or '!' (0x1000DA92..0x1000DAA2), and parsing repeats while the label ended at a '!' (0x1000DAF0). A node
  is malloc(0x30) with the label in 0x20 bytes at +0x10.
- `LHMusicPlay` takes these from segment 0: volume 127 (0x1000E0C5), or u16 +0x25C with flag 0x20
  (0x1000E1D6..0x1000E1ED); loops = the requested ones (opts+0x18, 0x1000E13E), or i32 +0x248 with flag 0x40
  (0x1000E1F1..0x1000E206); the distances, in the order 0x100 max, 0x80 min, 0x200 scale (0x1000E2D9..0x1000E338),
  otherwise the options' +0x30 / +0x2C / +0x34; the Hz (+0x128), copied to channel +0x50 (0x1000E210).
- With the retail banks `LHMusicGetTotalGroups` (sys+0x40, read at 0x100027B5) is 13.

openblack: `MusicBank` (`src/Audio/Engine/MusicBank.{h,cpp}`, **faithful**) covers `Register` (0x10002240), the segments, the
group (0x10002F30), the distances (0x10002EF0/0x10002F10), the flags, the Hz, `GetVolume`, `GetLoops`,
`GetDistanceMapping` (0x1000E30A..0x1000E338), `ParseMarkers` (0x1000D9E0) and `ReadSegment`. The file stays open and
each segment is read when used. A marker without a label gives an empty label **(approximate)**: it does not occur in the data.

### LHMusic: the 6-track engine

**Faithful.**
- **6 channels** `LH_MusicInfo` (sys+0x84, 6 × 0x6C). The **master** (0x10056284) is the last one started: it rises to its
  target, and all the others go down −3 per pass until 0 and are released.
- **"music" thread** 0x1000EB40, launched with `_lhbeginthread("music", 0xF)` at 0x1000DF27:
  - One pass walks the 6 channels and then does `Sleep(120)` (0x1000F726), or `Sleep(5000)` if a read failed
    (0x1000F71F).
  - It queues up to **4 chunks** per channel (0x1005628C).
  - It reads each segment from disk and decodes it with its own MPEG decoder (0x1000F740), with the state continuous
    between chunks; it is only reset when `LHMusicPlay` asks for it (0x1000E448).
- **Fades**:
  - The master rises **+4** per pass (0x1000F59F) only if opts+0x20 asks for it; otherwise, it jumps straight to the volume the
    first time.
  - The other channels, or the master above its target, go down **−3** (0x1000F613).
  - Durations: 0→127 in 32 passes (≈ 3.8 s), 0→80 in 20, 127→0 in 43 (≈ 5.2 s) and 80→0 in 27. The passes are faithful; the
    real duration (120 ms plus the processing time) is **(inferred)**.
- **`LHMusicPlay`** 0x1000DF60:
  - Without a music bank, it does `LHMusicStop(1)`.
  - If the bank is already playing, it is a **re-trigger**: it only changes the target, the fade, the sync and the master.
  - Otherwise, it takes a free channel; if there is none, it does not play. A 7th track is rejected.
- **Group sync** (0x1000ED03..0x1000ED7E, 0x1000F385): with `sync`, the new track starts at the **same chunk and
  the same sample** (`QSWaveMixGetPlayPosition`) as another track of the same group that is playing. This way the
  good↔evil or tribe change does not restart the melody.
- **End-of-chunk callback** 0x1000DC80: advances the audible chunk (+0x48) and, on the last one, moves to state 3.
  States: 0 free, 1 playing, 2 paused, 3 finished, 4 last chunk queued.
- **Volume** QMixer = `floor(floor(cur·sad·258/127)·master/127)` (0x1000F423..0x1000F464). That is, the gain is
  cur/127 · vol.sad/127 · master/127. In 3D QMixer's distance law is applied.
- **Functions**:

  | function | address | what it does |
  |---|---|---|
  | `LHMusicStop(fade)` | 0x1000E530 | with 1 they all go down; with 0 they are cut |
  | `LHMusicStop(info, fade)` | 0x1000E620 | for the master, stops all |
  | `GetInfo` | 0x1000E750 | |
  | `GetStatus` | 0x1000FC00 | |
  | `GetCurrentChunk` | 0x1000FB40 | |
  | `GetMasterInfo` | 0x1000FB70 | |
  | `GetTotalGroups` | 0x1000FB60 | |
  | `SetMasterVolume` | 0x1000E890 | unsigned: above 127 gives 127 |
  | `SetPitch` | 0x1000E970 | between 50 and 250 |
  | `Set3DPosition` | 0x1000FBA0 | |
  | `Pause` / `Restart` | 0x1000EA10 / 0x1000EA80 | |
  | `Switch` | 0x1000EB00 | |
  | `Close` | 0x1000E7A0 | |

- `LH_MusicPlayOptions` (0x50 bytes, ctor 0x1000D970):
  - +0x00 bank, +0x04 volume (127, 0x1000D982), +0x08 / +0x0C / +0x10 copied to channel +0x18 / +0x1C / +0x20;
  - +0x14 start chunk (1-based; 0 or more than the segments → 1, 0x1000ED6A..0x1000ED77), +0x18 loops, +0x1C sync,
    +0x20 fade, +0x24 is3D, +0x28 pitch;
  - +0x2C / +0x30 / +0x34 min / max / scale (10, 100, 1), +0x38..+0x40 position, +0x48 the marker callback.
- Init (0x1000DD50): the channels are malloc(0x288) at 0x1000DDD4; each is free, with .sad volume 127 and its QMixer
  channel enabled in 2D (0x1000DDF5..0x1000DF13); master volume 127 (0x1000DE08), no master channel (0x1000DDFE),
  installed +0x1C and active +0x24 (0x1000DF43 / 0x1000DF46).
- `LHMusicPlay` returns 0 when the music is not installed (0x1000DF6C..0x1000DFC6). It takes the first free channel
  (0x1000DFF1..0x1000E008).
- On a re-trigger, a channel that has not queued its first chunk yet is left alone (0x1000E07E).
- With a marker callback, `LHMusicPlay` makes a new marker list whose clock starts at max(startChunk, 1)
  (0x1000E472..0x1000E4E3).
- `LHMusicStop(info, fade)`: a fade on a channel that is not the master does nothing (0x1000E6C1). `LHMusicStop(1)` sets
  each target to 0 (0x1000E57A) and clears the master (0x1000E5E6..0x1000E5EF).
- Queueing (0x1000EC9E..0x1000F4D5) happens only for a channel in status 1, while the music is active and fewer than 4
  chunks are queued.
- With sync, the start chunk comes from the first other channel in status 1 of the same group (not −1). Without one,
  the start chunk is used if it is 1..n (unsigned compare), else 1 (0x1000ECF3..0x1000ED8B).
- After the last segment: with loops 0 the channel goes to its last chunk (status 4); otherwise the loops drop by one
  and it goes back to chunk 1 (0x1000EDB6..0x1000EE19).
- Each read is malloc(size + 4) (0x1000EE23).
- Before each chunk: a 2D channel is centred with QSWaveMixSetPolarPosition(0, 0, 0) (0x1000F3F0); the frequency is set
  to Hz · pitch / 100 (0x1000F3F6..0x1000F41D); the chunk is queued with QSWaveMixOpenWaveEx 0x1000F2FF +
  QSWaveMixPlayEx(0x400 = queue) 0x1000F4C4, starting at lStart (0x1000F395).
- The channel's WAVEFORMAT is built from the MPEG frame header at 0x1000FA34..0x1000FA9D.
- Channel enable: 2D is flag 0x100 (0x1000E3F7); 3D is 0x110 / 0x111 | 0x100 (0x1000E2D1) with
  QSWaveMixSetDistanceMapping (0x1000E355) and QSWaveMixSetSourcePosition (0x1000E38A).
- Fades run only for channels in status 1 or 4 while the music is active (0x1000F4E8..0x1000F4FF). A step down is
  max(cur − 3, 0): towards 0, not towards the target (0x1000F613..0x1000F620).
- A channel in status 3 is freed and its end callback runs (0x1000EBF3..0x1000EC94). Only the master's markers are
  dispatched (0x1000EBBA..0x1000EBEE).
- Marker clock 0x1000DB90:
  - samples = ftol((GetTickCount − start) · Hz · 0.001f) ([0x10030420]); chunk = samples / 0x5E80 + the base chunk;
    sample = samples % 0x5E80 (0x1000DBD9..0x1000DC1D);
  - the position (chunk << 32) | sample is compared as a 64-bit value (0x1000DC13..0x1000DC52), and a marker fires when
    previous < marker ≤ now (0x1000DC38..0x1000DC63);
  - the start tick is taken when the first chunk is queued (list +0x10, 0x1000F48D); the list is a 0x18-byte header
    (0x1000DB30) at channel +0x68.
- Channel +0x28 holds the status from before `LHMusicPause` (0x1000EA5C). Pause and restart are
  QSWaveMixPauseChannel / QSWaveMixRestartChannel (0x1000EA46 / 0x1000EAB6).
- `LHMusicGetMasterVolume` 0x1000E950 (−1 when not active), `LHMusicIsInstalled` 0x1000EAE0, `LHMusicIsActive`
  0x1000EAF0.
- The volume formula is done in unsigned 32-bit, with the magic 0x2040811 for each /127. The same code is at 0x1000E8F7
  and 0x1000F53D..0x1000F664.

openblack:
- `MusicEngine` (`src/Audio/Engine/MusicEngine.{h,cpp}`, **faithful**) is the DLL's logic over an `IMusicSink` interface (what
  QMixer does). That way it is tested without OpenAL (`test_music_engine`).
- `MusicStream` + `MusicSystem` (`src/Audio/Engine/MusicStream.{h,cpp}`):
  - They use the **audio device** (`audio::device`), without opening another one. They have one source per channel, with its queue of
    buffers and a continuous MPEG decoder per track (`MusicSegmentDecoder`, `audio::codec::MpegFrameDecoder`).
  - The thread does one pass and then waits 120/5000 ms. It pumps every 20 ms.
  - There is a recursive lock, which stands in for the critical section 0x100562B0.
  - Gain = QMixer volume / 32767, times QMixer's 3D law.
  - The MUSIC_TYPE banks are registered by `banks::MusicBankOf` on first use.
- **(approximate)**:
  - A segment that does not decode gives a frame of silence, so that the callback arrives.
  - If the OpenAL queue empties, the source stops and resumes as soon as there is data; QMixer plays the next wave as
    soon as it is there.
  - Before the first frame there is no WAVEFORMAT.
  - Lowering the volume within the pass: the original does not look at the state again inside the loop (MusicEngine.cpp:536).
- **(inferred)**:
  - The unit of `lStart` and of `QSWaveMixGetPlayPosition` is samples.
  - `FlushChannel` reports each flushed chunk before returning: this is deduced from the wait in `LHMusicClose`, 0x1000E854.
  - The marker clock runs in double (0x1001F874).

### MUSIC_TYPE table

**Faithful**. It is table 0x9C9748: 85 entries `{path, "MUSIC_TYPE_…"}` of 8 bytes, up to 0x9C99F0, at GAudio+0x2C + 4·type.
The ctor registers them all: 0x426E82..0x426F1F. If `-NOLOADMUSIC` exists (byte 0xD46AC3), none is registered. If
`GetFileAttributes` fails, it uses the path `"%c:\%s"` on the CD (`g_GameDriveCharacter` 0xC2B9E8).

| types | contents |
|---|---|
| 1..3 | `align/evil|neutral|good.sad` (generic) |
| 4..27 | tribe × alignment. **22..24 (Norse) point to the same strings as 4..6 (Celtic)**: 0x9CAE4C, 0x9CAE0C, 0x9CADCC |
| 28..43 | normal/_vox chants per tribe |
| 44..46 | `citadel.sad`, the same string 0x9CA498 |
| 47..53 | piper, hermit, missionaries. 51..53 are `audio/dialogue/MissionariesVerse1..3.sad`, which are music banks |
| 54 | intro |
| 55..84 | scripts. **56 WELCOME_DANCE points to `FollowUsWelcome.sad`, which is not installed**: the bank stays null and no script uses it. 74 CREATURE_FIGHT, 75 CREATURE_BIG_FIGHT, 77 OUTRO |

`Audio\Music\script\MissionariesSad.sad` is on disk but not in the table. The bw1-decomp values from 84 onwards
do not exist in W120. openblack: `audio::MusicType` and `k_MusicBanks` in `BankTables.h`.

### GameMusic: ProcessMusic and its sources

**Faithful.**

**GAudio music fields**:

| field | contents |
|---|---|
| +0x18 | pos[group] (malloc of `4·TotalGroups`) |
| +0x1C | alignment type that is playing (−1 = none) |
| +0x20 | silence turns |
| +0x24 | last finished type |
| +0x28 | script type |
| +0x2C | banks[85] |
| +0x180 | script music started |
| +0x184 / +0x188 | ThingMusicInfo list |
| +0x18C | current town |
| +0x190 | alignment at the camera |

**`ProcessMusic`** 0x427DF0 is called every turn. It tries the sources in priority order and the first one that takes the music
wins:
1. Video (`g_game+0x250188`) → "nothing" without stopping.
2. No LHMusic, or `LandNumber == 6` (0x427E1D) → return.
3. `ProcessCitadelMusic` 0x427B60.
4. Script music fn_00427CA0 (then +0x1C = −1).
5. Creature fight fn_00427660.
6. Chant `ProcessChantMusic` 0x427790.
7. Creature dance 0x427EC0.
8. Object music fn_00429790.
9. Alignment and tribe 0x4279C0 (the symbol "LoginBox::ControlCallback" is misnamed); then +0x180 = 0.
10. If there is none: `SavePositions` and `LHMusicStop(1)`, fade to silence.

"Nothing" (0x427E95..0x427EB3) sets "Music Playing=NONE", +0x180 = 0 and +0x1C = −1, without stopping anything.
`SavePositions` (fn_004281C0) goes before each `LHMusicPlay`: it saves pos[group−1] = audible chunk + 2 of each channel in
state 1 with group > 0.

The sources:
- **Script** fn_00427CA0. It uses vol 127, start 1, no sync or fade, 2D, and the volume and loops from the .sad.
  - End callback 0x426B80: if GAudio+0x28 == data, GAudio+0x28 = 0.
  - Marker callback 0x426BA0 (table 0x426C10): `L<n>` → GScript+0x98 = n, +0x9C = 1; `P`/`W` → +0x9C++.
  - `StartScriptMusic` 0x428230 does +0x28 = type; with a type, +0x180 = 0, and it is relaunched even if it is the same one.
  - With +0x28 == 0 and +0x180 active: `LHMusicStop(1)`.
  - `CitadelHeart::Built` 0x4650D1 launches EPIC_01 (61) if `LandNumber != 1`.
- **Alignment and tribe** 0x4279C0. It requires:
  - that there is a camera;
  - that the script widescreen is not on (HelpSystem +0x45E8 && +0x45EC) and the bars are not moving
    (fn_005C6C50);
  - GScript+0x94 ≠ 0;
  - turn > 20.

  The type comes from fn_00427460:
  - a = fn_00426C80(`Discrete`(GAudio+0x190)). `Discrete` is 0x414730: `ftol(min((a+1)/2·7, 6))`. Table 0x9C99F0
    `{0,0,1,1,1,2,2}` gives 0 evil, 1 neutral, 2 good, with thresholds ±3/7.
  - The nearest town within 400 (fn_00602160), with the camera < 400 high above the ground:
    - a ≤ 300 → the town's tribe;
    - with hysteresis, the previous town while it is < 400 away.
  - If there is no town, generic a + 1.
  - Tribe(a, t) = t ≥ 9 ? 5 : {4,4,7,10,13,16,19,22,25}[t] + a (0x427410, table 0x9C9A0C).
  - 300 and 400 are `townTriggerDistance` / `townTriggerOffDistance` from info.dat (0xD9A934 / 0xD9A938).

  It is played with vol **80**, start pos[g−1], sync 1, fade 1, 2D, loops = (start ≥ n/2) (0x427AF9..0x427B01). The
  end callback is 0x426B40 → 0x4279A0. When a track ends, **3500 turns of silence** as long as the type does not change.
- **Objects** (ATTACH_MUSIC) fn_00429790 / fn_00429500.
  - Each `ThingMusicInfo` takes 0x38 B: +0x14 type, +0x18 object, +0x1C active, +0x20 finished, +0x24 started,
    +0x28/+0x2C own position.
  - It plays in 3D if the camera is closer than `GetPlayDistance` (0x4293E0: the bank's maximum, or 100), with sync and
    fade = (group > 0).
  - Each turn it does a re-trigger of `LHMusicPlay` + `Set3DPosition`.
  - An inactive object in range still blocks the alignment music.
  - Nobody sets +0x20 = 1 in 0x429180..0x429950 (open question).
- **Citadel** 0x427B60 (44 + discrete alignment of the local player; `LHSampleStopAll` once, flag 0xC56164;
  vol 127, sync 1, fade 1).
- **Fight** 0x427660 / 0x427590 (interface state 0x10 or a `GArena` < 100 away; 75 BIG_FIGHT if `g_game+0x205A0C`, if
  it is multiplayer or if `LandNumber ≥ 4`; otherwise, 74).
- **Chant** 0x427790 (citadel < 150 from the camera, `GetNearestCitadel` 0x602200; of its six sites, the one that has
  dancers with the dance centre nearest, < 100 away, fn_004639A0; |camera height − ground at the centre| < 100;
  table 0x9C9A30 `{28,28,30,…,42}` + (dancers > 8) = the _vox version, fn_00427430; vol 127, sync 1, no fade,
  3D at `GetSpecialPos(8)`). Details in [Chants and heartbeat](#chants-and-heartbeat-in-openblack).
- **Creature dance** 0x427EC0 (action 0x17 < 75 away, Δheight < 60; action 0x17 is **(inferred)**).

More of GameMusic:
- Debug texts: "Music Playing=%s" (0x9CAFA0) with the MUSIC_TYPE name table 0x9C974C, and "Music Playing=NONE"
  (0x9CAFB4).
- The ctor registers the 85 banks at 0x426E8E..0x426F1F, then sets GAudio+0x18 = malloc(LHMusicGetTotalGroups() · 4)
  (0x426F2B..0x426F40). The game's banks give 13 groups, so GAudio+0x18 holds 13 positions.
- `SavePositions` runs only with LHMusicIsActive (0x4281C6). It saves a channel in status 1 with group > 0 (signed) and
  group − 1 < TotalGroups (unsigned) (0x4281E4..0x4281FD).
- ProcessMusic call sites: LHMusicIsInstalled 0x427E09, script 0x427E37, fight 0x427E42, creature dance 0x427E58,
  objects 0x427E63, alignment 0x427E6E (then +0x180 = 0 at 0x427E77), SavePositions 0x427E85, LHMusicStop(1) 0x427E8F.
- `AlignmentIndex` fn_00426C80: an index of 7 or more (signed compare, 0x426C87) gives 1; only then is the table
  0x9C99F0 read.
- `Discrete` 0x414730 constants: −1.0 (0x8AB640), 7.0 (0x8AB688), 6.0 (0x8AB35C); the result is truncated.
- Script music picks its bank with fn_00427C90(fn_00426C80(Discrete(+0x190))), which ignores the alignment and returns
  +0x28 (0x427CED..0x427D0D). The end callback 0x426B80 is set at 0x427D51, the marker callback 0x426BA0 at 0x427D59,
  and the user data is the type (0x427D2F).
- The marker callback switches on label[0] − 'L' over 0..0x2B: byte table 0x426C10 → jump table 0x426C04
  (+0x98 = atoi(label + 1), 0x426BBE..0x426BE5).
- The alignment music's end: pos[group − 1] = 1 (0x426B61), then +0x24 = the type (0x4279A3), +0x20 = 0 (0x4279A6) and
  +0x1C = −1 (0x4279AD). ProcessAlignmentMusic clears +0x24 at 0x427A6E.
- `AddThingMusic` 0x429230 (ATTACH_MUSIC): it accepts only an available thing (0x42923E); a thing that already has music
  only changes its type (0x42924C..0x42925E); a new info cuts its bank if that bank is already playing
  (fn_004296C0(info, 0)) and goes at the head of the list (0x429265..0x4292E8); a new info starts enabled, with
  everything else 0.
- `ThingMusicInfo` has vtable 0x8C4A14 (AudioMusicThing.cpp). `RemoveThingMusic` 0x429340 stops nothing.
- ENABLE_DISABLE_MUSIC (fn_004298A0) sets enabled = on, finished = started = 0 (0x4298B4..0x4298BA).
- `RestartMusicThing` 0x429910 sets finished = started = 0 and cuts the music with fade 0 (0x429943).
- `IsMusicThingFinished` of a thing with no info returns 1 (0x429904).
- GET_MUSIC_OBJ_DISTANCE (fn_004293B0) gives GetPlayDistance of the thing's type, or 0 when it has no info.
- `GetPlayDistance` 0x4293E0: 100 (0x8AB41C) without a bank. Otherwise `LHBankGetMusicMaxDistance` (+0x26C of segment
  0), with a negative value giving 100 and 0 staying 0 (0x4293FA..0x429409).
- A thing is in range when GetPlayDistance > its distance (strict, 0x4294CD..0x4294DC).
- fn_00429500 measures the range at the thing even when a play position is set (0x42953F); it plays at
  SET_MUSIC_PLAY_POSITION's point if there is one, otherwise at the thing.
- Then: with no channel holding the bank, and both started and finished set, it returns 0. With finished set, it
  returns 1 without playing. Otherwise it calls LHMusicPlay and Set3DPosition (0x429656) and sets started = 1
  (0x42964F).
- fn_00429790 deletes the info of a thing that is no longer available (0x4297FE..0x42984A). The first info that plays
  takes the music (0x4297D4..0x4297DF). A thing whose music is disabled (ENABLE_DISABLE_MUSIC off) but in range plays
  nothing, and the alignment music does not take over either (0x4297F7).
- Chants: with no bank for the tribe's chant, nothing plays (0x427882).

openblack:
- `GameMusic` (`src/Audio/Services/GameMusic.{h,cpp}`, **faithful**): ctor fn_00426D40 (music part), `Reset` 0x426CA0,
  `UpdateTurn` (was `ProcessAudioGameTurn`) 0x427080, `ProcessMusic` 0x427DF0, `StartScriptMusic` 0x428230, `ProcessScriptMusic`
  0x427CA0, the callbacks 0x426B40/0x426B80/0x426BA0, `ProcessAlignmentMusic` 0x4279C0 and fn_00427460, `DiscreteAlignment`
  0x414730, `AlignmentIndex` fn_00426C80, `TribeMusicType` fn_00427410, `SavePositions` fn_004281C0, `ResetPositions`
  fn_00428190, the object music fn_00429790/fn_00429500/fn_00429420/fn_00429680/fn_004296C0, `PurgeThingMusic`
  fn_00429700 and the CHL functions.
- `ThingMusicList` (`src/Audio/Services/ThingMusic.{h,cpp}`): 0x429180, 0x429230, 0x429340, 0x4291B0, fn_00429880,
  fn_004298A0, 0x4298C0, 0x4298F0, and the `MapCoords` round trip (0x603340, 6553.6 at 0x8AC400, 10/65536 at 0x8AA3A4).
- `Game.cpp` hooks: `GAudio::ProcessAudioGameTurn` after turn 5 (0x54E997) and `Reset` in `LoadMap`.
- The fight and dance branches are ownerless queries (false) until there is a creature; the chant one is
  `GameMusic::ProcessChantMusic` and the citadel one `GameMusic::ProcessCitadelMusic`. The camera's
  alignment (GAudio+0x190) and the town with its tribe come from `ecs::map_cells` (see [Weather and alignment](#weather-and-alignment-in-openblack)):
  with the camera over a town with a centre, its tribe's music plays, otherwise the generic alignment one, at volume 80,
  from turn 20 on lands ≠ 6.
- **(approximate)**:
  - A type outside 0..84 has no bank: the original reads the following fields.
  - Group 0 reads `pos[-1]`, outside the array: here it gives 0, and the DLL starts at chunk 1.
  - Outside −1..1 the tribe reads outside its table: the same as from 9 onwards is taken. For the alignment the
    original itself gives 1 for an index of 7 or more (0x426C87); only a negative index reads outside the table
    0x9C99F0, and neutral is taken.
  - Without a camera, an object is out of range.
  - `LHWaveIsActive` = the music engine exists and is active.
  - `GameThing::IsAvailable` = a valid entity with Transform. Its float position replaces the MapCoords, without the
    16.16 rounding.
- **(inferred)**: openblack's turn (which goes back to 0 on each `LoadMap`) stands in for `g_game+0x205A40`, and the render
  camera stands in for GGame's.

### Intro, trailer, outro, videos and menu

**Faithful except where marked.**
- **Trailer**: `PlayPreIntroVideo` 0x6426F0 plays `pre_intro.bik` without Bink sound and plays `audio\music\intro\trailer.sad`
  (0xBFEB44) with `LHMusicPlay` (vol 127, start 1, no fade, 2D). When it ends, `LHMusicStop(0)` (0x642907).
- **Intro**: START_MUSIC 54 in the script `followus.txt::FollowUs`.
- **Outro**: START_MUSIC 77 in `landcontrol5.txt::VillageWavingSequence` (`Outro.sad`, 160 kbps, loop −1).
- **Videos**: `PlayFullScreenMovie(path, sad)` 0x54D920 registers an optional bank and fn_0054A9B0 starts it at
  frame 3 (vol 127, start 1, fade 1). It also sets GAudio+0x1C = −1.
- **Main menu**: none of the sites that call `LHMusicPlay` belongs to the menu, so the menu has no music
  of its own: the menu is boxes on top of the loaded world and its normal music plays (confirmed by the user).
- `Data\intro.wav` does not appear in the exe.
- `MusicMoodController` 0x633EF0 is not audio: they are network packets.
- openblack: the videos play (`videoPlaying`: the ambience is silenced and the music continues underneath; the 11
  sounds of fall.bik and `MusicStop(1)` at 43.9 s, checked, see [video.md](video.md)); the trailer and the menu are
  [pending](#pending).

### Music in openblack

The chain is `Game::Initialize` → `audio::music::Start()` (`LH_AudioSystem` init 0x1000DD50,
on the `audio::device` device) → `audio::game_music::Start(GameQueries, townTrigger)`. Per frame,
`audio::music::Update()` applies the master from the configuration and the test hook. On the turn
`game_music::ProcessTurn` runs. On shutdown, `game_music::Shutdown` and then `music::Shutdown` (`LHMusicClose` 0x1000E7A0),
before releasing the context. Debug panel: the "Music" window (`src/Debug/Music.{h,cpp}`), with the master, the 6
channels, a player for any MUSIC_TYPE (with or without sync and fade; stop with fade or abruptly), the state of
GameMusic (+0x28, +0x180, +0x1C, +0x20, +0x24, +0x18C, pos[group]), GScript +0x84..+0x9C and the object list.

## Voices and texts

### Text voice table

**Faithful.**
- The table is at 0x915D40, with identical copies at 0x942B38 (SAY), 0x957310 (GConfirmation) and 0x96BA30 (GGuidance).
  They are `{u32 id, u32 bank, u32 sample}` entries, 0x1B3E = **6974** texts (`HELP_TEXT_LAST`).
- The index is the position of the `ADD_TEXT` in `Scripts\InfoScript2.txt`. bw1-decomp's `HelpTextEnums.h` is from another
  version and drifts from 1009 onwards.
- **3477 texts have a voice**: 1922 in HelpSprites (6), 1328 in villagers (7) and 227 in Guidance (10). None in
  VillagersBanter or in SpellDialogue.
- **It is rebuilt from the data**: the name of the text is the name of its sample's .wav. It is looked up first in
  villagers, then in HelpSprites and then in Guidance. There is a single duplicate: `HELP_TEXT_LAND_2_WORKSHOP_10` is in
  HelpSprites 801 and in villagers 399, and the exe gives villagers 399. That villagers goes first is deduced from that single case
  **(inferred)**.
- **Narrators** (InfoScript2 W120): 0 NONE, 1 DEFAULT, 2 GOOD_SPIRIT, 3 EVIL_SPIRIT, 4 WOMAN, 5 MAN, 6 OGRE, 7 KHAZAR,
  8 LETHYS, 9 NEMESIS, 10 BOY, 11 BIG_VOICE, 12 TRAINER. `ADD_TEXT` also uses GUIDE and MONK without declaring them; their value is
  **(inferred)**, and openblack gives them −1.
- `HelpTextDatabase` 0xD17CA8 stores {+0 narrator, +4 arg0, +8 text} (fn_005CAD00). The conversion fn_007191F0 is applied
  to the text: `~` → 0xF8FE and `\n` → line break.
- **Fixed channel owners**:

  | owner | who |
  |---|---|
  | 0x270C | advisors (HelpDude) |
  | 0x270D | SAY with alt |
  | 0x270E | only stopped by STOP_SOUND_EFFECT(isSay); nobody plays it |
  | 0x270F | narration and SAY without alt |
  | the sample number | script PLAY_SOUND_EFFECT |

- In W120's say table every entry has id == index, so the id test of SaySoundEffect (0x70F910) never zeroes the bank.
- STOP_SOUND_EFFECT(isSay) takes the narrator from HelpTextDatabase[0 < id < count ? id : 0] (0x70FA8D..0x70FAAD), and
  reads the say table 0x942B38 without checking the id's bounds (0x70FAB3 / 0x70FAB9).

### Narration and reading time

**Faithful except where marked.**
- **`RUN_TEXT`** (`GScript::RunText` 0x6F7D60): an id ≥ 6974 gives "Invalid text" and becomes 0. With `singleLine` or with the
  flag +0xB0, `ClearAllText` 0x5C5550. Then it goes to fn_005C5F90:
  - it shows the text and starts the reading time (fn_005C6100 → fn_005C61B0);
  - it stores it in the queue of 6 (+0x584..+0x598) and in the history of 1024 (fn_005C5EE0);
  - and it says the voice:
    - bank 6 with narrator 2 or 3 → the good or evil **advisor** (`HelpDudeControl::Say` 0x5C36D0, with a delay of
      0..500 ms and owner 0x270C);
    - another bank with a sample → `GAudio::PlaySoundEffect` 2D, owner 0x270F, +0x164 = 1.
- **`IsTextRead`** 0x5C64E0 → 0x5C6340:
  - With `withInteraction` it waits for the click.
  - If the text has a voice (fn_005C62F0: entry, bank, sample and the bank registered):
    - advisor: the text is read when none is speaking or went quiet less than 200 ms ago (fn_005BB730);
    - narration: while 0x270F is playing, end = now + 450 ms.
  - If it has no voice: inside the citadel, by ms; otherwise, by turns.
- **Reading time without voice** (fn_005C61B0): `s = (readDefaultWordGTTime·n + readDefaultAdjustGTTime) · msPerTurn ·
  0.001 · f(READ_SPEED)`.
  - The two constants are 5 and 8 in info.dat 0x4AEC4, in `HelpSystemInfo` 0xD1617C/0xD16178. The values come from
    pattern matching **(inferred)**.
  - `msPerTurn` = [0xD01A38] = 100 (0x54F4A5).
  - f(r) = r ≤ 0.5 ? 3 − 4r : (1 − 2(r − 0.5))·0.8 + 0.2 (fn_005C6CB0), with READ_SPEED = 0.5 by default.
  - n = words (fn_005CBEC0 over the splitter fn_005CB590).
- **Click** (`HelpSystem::ProcessInterface` 0x5C69B0, from `GInterface` 0x5D11C0): with unread text and (widescreen
  or the key [0xE85410]):
  - it stops the spirits (fn_005C6720, unread);
  - it does `StopPlayingSoundEffect(0, 0x270F, 7)`: **only villagers**, with a 20 ms ramp (DLL 0x10012C50).
- `GAME_CLEAR_DIALOGUE` 0x6FF6F0 and `GAME_CLOSE_DIALOGUE` 0x6FF700 clear the text, but **do not stop the voice**.
- **There is no ducking** of the music by the voices: the music only changes with the options. This is faithful in the exe; what
  happens inside the DLL's thread is **(inferred)**.

### Advisors, Guidance, confirmations and night voices

**Faithful in the original.** openblack: [Voices on channels](#voices-on-channels-in-openblack) and
[Guidance and night voices](#guidance-and-night-voices-in-openblack).
- **Advisors**:
  - `HelpDudeControl::Say` 0x5C36D0: v = |+0x3514| − 0.95 (0.95 at 0x915438); delay = v < 0 ? 0 : min((v + 1)·250,
    500) ms (+0x3514 = the advisor's floating position, **(inferred)**).
  - `HelpDude::SaySentence` 0x5BB340 → `UpdateSaySentence` 0x5BB610 (per frame) → `PlaySample` 0x5BB530
    (`LHSamplePlay` directly, without filters, with the PCM for the mouth).
  - Lip-sync: `ApplyLipSync` 0x5BCD00 and `AutoVoiceParams::CalcKey` 0x428850. CalcKey uses the FFT `AudioAnalyse::Analyse`
    0x428C60 + `four1` 0x428D50, which have not been dumped.
  - `IsTalking` 0x5BB760 and `StopSentence` 0x5BB840.
  - Spirit effects: `HelpDude::PlaySoundFX` 0x5C2800 (bank `GetSoundFXBank` 0x5BC7C0).
- **GGuidance** (`Guidance.sad`, 0x71AB10..0x71D490): villager reactions and advisor comments.
  All read; the details are in
  [Guidance and night voices in openblack](#guidance-and-night-voices-in-openblack).
  - 33 types with an interval table 0x980190 {base, help level, always}.
  - `PlayNow` 0x71AF50: a non-"always" type stays silent on **Land 1 of a single-player game that is not the
    playground** (g_game+0x205A08 == 1, `IsMultiplayerGame` 0x552F80, +0x205A0C); then the help level
    (HelpSystem+0x45F8 ? +0x45F4 : 0) and turns since the last time > interval = base + rand(5·base·(1 − r³))
    (0x71AEE0, drawn on every call). `PlaySample` 0x71C6F0.
  - Types 9..30 (and 31, 32) make an advisor speak: `HelpSpiritSay` 0x71D270 starts the help script
    `MultiHelpJustTalkWithText(text, text)` (HelpSystem.txt), which waits for the dialogue, does RUN_TEXT and waits for it to be read.
  - `BeliefSFX` 0x437F40 → 0x71BF70 (thresholds 0.05/0.4/0.7, max 200).
  - Heartbeat: InGame 45 looping, 3D at the **citadel** if its heart (+0x30) is alive, max 500, pitch = 30 + 70·v
    smoothed 0.1 (0x71C460, already **faithful**: 0x8BF51C, 0x92B2C8, 0x8AB22C).
- **GConfirmation** (START_ANGLE_SOUND 285 / "START_ANGLE_SOUND" 348 = `StartPitchSound`):
  - Init 0x71A560, Start 0x71A610, Process 0x71A650.
  - HelpSprites 1686 (BETTER_14), 1673..1685 and 1704..1717.
  - It never says "no": it is a bug in the original and it is copied.
- **GSpookyVoices** 0x72E2A0..0x72E870: at night (real clock 20:45-20:59 or 23:00-05:59) it whispers the profile's name
  chosen by Soundex among the 100 `HELP_TEXT_SPOOKY_NAMES_*` (details in
  [Guidance and night voices in openblack](#guidance-and-night-voices-in-openblack)).
- The **creature does not speak**: `creature.sad` and the per-species banks are effects **(inferred)**.
- **Advisors, in detail.**
  - `HelpDudeControl::Init` comes from `HelpSystem::CallVirtualFunctionsForCreation` 0x5C5860 → fn_005C3660 →
    fn_005BB060. It sets the bank (+0x2EFC) and the sample count (+0x2F00, 0x5BB0C1).
  - HelpDudeControl lives as long as HelpSystem: `HelpSystem::Reset` does not touch it, and `Uninit` 0x5C5680 deletes it.
  - UpdateSaySentence is called from `HelpDude::Update1` 0x5BDE41 and from the control's loop 0x5C3CB3
    (0x5C3B05..0x5C3CBC). A sentence starts once GetTickCount ≥ its start tick (signed compare, 0x5BB62B).
  - Sentence globals: start tick 0xD15A98 (GetTickCount at PlaySample, 0x5BB55A); PCM 0xD15A94; frames 0xD15A8C =
    bytes / (2 · nChannels); rate 0xD15A88; duration 0xD15A90 = frames / rate (0x5BB5CC..0x5BB5FE). The previous PCM
    copy is freed first (0x5BB568); the spectrum buffer is 0xD14084 (2 × 0x200 floats).
  - A dude's play options hold the bank at +0x3680, the sample at +0x36A0 and keep-PCM at +0x37E0 (0x5BB39C..0x5BB3C0,
    0x5BB53A).
  - The last-talk tick is +0x37EC. TalkingOrJustStopped tests it with an unsigned compare < 200 ms
    (0x5BB749..0x5BB756). IsTalking refreshes it (0x5BB7A7) and calls StopSentence once the sentence is over (0x5BB7B6).
  - +0x2F0C holds the sample and its AudioTags (`BuildAudioTags` 0x42AE70 over the wave's tags, info+0x78 / +0x7C;
    0x5BB669..0x5BB71F).
  - StopSentence also calls fn_005BCBC0(duration + 100, 0) (0x5BB87B..0x5BB88E).
  - VoiceKey at HelpDude+0x2F60 (ctor fn_005C2D50): +0x00 offset in ms (0, 0x5C1EC8), +0x04 threshold 0.05, +0x08 level
    2.5, +0x0C rate 40, +0x10 the three bands; the band total is kept at +0x4C (0x42891E).
  - The key time +0x2F70 is the play position · 0.001 when that position is ≥ 0, and then the vowels fn_005BF810 run on
    +0x2F60 (0x5BCD6C..0x5BCDDB). Otherwise it is the tick time.
  - CalcKey's window: centre sample = (offset · 0.001 + dt + t) · rate (0x428860..0x428874); start = centre − window /
    2, clamped to 0..frames − window (0x428886..0x428892).
  - `BandLevel` fn_00428A80: from = ftol(low / rate · n), to = ftol(high / rate · n), each capped at n
    (0x428AAB..0x428AB5). The level is the sum of spectrum[from..to) divided by (to − from) (fidiv 0x428ADC).
  - `Amplitude` fn_005BB420(sample, window) = √Σ(s / 32768)² (3.0517578e-5, 0x8C5848) over `window` PCM samples,
    starting at ftol(t / duration · frames) with t = (GetTickCount − start) · 0.001. It gives 0 for sample 0, for t < 0
    or t ≥ the duration, without PCM, or when the sample is not playing on HelpSprites with owner 0x270C (0x5BB497).
  - Say's constants: 250 (0x8C7B2C) and 500 (0x8C78EC). It sets +0x74 + 4 · dude = 1 at 0x5C373B.
- **GConfirmation, in detail.**
  - A static at 0xD99BA0. Init 0x71A560 (from InitOneTimeOnly 0x54F03D) zeroes it, and `GGame::ProcessTurn` calls
    Process at 0x54E731 once per turn.
  - Fields: +0x08 the watched value (g_angle 0xC5E158 or g_pitch 0xC5E15C), +0x0C the range, +0x14 the turn of the last
    "better", +0x18 active, +0x1C the turn of the last sample.
  - START_ANGLE_SOUND (285, 0x70FFA0) watches the turn with range 7 (0x70FFB7) and clears g_angle (0x70FFC1). CHL 348
    (StartPitchSound 0x70FFE0) watches the tilt with range 5 (0x70FFF7) and clears g_pitch (0x710001).
  - Start 0x71A610 sets the value, the range and active, with +0x1C = +0x14 = 0. Stop 0x71A640 clears active.
  - The camera feeds both values with fn_00454900 / fn_00454930: g = (a / b − g) · 0.4 + g. Here a is the frame's turn
    (or tilt) and b the camera's seconds for the frame (GetCameraTimeInc · 0.001); a = 0 on frames with no turn
    (0x46053F / 0x46055C).
  - Process 0x71A650: v = clamp(value / range, −1, 1), with a NaN giving −1; a = |v|. a == 1 and more than 100 turns
    since the last "better" (0x71A6BC) → BETTER_14 1686 (fn_0071A850; both turn stamps are set). a > 0, more than 20
    turns since the last sample (0x71A901) and a > LocalFloatRand(1) (drawn only at that point) → 1704 + LocalRand(14)
    (0x71A741..0x71A752).
  - It plays with Init's options: HelpSprites, volume 100 (+0x28), +0x2C 90, owner 0, 2D, no track; through 0x429E30
    (0x71A76B / 0x71A869).
- **GGuidance, in detail.**
  - Constants: 0.3 desire threshold 0x98012C, 200 desire distance 0x980130, 300 thing turns 0x980134, 600 forget turns
    0x980138; 0.55 / −0.55 alignment class 0x98014C / 0x980150, 200 attack 0x980158, aggression 1 0x98015C, 200 belief
    0x980164, 0.3 visibility 0x980168; 100 / 400 creature 0x98016C / 0x980170, 300 hand range 0x980174, 0.95 / 2 /
    0.75 / 0.4 alignment 0x980178..0x980184; heartbeat max distance 500 (`push 0x43FA0000`, 0x71C5D7), G_HeartBeat 45
    (0x71C600 / 0x71C63B); the strings "MultiHelpJustTalkWithText" 0xBF1988 and "…WithNoText" 0xC22254.
  - `Close` 0x71ABF0 is called from GInterfaceStatus::UnInit 0x5DD226. Init is also called from ResolveLoad 0x5DD194.
  - LastThings (+0x90 / +0x94, {thing, turn}, newest first): TimeSinceThingSeen fn_0071AE10 gives the turns since the
    thing was first seen. Past 600 it starts again (0x71AE65), and a new thing is added and gives 0
    (0x71AE99..0x71AEAA).
  - `DesireSample` fn_0071AA90: x = v − LocalFloatRand(v / 3). x ≥ 0.85 / 0.65 / 0.45 (0x980324 / 0x980320 / 0x98031C)
    picks the desire's text 0 / 1 / 2; otherwise 0.
  - `DesireScore` fn_0071B410 = 2 · t0 · s · v³ · (1 − d²) · (1 − t1³), with t0 = min(TimeSince(TownDesire) / 50, 1)
    and t1 = min(TimeSinceThingSeen / 300, 1) (fn_0071AE10 is called a second time for the value kept); d =
    min(distance / 200, 1) and v = min(value, 1); s = t0³ when the sample is the last desire's (+0x98), otherwise 1.
  - CheckWorshipSiteDesiresSFX draws both texts with the food value (`push ebp` twice, 0x71B33E..0x71B34F).
  - The town desire plays at volume 127, pitch 100, +0x2C 90.
  - `GetRandomSampleBasedOnValue` clamps the value to 0..1 first (0x71D320..0x71D358).
  - PlaySample's point is (x / 6553.6, GetAltitude + y, z / 6553.6) (0x71C75D..0x71C7C1). lastPlayed is written only
    for a type < 33. In 3D without a point nothing plays and lastPlayed is not updated (0x71C757).
  - The 22 HELP_SPRITES_GUIDANCE lists are objects at 0xD99BD8 + 0x98 · k.
  - GetGuidanceResourceType of each class: food (1) for Pot food 0x71BDEF and Animal 0x71BE10; wood (2) for Pot wood,
    Tree 0x71BE20 and DeadTree 0x71BE30.
  - `StrongestEffect` fn_0071BE40: the strongest of effects 0, 1, 2 and 4 that is above 0 (effect 0 scaled by 0.001
    (0x8AC418) and capped at 1); 7 when there is none.
  - Attacker samples (GetSampleForAttack vt +0xDC, 0x71BC20..0x71BD50): a plain thing gives 0; a spell of type 6
    (fn_0072B200) gives LIGHTNING_01..10 = 0x132E + LocalRand(10), other spells 0; a rock gives ROCKS_01..10 = 0x1342 +
    LocalRand(10); a creature gives a MONSTER text. The attacker is EffectValues +0x28; the caused player is
    `EffectValues::GetCausedPlayer` 0x525910.
  - HelpSpritesTownBeingAttacked reads that player's aggression at +0x9F8 + 0x80 · n.
  - `BeliefSFX` starts the strongest of the 8 beliefs (GBelief +0x8) at 0 (0x437F41..0x437F67). A believed-in player
    that is not below the strongest says nothing (0x437F84).
  - `BeliefSample` fn_0071BF70: good: ≥ 0.7 GOOD_AWE_01, ≥ 0.4 _02, > 0.05 _03; evil: the EVIL_AWE texts the same way;
    an alignment other than 1 / 2 is a coin toss, LocalRand(2) ? 2 : 1 (0x71BFB5); the last belief sample is +0x9C
    (0x71C154).
  - Heartbeat pulse: fn_0071C450 = +0xAC; fn_0071C430 = +0xB0 + (+0xAC − +0xB0) · g_game+0x205D64 (the turn's
    fraction).
  - fn_0071C3F0(v) sets the heartbeat override +0xB8 (0 = follow +0xA4). Callers: CitadelHeart fn_00465C70 (0x465DA3,
    0x466489).
  - HelpSpritesCheckMoonPhase constants: IsVisualNight 0x5575E0, π 0x8C36A0, 12000 0x9804C8, 600000 turns = 0x927C0.
  - DeathInVillageSFX is called from `Villager::VillagerDead` 0x7507B8 for a villager of the local player killed by
    another player (the death table's +0xC, 0x99A370). It plays 0x1658 (DEATH_IN_VILLAGE_06) + LocalRand(5) at
    volume 127.
  - Type 31 "MultiHelpJustTalkWithNoText" has no caller.
  - `HelpSystem::RunMessage` with first > last does nothing (0x5C8CEE).
  - `GRand::LocalFloatRand` returns a float (0x6DE5E3: the caller stores it).
  - The HelpSprites functions, with caller and list: TownBeingAttacked 0x71C870 → list 0; CreatureAttackingThem
    fn_0071C960 (Town::UpdateAggressor 0x73CAFA; needs [0xC221CC]) → list 2; LosingVillagers 0x71C990 (VillagerDead
    0x7508C5; the villager within 300 of the hand) → list 3; AttackingTown fn_0071C9F0 (0x73CB24; a town not of the
    local player, with people) → list 4; LowOnFood 0x71CA60 (Town::CalculateDesireForFood 0x747FA0; by value) →
    list 5; LowOnWood fn_0071CAF0 (0x7481BC) → list 6; InjuredPeople fn_0071CB80 (no caller) → list 7; LowOnPeople
    0x71CBE0 (VillagerDead 0x7508EF) → list 8; VillagerUnhappy 0x71CC40 (TownDesire::Process 0x745C8A; the value is not
    used) → list 9; LosingBelief fn_0071CCC0 (fn_00438340 0x4383C4) → list 10; OtherVillages fn_0071CD00 (no caller) →
    list 11; CreatureFight fn_0071CD40 (CameraModeNew3::StartFight 0x45A772 / 0x45A7B2) → list 12; GeneralBad
    fn_0071CD70 (fn_004383D0 0x43872B, creature dance 0x5039E7) and fn_0071CDF0 (0x438755, 0x50394F), which need the
    point on screen (fn_0081F1D0) → lists 13 / 14; KillingPeople fn_0071CE70 (VillagerDead 0x75078B; the villager's
    bounding box on screen, fn_0081F1A0) → list 15; WorshippersDying fn_0071CFE0 (VillagerDead 0x75087C, death
    reason 4) → list 18.
- **Night voices, in detail.**
  - `GSpookyVoices::Init` is called from `GGame::InitOneTimeOnly` 0x54F024. GetPlayerName 0x72E870 is called from
    `GGame::Init` 0x54FD10. fn_0072E280 (called from fn_0054EB40 0x54EC24) frees the options.
  - CRT calls: time() 0x7C79FD, localtime 0x7C789D, _isalpha 0x7C686F. SoundExCode indexes its jump table with
    (c | 0x60) − 0x61.
  - The countdown is reset to 100 and then decremented, so it stores 99 (0x72E3A2..0x72E3A8). The night test
    fn_0072E3B0 is at 0x72E3C5..0x72E3E9.
  - `Process` does nothing on lands 1 and 2 (0x72E31E / 0x72E327). `SoundexOverlap` tries the name's words in turn
    (0x72E6F3..0x72E727).

### Voices and texts in openblack

- **`helptext`** (`src/Common/HelpText.{h,cpp}`):
  - `Entry {arg0, narrator, name, text}`: the name is needed by the voice table; the original does not store it.
  - `Parse` (UTF-16 with or without BOM, narrator names resolved with the file itself), `ConvertScriptText`
    fn_007191F0, `GetEntry`, `Count`.
  - `k_TextCount = 0x1B3E` and `k_NarratorGoodSpirit/EvilSpirit = 2/3`.
  - A narrator number is read like `_wtoi` **(inferred)**: the reader `LHScriptX` 0x7E7960 has not been read.
- **`audio::VoiceTable`** (`src/Audio/Services/Voices.{h,cpp}`): `Build` with the rule above and `Get`. `voices::BuildTable`
  is called in `Game::Initialize` with the wave names that `Game.cpp` reads from banks 7, 6 and 10. There are also
  `VoiceOwner` (0x270C..0x270F) and `TextVoice::HasVoice` (fn_005C62F0).
- **`help::HelpSystem`** (`src/Help/HelpSystem.{h,cpp}`, **the text part**):
  - `RunText` 0x6F7D60, `RunTextWithNumber` 0x6F7C70, `TempText` 0x6F7E40, `TempTextWithNumber` 0x6F7F50,
    `ClearDialogue` 0x6FF6F0, `CloseDialogue` 0x6FF700.
  - `SayText` fn_005C5F90, `AddText` fn_005C6100, `ClearAllText` 0x5C5550, `ClearTextDisplayed` 0x5C54E0, `Reset`
    0x5C5580.
  - `IsTextRead` 0x5C64E0, `ProcessInterface` 0x5C69B0.
  - `CountWords` fn_005CBEC0, `ReadSpeedFactor` fn_005C6CB0, `RouteOf` (the branches of 0x5C6025..0x5C60DB) and the history
    fn_005C5EE0/fn_005C5F50.
  - **Nothing is drawn**: `OPENBLACK_TEXT_TRACE` writes each text to the log.
  - The voices go through the hooks `sayVoice`, `stopVoicesOnClick` and `spiritStop` and the queries `voiceBankLoaded`,
    `advisorsTalking` and `isPlaying`, which `Game.cpp` connects to `audio::voices` and `audio::advisor`
    ([below](#voices-on-channels-in-openblack)).
  - The click is the left button on press **(inferred)**.
  - The scaled clock 0xEA1C78..0xEA1C80 is real ms since startup **(approximate)**.
  - `+0x460C` (`GInterface::SetActive`) is not ported.

## Script: audio CHL

**Faithful.**
- The CHL table `g_scriptFunctionTable` 0xC0DB98 has 464 entries of 0x90 B. The **37 audio opcodes** match
  openblack's bindings.
- In W120 there is no ENABLE_BANTER, IS_PLAYING_SOUND, music fades or audio STOP_ALL: they must not be
  invented.
- `ScriptErrorMessage` only warns: the function **continues** with the bad value.
- State in openblack: **done** = implemented; **stub** = `NotImplemented`.

| op | name | original | openblack |
|---|---|---|---|
| 13 | RUN_TEXT | 0x6F7D60 | done (text and voice) |
| 14 | TEMP_TEXT | 0x6F7E40 | done (no voice, as in the original) |
| 15 | TEXT_READ | 0x6F8260 → IsTextRead | done (with the voice branch) |
| 43 | PLAY_SOUND_EFFECT | 0x70F7F0 (owner = sample no.) | done |
| 44 | START_MUSIC | 0x70FB20 (error outside 0..0x55 and continues; +0x98 = +0x9C = 0) | done |
| 45 | STOP_MUSIC | 0x70FB90 | done |
| 46 | ATTACH_MUSIC | 0x70FBF0 (error outside 1..84 and adds it anyway) | done |
| 47 | DETACH_MUSIC | 0x70FC60 | done |
| 30 / 31 | START_CAMERA_CONTROL / END_CAMERA_CONTROL | 0x6ECCA0 / 0x6ECEF0 (on release: +0x84 = 1, 0x6ECE74) | state done; the script camera, pending ([below](#dialogue-widescreen-and-script-camera)) |
| 32 | SET_WIDESCREEN | 0x6F7BF0 → HelpSystem::SetWideScreen 0x5C6AD0 | done, with the owner |
| 120 / 122 | START_DIALOGUE / IS_DIALOGUE_READY | 0x710690 / 0x710830 (no audio) | done |
| 121 | END_DIALOGUE | 0x710780 (+0x84 = 1, +0x9C = 0 only if the task has the dialogue) | done |
| 149 | MOVE_MUSIC | 0x70FCA0 | done |
| 181 | ENABLE_DISABLE_MUSIC | 0x70FD10 | done |
| 182 | GET_MUSIC_OBJ_DISTANCE | 0x70FD70 | done |
| 183 | GET_MUSIC_ENUM_DISTANCE | 0x70FDE0 (with an invalid type it pushes twice) | done, with the bug |
| 184 | SET_MUSIC_PLAY_POSITION | 0x70FE60 | done |
| 190 | RESTART_MUSIC | 0x70FF00 | done |
| 191 | MUSIC_PLAYED (object) | 0x70FF40 (true if the object is not valid) | done |
| 231 / 232 | TEMP_TEXT_WITH_NUMBER / RUN_TEXT_WITH_NUMBER | 0x6F7F50 / 0x6F7C70 | done |
| 246 | SPIRIT_SPEAKS | 0x710C40 → ConvertScriptSpiritToHelpSpirit 0x710350, GetSpiritWhoTalks 0x5C6E20 (query) | done |
| 285 / 348 | START_ANGLE_SOUND / StartPitchSound | 0x70FFA0 / 0x70FFE0 | stub |
| 317 | SET_CREATURE_SOUND | 0x710020 | done |
| 335 | LAST_MUSIC_LINE | 0x710050 (without audio: TEXT_READ, 0x7100A6) | done |
| 340 | GAME_PLAY_SAY_SOUND_EFFECT | 0x70F9B0 → 0x70F8E0 | done |
| 350 | MUSIC_PLAYED (type) | 0x70FBA0 (GAudio+0x28 != type; without audio: TEXT_READ, 0x70FBE4) | done |
| 357 | SET_GAME_SOUND | 0x7100B0 | done (`audio::SetGameSound`) |
| 409 | SOUND_EXISTS | 0x710100 → GAudio::IsInstalled 0x426D30 → LHWaveIsInstalled | done |
| 411 / 412 | GAME_CLEAR_DIALOGUE / GAME_CLOSE_DIALOGUE | 0x6FF6F0 / 0x6FF700 | done |
| 424 | STOP_SOUND_EFFECT | 0x70FA50 (isSay: narrator 2 → 0x270C; otherwise, 0x270E + 0x270D; never 0x270F) | done (with isSay for the voices) |
| 445 | ENABLE_DISABLE_ALIGNMENT_MUSIC | 0x710120 | done |
| 447 / 448 | ATTACH / DETACH_SOUND_TAG | 0x710150 / 0x7101D0 → 0x71E840 / 0x71EBE0 | done |
| 450 | GAME_SOUND_PLAYING | 0x710230 → fn_0042A280 | done |
| 458 | SAY_SOUND_EFFECT_PLAYING | 0x710280 | done |

- `GScript::ScriptErrorMessage` is 0x6F62B0.
- LAST_MUSIC_LINE does ftol(line) and then tests +0x98 ≥ it, unsigned (0x71007A..0x71009C).
- GET_MUSIC_ENUM_DISTANCE: a type outside 1..0x54 prints "Invalid music man!" and pushes 0.0 (0x70FDF4..0x70FE28), then
  GetPlayDistance is always pushed (0x70FE34..0x70FE4A).
- `MOVE_MUSIC` 0x70FCA0: "to" is popped first, then "from"; both must be valid (0x70FCEE..0x70FCF4).
- `GET_MUSIC_OBJ_DISTANCE` 0x70FD70: 0 for an invalid thing (0x70FDA1), else fn_004293B0.
- `SET_MUSIC_PLAY_POSITION` 0x70FE60: z, y and x popped first (the LHPoint at 0x70FEDB), then the thing.
- `MUSIC_PLAYED` 0x70FF40: true for an invalid thing (0x70FF71), else IsMusicThingFinished (a bool).

`GetScriptGameThing` 0x70D220 is approximated in `MusicThing` (CHLApi.cpp): 0 is null and a valid entity is a live
object. The original looks the id up in its table 0xD967F8 (1..0x1FF) **(approximate)**. `CHAR2WCHAR` 0x8300A0 is done byte by
byte, without the 0x7FF limit **(approximate)**: it only changes in 0x80..0x9F of CP-1252.

### GScript switches

**Faithful.** They are at g_game+0x250090 and are atomic in openblack, because the music thread writes the line and the hits.
They live in `src/Audio/Services/ScriptAudioState.{h,cpp}`, and `GScript::Reset` 0x6EB2D0 (`ScriptAudioState::Reset`, from
`LoadMap`) leaves them like this:

| field | what it is | after Reset | who writes it and who reads it |
|---|---|---|---|
| +0x84 | creature sounds | 1 (0x6EB2F4) | `SET_CREATURE_SOUND`; `END_DIALOGUE` (0x71080A) and the release of the script camera (fn_006ECD70 0x6ECE74, from `END_CAMERA_CONTROL` and when the task stops) set it to 1. It is read by fn_00483290+0x16A: with 0, only the local creature sounds |
| +0x90 | dialogue only (SET_GAME_SOUND) | 0 (0x6EB403) | stored here for the reset; read by the GAudio filters |
| +0x94 | alignment music | 1 (0x6EB409) | `ProcessAlignmentMusic` does nothing with 0 (0x427A20) |
| +0x98 | last line `L<n>` of the script music | 0 (0x6EB306) | 0x426BD1; START_MUSIC sets it to 0 |
| +0x9C | hits | 0 (0x6EB30C) | 1 on `L`, +1 on `P`/`W`. It is read by fn_005CB590+0x62B, **(inferred)**: a song's text follows it |

`HelpSystem::Reset` (0x6EB340) also goes in `LoadMap`.

- SET_GAME_SOUND false sets +0x90 = 1 and calls LHSampleStopAll; true sets it to 0.
- Write sites: SET_CREATURE_SOUND +0x84 at 0x71003D, ENABLE_DISABLE_ALIGNMENT_MUSIC +0x94 at 0x71013D, START_MUSIC
  +0x98 = 0 at 0x70FB56 and +0x9C = 0 at 0x70FB6B, END_DIALOGUE +0x9C = 0 at 0x710820. LAST_MUSIC_LINE compares +0x98 at
  0x710098.
- END_DIALOGUE writes the audio fields only when the calling task owns the dialogue (HelpSystem+0x45CC ==
  ScriptDLL::TaskNumber, 0x71078C..0x71079F).

### Dialogue, widescreen and script camera

**Faithful** in the state (`src/Help/ScriptControl.{h,cpp}` and `HelpSystem`). The original stores which
script **task** owns each thing, with `ScriptDLL::TaskNumber` 0x6F69F0 (ScriptLibraryR.dll 0x10008320: the current
task, +0x14; 0 outside a task). openblack provides it with `LHVM::GetCurrentTaskNumber` (the task `_currentTask`
running the native function), `GetCurrentTaskScriptType` (0x6F6A90) and `GetTaskScriptType` (0x6F6C50 → 0x100051F0:
+0x158, 1 if the task does not exist).

| field | what it is | who writes it |
|---|---|---|
| HelpSystem+0x45CC | the task that has the dialogue | `DialogueControlRequest` 0x5C6790 (if nobody controls it), `ClearDialogueControl` 0x5C67E0 |
| HelpSystem+0x45E8 / +0x45EC | widescreen and the task that set it (0 = the game) | `SetWideScreen` 0x5C6AD0 (only if +0x45E8 changes; on turning off, +0x45EC = 0) |
| GScript+0xA8 | the task that has the camera | `START_CAMERA_CONTROL` 0x6ECCDF / 0x6ECD5B; 0 on release (0x6ECE59) |
| GScript+0x78 / +0x80 | the leashes / the highlights are drawn | 0 when taking the camera outside the citadel; 1 when releasing it and on Reset |

- `IsDialogueControlled` 0x5C6740 = +0x45CC != 0, or +0x45E8 and +0x45EC. `IS_DIALOGUE_READY` pushes its negation.
- `START_DIALOGUE`: with no owner, both advisors go home (`SpiritHome(1/2, 0)`) and the request; it pushes **true even if
  the request fails** because of another task's widescreen (0x710722..0x71072E). The same task: warning and true. Another one:
  false, unless the owner is Help (type 2) and the requester is Script (1): `StopHelpScripts` (mask 0x4A) and the
  owner is checked again.
- `END_DIALOGUE`: only the owning task. `SpiritHome(1/2, isHelp)`, fn_005C6800 (releases the dialogue, removes the
  widescreen, advisors home, clears the text) and +0x84 = 1, +0x9C = 0.
- `SET_WIDESCREEN`: only the owner, or anyone if there is no owner; if the owner turns it on again (value != 0)
  it warns and continues (0x6F7C23..0x6F7C32).
- `START_CAMERA_CONTROL`: in the citadel (g_game+0x205A28 == 1) only TempleHelp/TempleSpecial tasks (0x18), without
  a camera mode. Outside, if the `CameraModeScript` mode is created (fn_00461140; not if `CantExitCurrentMode`).
- `END_CAMERA_CONTROL`: if the task has it, fn_006ECD70: mode `CameraModeNew3`, FOV 70° (0x8C762C) in 0.5 s,
  +0xA8 = 0, +0x84 = +0x80 = +0x78 = 1, fn_0042A5F0(1), SuperVillagers off, +0x7C = 0.
- When **a task is stopped** (callback 0x6EC6D0, LHVM's `stopTaskCallback`): fn_005C6800(task) and fn_006ECF20(task)
  give back the dialogue and the camera. ScriptLibraryR.dll calls it before removing the task from the list (0x100065CD,
  the removal from 0x10006612): fn_005C6800 still sees its type, the same as in LHVM.

- RUN_TEXT_WITH_NUMBER also turns an out-of-range text into text 0 (0x6F7CE8).
- `HelpSystem::Reset` takes the wide screen away (0x5C55D6), but not the dialogue owner +0x45CC.
- The script camera's release fn_006ECD70 sets the draw-highlight switch to 1 (0x6ECE85) and the draw-leash switch to 1
  (0x6ECE97).

In openblack the visual part is missing: there are no camera modes (it is always taken **(inferred)**, and the camera does not change), no exit
FOV, no advisors (`SpiritHome`, fn_005C6720 are empty hooks), no `DialogBoxBase::HideAll` /
`GInterface::SetActive`. The citadel is openblack's temple interior **(inferred)**. The bars are there:
the `SetWideScreen` hook moves `ScreenFade` (in 16:9 they are not visible, as in the original). The alignment music
already reads the script widescreen (0x4279E9).

In game (Land 1, `FollowUs`): START_CAMERA_CONTROL → START_DIALOGUE → START_GAME_SPEED → SET_WIDESCREEN →
**START_MUSIC 54** (intro.sad plays). With `MOVE_GAME_THING` (033) `FollowUs_loop_4` passes, the
family walks and the singing stones sound (PLAY_SOUND_EFFECT 49/50/54). The camera blocks (035, 003/004) and
SET_AVI_SEQUENCE (203) have been ported since (script-camera.md, video.md); the intro now stops in `Drag` at the hand demo
(map-loading.md, "In game (Land 1)"). The intro speaks: see [Voices in game](#voices-in-game).

## Music and texts in openblack: files and tests

The music, script and text pieces, with their files and tests:

| piece | state | files | test |
|---|---|---|---|
| Tables | done | `src/Audio/Game/BankTables.h`, `components/pack` (`AudioBankInfo`) | `test_audio_tables` |
| MusicBank | done | `src/Audio/MusicBank.*` | `test_music_bank` |
| MusicEngine | done | `src/Audio/MusicEngine.*` | `test_music_engine` (0→127 in 32 passes, 127→0 in 43, 80→0 in 27, sync, 7th track, master) |
| MP2 streaming | done, on `audio::device` | `src/Audio/MusicStream.*`, `src/Debug/Music.*`, `Debug/Gui.cpp` (two lines) | `test_music_stream` |
| Script music | done | `src/Audio/GameMusic.*`, `CHLApi.cpp` | `test_game_music` |
| Script state | done | `src/Audio/ScriptAudioState.*`, `CHLApi.cpp` | `test_game_music` |
| ThingMusic | done | `src/Audio/ThingMusic.*`, `CHLApi.cpp` | `test_game_music` |
| Music volume | partial: `EngineConfig::audioMusicMainVolume` and the debug slider; it is not saved, and it is not in the menu | `src/EngineConfig.h`, `src/Debug/Music.cpp` | trace `gain=` |
| Alignment and tribe | done: alignment at the camera and town/tribe via `ecs::map_cells` (`ECS/AudioQueries.cpp`); audited: camera height as in UpdateGameThingWithPosData 0x442EF0 | `src/Audio/GameMusic.*`, `src/Audio/GameQueries.h` | `test_game_music` (with fake queries) |
| Voice table | done | `src/Common/HelpText.*`, `src/Audio/Voices.*` | `test_voice_table` (6974 / 3477 / 1922 / 1328 / 227, WORKSHOP_10 → villagers 399) |
| HelpSystem (text) | done, without drawing | `src/Help/HelpSystem.*`, `CHLApi.cpp`, `Game.cpp` (click) | `test_help_system` |

The tests that read data use `OPENBLACK_TEST_BW_ROOT` and do `GTEST_SKIP` if there is no installation.

## The effects core in openblack

### Player fixes

| fix | state | where |
|---|---|---|
| Empty sample → `continue` | already done | `Game.cpp` |
| RIFF 0x50 (MPEG layer II) → the MPEG decoder | **done**: HelpSprites 1 = 5.2 s; the 209 of InGame decode | `WaveBuffers.cpp` (`Decode`) |
| One AL buffer per sample, on first use (leak gone) | **done**, also for the old players | `WaveBuffers.cpp` (`Get`), `AudioManager::CreateEmitter`/`CreateBuffer` |
| Finite loops N | **done**: the source loops and each wrap of the offset is one pass; after N wraps it stops looping, so N+1 passes of the span sound **(inferred)** | `SampleOutput.h` (`LoopCounter`), `AlSampleOutput::Update` |
| Span lStart..lEnd | **done** with `AL_SOFT_loop_points` (G_VillageBell 0..27400 of 57855) | `WaveBuffers.cpp`, `Sound::loopStart/loopEnd` (+0x138/+0x13C, `Loaders.cpp`) |
| "Villagers.sad" case-insensitive | **done**: the filter compares banks (`BankId`), not names; `RegisterBank` recognises the 11 types of 0x9CB3F8 by case-insensitive path | `AudioSystem.cpp` |
| AL_PITCH overwritten / per-frame listener | already done | `AudioPlayer.cpp`, `SamplePlay` |

### The core

Layers:
- **0. Device**: `AudioPlayer` (OpenAL context), `AlSampleOutput` (one source per channel, outside the registry; they are
  deleted in `ClearMap`), `WaveBuffers` (buffers and decoders).
- **1. LHaudio/QMixer**: `QMixerLaws` (`qmixer::Gain` 0x100133C1, `DistanceGain` 0x1802CE50, `PolarRelative`
  0x10012269, `FrequencyRatio` 0x10012820, `StartPitch` 0x1001278B) and `SamplePlay` (16 channels: `Start` 0x100113B0,
  `Stop` 0x10012C50 / 0x10012DF0 with the first-channel rule, `StopAll` 0x10012BF0 without the ambience ones, `IsPlaying`
  0x10013ED0 / 0x10013FB0, `ReleaseLoop` 0x10012F20 on the first one, `SetPitch` 0x10013520, `SetVolume` 0x10013400,
  `SetMasterVolume` 0x100150E0, `UpdateChannels` 0x10014310 + listener, `Switch` 0x10015D40, `ClearChannelOwners`
  (was `ClearInfoList`) 0x100142C0).
- **2. GAudio** (`AudioSystem`): banks (`RegisterBank`, `Bank(SfxBank)` = GAudio+0x3A8+4·type, `FindBank`,
  `SampleId`, `CreatureBank`), `PlaySoundEffect` filters 0x429E30 (3D cutoff with +0x58, userParam 1/2/4,
  SET_GAME_SOUND on GScript+0x90 of `ScriptAudioState`, `OwnerUnavailable` 0x429D20), life cycle. (The
  `PlayAnimEffectSample` sample branch of `CollisionSounds` was removed: the physics now passes its key.)
- **3. GameSfx**: the 5 variants 0x429D60 / 0x429DA0 / 0x42A000 / 0x42A040 / 0x42A100, Stop 0x42A210, ReleaseLoop
  0x42A330 / 0x42A310, IsPlaying 0x42A280 / 0x42A2B0 / 0x42A2D0, SetPitch 0x428740 and the cyclic counters
  (`enum class Counter`).
- **4. Public API** `src/Audio/Audio.h`.

Public API (no default arguments; each caller passes what the original passes):

| function | original |
|---|---|
| `PlaySoundEffect(PlayOptions)` | 0x429E30 (own options: Guidance, PSysSound, SoundTag) |
| `PlaySoundEffect(owner, sample, mode, loops, extra3DFlag, is3D, SfxBank / BankId)` | 0x429D60 / 0x429DA0 (3D without owner or with an unavailable owner: nothing) |
| `PlaySoundEffectAt(owner, pos, sample, mode, loops, extra3DFlag, is3D, SfxBank / BankId)` | 0x42A000 / 0x42A040 (track = is3D) |
| `PlaySoundEffectAt(owner, pos, offset, sample, track, mode, loops, extra3DFlag, is3D, BankId)` | 0x42A100 (SoundTag fn_0071E680) |
| `StopSoundEffect(sample, owner, bank)` (sample 0 = all of the owner's) | 0x42A210 → LHSampleStop |
| `StopOwner(owner)` | (openblack) `LHSampleStop(bank, owner, 0)` 0x10012C50 on each registered bank |
| `NewOwner()` | (openblack) `Owner::Object(NewObjectId())`: an owner of its own |
| `StopAllSoundEffects()` | fn_004287D0 |
| `LeaveCitadel()` | Temple fn_00793D00 from LeaveInsideCitadel 0x553B25: Stop 2 and 12 of InGame, owner 0 |
| `GuardSoundPoint(p)` (AudioSystem.h) | fn_00427200: |v| > 5000 → 0 per coordinate |
| `ReleaseLoop(owner, sample, bank)` | 0x42A330 / 0x42A310 |
| `IsPlaying(owner, sample, bank)`, `IsPlaying(owner, SfxBank)`, `IsPlaying(Channel)` | 0x42A280 / 0x42A2D0, 0x42A2B0, 0x10014070 |
| `SetPitch(bank, owner, sample, percent)`, `SetVolume(Channel, v)` | 0x428740, 0x10013400 |
| `NextCounter(Counter)` | counters 0xC4CC7C, 0xC5E3E4/E8, 0xC6421C, 0xC64220, 0xD18228, 0xD4437C, 0xD559AC, 0xD95AF8/FC |
| `MaxDistance(Sample)`, `CreatureBank(species)` | 0x42A430, 0x4EBD81 |
| `RegisterObject(id, fn)` / `UnregisterObject` | Get3DSoundPos (vt +0x10) of an owner that is not a GameThing |
| `Init(GameQueries)`, `Shutdown()` | ctor 0x426D40 (master fn_00428250), ToBeDeleted 0x426FE0 |
| `ProcessTurn()` | GGame::EndTurn 0x54E960 (the turn from `game_clock::Turn()`, the sky type from `sky_type::Frame()`): GSoundMap::Update, ProcessSoundTags, and after turn 5 ProcessAudioGameTurn 0x427080 (with the LHWaveIsActive gate) or AtmosProcess(0) |
| `ProcessCitadelTurn()` | Temple::ProcessGameTurn 0x794A5A: ProcessAudioGameTurn alone, every 100 ms while the game is paused inside the citadel |
| `Paused()`, `UpdateFrame()` | EndTurn while paused (0x54E9B4); the live master and the finite loops |
| `ClearMap()` | GAudio::Reset 0x426CA0 (+ the map's SoundTags and street lanterns) |
| `OnFocus(bool)` | minimise / restore: 0x7DE6D0 / 0x7DE6F0 → 0x642470 → fn_00428720 → LHGlobalSwitch 0x10015790 |
| `SetSampleMainVolume(v)`, `SampleMainVolume()` (were `SetSampleMasterVolume`, `SampleMasterVolume`) | 0x100150E0 / 0x10015170 |

Read in the disassembly:
- 0x42A100 has **10** arguments: the 7th is the loops (+0x4C, 0x42A17A) and the 8th the +0x10 (0x42A121).
- `GAudio::PlaySoundEffect` always returns 0 (0x429FE8): no caller receives the channel.
- `LHSampleStop(bank, owner, sample)` with sample ≠ 0 only stops the **first** matching channel; with sample 0, all
  of the owner's; with the audio off it stops nothing except ambience channels (0x10012C50..0x10012DD1). Before stopping,
  QMixer does a 20 ms volume ramp (SetPanRate 20, volume 0, Sleep(20)).
- `LHSampleStopAll` does not touch the ambience channels (+0x00 ≠ 0, 0x10012C13). `SET_GAME_SOUND false` no longer stops them
  (it used to).
- `LHSampleIsPlaying` and `LHSampleReleaseLoop` only look at the first matching channel and do nothing with the audio
  off (0x10013F69, 0x10012F2A). `LHSampleSetPitch` ignores pitch 0.
- `LHSampleClearInfoList` does nothing with the audio off (0x100142CC), so in `GAudio::Reset`, which calls it between
  `LHGlobalSwitch(0)` and `(1)`, it clears nothing.
- `LHSampleUpdate3DChannels` stops the channel if the distance is **≥** the maximum (0x100143AB) and, if the owner is gone,
  also sets the channel's owner to 0 (0x100143A2).
- In `SamplePlayAnimEffect` the unavailable-owner filter depends only on track, without is3D (0x42A5A1).
- The site of `ProcessAudioGameTurn` in `GGame::EndTurn` (0x54E960..0x54E9B4): GSoundMap::Update, Dump,
  ProcessSoundTags and, if not paused (`g_game+0x14 & 4`) and turn > 5, ProcessAudioGameTurn; otherwise, AtmosProcess(0).

Audible behaviour changes (all because of the original):
- `SET_GAME_SOUND` no longer cuts the ambience (0x10012C13) and its flag goes back to 0 on each map (GScript::Reset 0x6EB403;
  before it was a separate flag that was not reset).
- `Stop`/`ReleaseLoop`/`SetPitch` act on the first matching channel (before on all of them).
- With the bank filter by `BankId`, the villagers are no longer silenced after `SET_GAME_SOUND false`.
- Minimising the window stops the effects and the music (losing focus does not).
- Finite loops end (bell ×5, frogs, birds, doves) and loops with a span repeat only the span on the
  16 channels. (The old players of `AudioManager::CreateEmitter` queued the buffer, and OpenAL Soft only
  uses loop points on static sources; there are none left now.)

Audit of the core:
- Checked in the disassembly: 0x429D20, 0x429D60/0x429DA0, 0x42A040, 0x42A100 (order of the 10 arguments),
  0x429E30 (order of the filters and the `<=` of the 3D cutoff), 0x42A4B0, 0x426E6B (800), 0x54E960, 0x427080, 0x426CA0,
  0x427200, 0x4068F4, 0x63AA39, 0x5D2109 (counters), 0x7DBFF6 / 0x7DE8DC / 0x7DE6D0 / 0x642470 (minimise) and in
  LHaudiodllR 0x10011020 (allocation), 0x10012BF0, 0x10012C50, 0x10012F20, 0x10013400, 0x10013520, 0x10013AC0,
  0x10013ED0 / 0x10013FB0, 0x10014070, 0x100142C0, 0x10014310, 0x100146F0, 0x100150E0, 0x10015D40, 0x10001EBF.
- Fixed: `LHSampleSetVolume` and `LHSampleSetPitch` do nothing with the audio off except on ambience channels
  (0x10013412, 0x10013572), and `SetVolume` acts on the first channel of its (bank, owner, sample) (0x10013489).
  `LHAtmosProcess(0)` stops each stored channel without asking whether it is playing (0x10001ED3); `atmos_banks::StopChannel` likewise.
- Fixed (leak): `audio::ClearMap` did not stop the old emitters (`AudioManager::CreateEmitter`: trees,
  AnimationSounds, fire, spells…); the reset of the new map's registry deleted them with their AL source playing (one
  looping, forever) and on exit "7 Sources not deleted" and "Deleting in-use buffer" were left. In the original they are
  channels and `LHSampleStopAll` 0x426CE6 stops them: now `ClearMap` calls `AudioManagerInterface::DestroyAllEmitters`
  (all except the old music one). Trace: `Sample play: N channel sources released` and `AudioManager: N emitters
  destroyed` with `OPENBLACK_AUDIO_TRACE`.
- Marked **(approximate)**: `LHSampleIsPlaying(info)` gives 0 with the audio off (0x1001407A); openblack looks at the
  handle's start also when off, because its game keeps running minimised and PSysSound (0x6D120A) would relaunch every
  turn. `LHSampleSet3DPosition` also looks for the first channel of the triple and respects the channel's fixed axes (+0x14
  bits 4/8/0x10, 0x10013BCC): openblack moves the handle's channel and has no fixed axes.
- The distance for `SamplePlayAnimEffect` is computed by the caller (`|LH3DTech::g_camera − pos|`).
- The music and the ambience are processed in the order of `ProcessAudioGameTurn` (before, the music went at the start of the turn).
- `audio::GetSurfaceType` called `ecs::sea_cells::GetSurfaceType` (a single source); it no longer exists: the game calls `ecs::sea_cells` and the audio calls `audio::SurfaceType` (query `surfaceType`).

## Anim-effects and SoundTag in openblack

### The anim-effects in the core

| piece | original | where |
|---|---|---|
| Tables of each bank, read **once** when it is registered | `LHBankRegister` 0x10002778..0x100029AB (`LHFileSegmentAnimArray` → bank +0x124 / +0x12C width / +0x130 rows; `LHAudioWaveNumTable`) | `AnimEffects.cpp` `anim_effects::RegisterTables` from the bank loop in `Game.cpp` |
| `AnimEffectTable` (the magic code's `AnimEffectBank`, unchanged: same members, `Load`, `FindList`, `SoundId`, `FindSample`) | `LHFindAttribRow` 0x10014420 (row with the most exact columns; on a tie, the last one, fn_10014610) | `AnimEffects.h`; `AnimEffectBank.h` remains as an alias |
| `anim_effects::Number` | `LHSampleGetAnimEffectNumber` 0x10014670: list of 1 → that sample; otherwise, `list[LH_AudioSystem::Rand(n)]` (0x10015710) | `AnimEffects.cpp`, `sample_play::Random` |
| `anim_effects::Play` | `LHSamplePlayAnimEffect(owner, dist, n, track, bank, min, max)` 0x10014A20: nothing with the audio off; the **caller's** distance ≤ 800 (+0x44, `LHSampleRegister3DObjectFunction` 0x426E6B) and ≤ the sample's maxDist (.sad +0x26C, 0x10014ABD); is3D 1, +0x10 0, min/max with their bits 0x80/0x100 only if > 0; the point is given by the game's 3D function for the owner (fn_00427200: without an owner, the camera; unavailable owner or the ambience one, nothing) | `AnimEffects.cpp`, `audio::OwnerSoundPoint` |
| `anim_effects::PlayKey` | 0x100146F0: same gates (800 also for stopping); action 1 = `LHSampleStop` of the first channel of (bank, owner, sample) for each sample in the list (0x1001491C); other = `LHSampleReleaseLoop` (0x10014990) | `AnimEffects.cpp` |
| `audio::PlayAnimationEffect(owner, dist, key, action, bank, track, min, max)` | GAudio 0x42A4B0: action ≠ 0 goes straight to 0x100146F0 without filters or min/max (0x42A4BC); action 0: number (0x42A4F9; 0 = nothing), userParam filters (1 with the script widescreen, only 2 in the citadel, banks after SET_GAME_SOUND, 4 in states 0x10/0x16/0x17), unavailable owner if track (0x42A5A1), and 0x10014A20 | `Audio.h`, `AudioSystem.cpp` |
| `AnimationSounds::Fire` (signature intact) | fn_00516510: the events with from ≤ t < to; without `Get3DSoundPos` nothing; the distance and the surface are **the villager's** (0x51655F, `GSoundMap::GetSurfaceType` 0x51662B → `ecs::sea_cells::GetSurfaceType`); group 1: dead → nothing more (0x5165BC), voice 3 child / 1 + (+0x1F8 ≠ 0); 0x92 → banter with the house as owner (`GetAbode` 0x51675D; without a house, owner 0 = the camera); 0x93/0x94 → banter on the villager; step (4) of a villager with the script widescreen → **the rest of the list is discarded** (0x5166A4 jumps to the end); THROWN 399/401 only with < 15 / < 10 turns; always track 1, min/max 0 (0x5167A8) | `AnimationSounds.cpp` |
| `AnimationSounds::PlayFromTable` (signature intact, trees) | Tree::Draw 0x74B009 (bend, track 0) / 0x74B25C (rustle, track 1), editor bank (GAudio+0x3B0) | `AnimationSounds.cpp` |

`AnimEffectBank` (from the magic code) remains as an alias of `AnimEffectTable` for its tests; its `Load(path)` copies the tables
that the core read when registering spells.sad (`test_anim_effects` `MiraclesBankCopiesTheCoresTables`: same rows, lists and
samples). `SpellSounds` no longer uses it: it plays through `audio::PlayAnimationEffect`.

Audible changes (all because of the original):
- The sounds of the clips and of the trees go through the 16 channels (priority, mode, stealing) and follow their owner once per
  turn (`LHSampleUpdate3DChannels`), not every frame.
- GAudio's filters: in the Land 1 intro (the script widescreen lasts until a click) the userParam 1 samples do not
  sound: the tree rustles 307..319 and **all** of VillagersBanter.sad; the creaks 320/321 do. Nor do the villagers'
  footsteps.
- The 0x92 banter is heard at the house but is cut by the villager's distance: with the camera 4 m from him it now sounds
  (before it measured from the house and almost never did). Without a house it sounds at the camera (before nothing).
- Stopping (the saw) stops the first channel of the triple (before all of the owner's emitters).

`OPENBLACK_ANIM_TRACE` trace before/after (Land 1; camera 4 m from villager 0 on turn 100; forced clips
437 yawn, 354 saw, 369 sitting; WATER miracle; the "after" with `OPENBLACK_AUDIO_TEST_NO_WIDESCREEN=1` to remove
the intro filter): same lines and same cases — tree rows with the 15 samples 307..321, footsteps 259..268 on
surfaces 2/3, saw 229/230 on 2/3, `banter N too far (D > 8)` and the 4 lines of the water PSys the same (spells.sad/51).
Difference: the yawn gives 4 banter that sound (`VillagersBanter.sad/N`) and before 0, because of the villager's distance. Without the
hook (faithful) only the creaks 320/321 sound. The core's rejections (800, maxDist) go to `OPENBLACK_AUDIO_TRACE`.

### SoundTag

`src/Audio/Services/SoundTags.{h,cpp}` (SoundTag.cpp 0x71E300..0x71ED90), API `audio::tags`:

| function | original |
|---|---|
| `Create(thing, sample, track, mode, loops, extra3DFlag, is3D, SfxBank, delay)` | 0x71E840 (offset 0) |
| `Create(thing, offset, …)` | fn_0071E8C0 → ctor 0x71E300: the point is the thing's MapCoords (x, GetAltitude + y, z) |
| `Create(point, …)` | fn_0071EA40: sounds **right away** through 0x429E30 (owner the tag, track 0, mode +0x50, without +0x10) except 3D with delay |
| `CreateAtMapCoords(x, z, height, …)` | 0x71EB60 (`GameQueries::landAltitude` = LH3DIsland::GetAltitude 0x803090) |
| `SetActive(tag, b)` | fn_0071E640: an active tag that is switched off cuts its sample (0x42A210) |
| `Remove(thing, sample, SfxBank)`, `Remove(…, stop)` | 0x71EBE0 / 0x71EC30: each tag from fn_0071ED60 (thing, sample, type) → ToBeDeleted; with stop, 0x42A210 first |
| `Delete(tag)` | ToBeDeleted 0x71ECB0 → CreateSoundTagForDeadObject 0x71ECD0: forgets the thing; if the sample is playing with loops (fn_0042A460 +0x40 and fn_0042A2D0) it releases the loop (0x42A310) and the tag lives until it ends; otherwise, it is deleted (whatever is playing continues) |
| `RandomSample(first, n)` | 0x71ED40 |
| (internal) `ProcessSoundTags` | 0x71E5F0 from `GGame::EndTurn` 0x54E989, from newest to oldest; fn_0071E680: with a thing, not functional / no point → ToBeDeleted, inactive → nothing, otherwise 0x42A100 (point, +0x1C, sample, +0x30, mode, loops, +0x40, is3D, bank) every turn; without a thing: the delay (`CheckDelay` 0x71E760: sounds when 347 ([0x980530]) · turns · [0xD01A38] ms ≥ the distance, if it is within maxDist and active; the delay runs out anyway) or it is deleted when its sample stops |
| (internal) `TagSoundPoint` (was `Get3DSoundPos`) | 0x71EC90: its thing's; without a thing it answers 1 without writing, so the channel keeps its point (`SamplePlay::UpdateChannels`) |

`SoundTag::Set` 0x71E4F0: active 1, track only with a thing (0x71E55D), delay only if is3D (0x71E56B), +0x34 = 0 in all
the ported forms (bank = GAudio+0x3A8 + 4·type, GetBank 0x71E610). There is a fourth form **not ported**: fn_0071E920
(ctor fn_0071E460, +0x34 = 1 → ambience bank GAudio+0x194 + 4·type), a point tag that sounds right away through 0x429E30 except
3D with delay; its only caller is the thunder in `GWeather::Update` 0x83FC62 via the callback [0xEEA388] = 0x429CE0
(sample 2 + GetTickCount() % 11, mode 2, 3D, type 12, delay 1). It remains pending along with the thunder. `CreateAtMapCoords`
receives x/z in world units: the original's MapCoords stores them as integers and scales them by 1/6553.6 ([0x8AA3A4],
0x71EB8A/0x71EBA6). The older names (`sound_tags::Create(TagDesc)`, the waterfall of
`DesignedScenery`) remain on top: a `TagDesc` without a thing is the waterfall's ScriptMarker (a GameThingWithPos that does not
go away), which is repeated as a thing tag.

`LanternSounds` is now a street lantern with its tag (API intact: `SetOn`, `ProcessTurn`, `Clear`):
`CallVirtualFunctionsForCreation` 0x734810 → fn_0071E8C0(lantern, (0, Object::GetHeight, 0), 0x93, 0, 2, −1, 0, 1, InGame, 0)
and `SetActive([0xDA0A10])` 0x734965; `SetOn` = fn_007349E0 (SetActive of each lantern on change). A lantern that goes away
leaves its tag like that of a dead object (it releases the loop). It sounds the same as before: at the tip, when approaching to less than
5 (0x429E30), looping, and it is not cut when moving away (channel without track); now it counts in the 16 channels and respects GAudio's
filters.

Tests: `test_sound_tags` (10: mode 2 only re-triggers if not playing, mode 3 restarts, out of range and inactive, the thing that
goes away releases the loop and the tag waits for it to end, one without loops goes away immediately, point once, delay at 347/s, Remove with and
without stop and by bank type, lantern at its tip) and `test_anim_effects` (7). In game (`OPENBLACK_TIME_OF_DAY=22`,
`OPENBLACK_AUDIO_TEST_LANTERN=100`): 12 lanterns with a tag, `G_Lantern_01.wav mode 2 owner sound tag`, looping, with the
camera 3.2 from the tip.

Original facts:
- The tag list is g_game+0x205C1C; new tags go at its head (0x71E31F..0x71E339). ProcessSoundTags reads the next tag
  before processing one, because a tag may delete itself (0x71E600).
- The channel's owner is the tag itself (0x71E6F1).
- Tag fields: +0x10 the point, +0x4C active (1 from Set, 0x71E4FA). +0x30 = 0 without a thing (0x71E568). A turn
  counter goes up on each pass (0x71E686). The dtor is fn_0071E5A0. CreateSoundTagForDeadObject clears the thing at
  0x71ECD3.
- fn_0071E680, one pass of a tag: IsFunctional (vt +0xD4) false (0x71E69A), or Get3DSoundPos ≠ 1 (0x71E6B8), →
  ToBeDeleted; inactive (+0x4C, 0x71E6A4) → nothing; otherwise it plays at 0x71E6F2; a tag without a thing goes to its
  delay (0x71E71B), and otherwise is deleted once its sample stops (fn_0042A2D0).
- CheckDelay: it measures `GCamera::GetDistanceSq` of +0x10 (0x71E7A6); while reach² < dist², the sound has not reached
  the camera yet (0x71E7D4); it plays through 0x42A040 when dist² < max² and the tag is active (0x71E7E1..0x71E7F9); the
  delay is cleared at 0x71E825.
- The point form fn_0071EA40 builds the tag with fn_0071E3E0 (no thing, no offset).
- Street lanterns: `GStreetLantern::ToBeDeleted` 0x734AB0 only clears +0x60 and unlinks the lantern, so its tag finds
  the thing gone by itself. `InitStaticsValues` 0x54A84F clears the lanterns' tags and the on flag for a new map.
- Original callers: `Windmill::CallVirtualFunctionsForCreation` 0x405974 → SoundTag::Create(windmill, 13 G_windmill, 0,
  2, −1, 0, 1, InGame, 0); `Creature::SetCreedPower` 0x4F8ABD / 0x4F8AD2 → SoundTag::Create(creature, 166 G_Creed_01, 0,
  2, −1, …), then Remove.

**Audit.** Checked in the disassembly 0x42A4B0, fn_00516510 (0x5165BC, 0x5166A4, 0x51675D, 0x5167A8), Tree::Draw
0x74AFE1/0x74B1FA, 0x10014A20, 0x100146F0 (0x1001491C/0x10014990), 0x10015710, fn_10014610, 0x71E300, 0x71E4F0, 0x71E5F0,
0x71E640, fn_0071E680, 0x71E760, 0x71EA40, 0x71EB60, 0x71EBE0/0x71EC30, 0x71EC90, 0x71ECB0/0x71ECD0, 0x71ED40,
0x734920/0x734965: they add up. Only comments fixed (the ambience form fn_0071E920 that was missing, the MapCoords
scale, the stale point of 0x427209). Note for the trees code: `Trees.cpp` passes column 1 of the key = 2 and the
original 0 (ebp = 0, 0x74AB6A); no audible effect, all rows of editor.sad have a wildcard in that column.

## The world's callers and the effects CHL in openblack

### The world's callers on the channels

Each site goes through `audio::` with what its original call passes (checked in the disassembly). Nobody outside
`src/Audio` calls OpenAL (`AudioManager` no longer exists, [below](#a-single-engine)).

| openblack site | original | what it does now |
|---|---|---|
| `HandHolding` `PickUp` | `GInterface::GenericPickup` 0x5D2800 (0x5D2881..0x5D295D) | `tags::Create(object point, 10 G_PickUpObject, 0, 3, 0, 0, 3D, InGame, 0)` (0x71EB60) except for a rooted tree; a live villager (`Object::IsAlive` 0x402610) screams with another identical tag: child 180, woman 194, man 187 + `GetRandomSample(7)`. A `DeadTree` also sounds (it has no `IsTree`: `GameThingWithPos::IsTree` 0x402320 = 0) |
| `HandHolding` (uprooting) | `Tree::InterfaceSetInMagicHand` 0x74B730 | `tags::Create(tree point, 32 + GetRandomSample(3), 0, 3, 0, 0, 3D, InGame, 0)` if it does not have +0x24 & 0x40 |
| `HandPhysics` (dropping a tree) | `Tree::DropSfx` 0x74BC60 | options: InGame, owner the tree, 3D, track 0, 83 + GetTickCount() % 3, at its point |
| `HandResources` (wood to the store) | `Object::DoDeleteObjectAndTakeResource` 0x63AA93 | options: InGame, owner the object, 3D, track 0, 155 + `Counter::TreeMulch` ((c + 1) & 3) |
| `HandFish` (grabbing land) | fn_005D1AB0 0x5D1FE4 | `tags::Create(point, 4 + GetRandomSample(6), 0, 3, 0, 0, 2D, InGame, 0)`: the tag is the owner (new channel on each grab) |
| `HandFish` (hand in the water) | fn_005D1AB0 0x5D2167 | options: 99 + `Counter::HandInWater`, 3D, track 0, at (x, 0.2, z) |
| `HandEffects` (multi-pickup) | fn_0068F930 0x68F9E8 / 0x68FA25, StopMultiPickup 0x68FABA / 0x68FAD4 | `PlaySoundEffect` 44/98 3D track 0 at the hand every turn, `SetPitch(InGame, 0, n, 60 + 180 t)`, `StopSoundEffect(44/98, 0, InGame)` |
| `hand_detail::PlaySample(id)` | `FailApply` fn_005D18F0 0x5D1941 | `PlaySoundEffect(NULL, n, 3, 0, 0, 0, bank)` 2D (used by `HandSpellSeed`) |
| `hand_detail::PlaySample3D` | 3D options form, track 0 | no callers; the signature is kept |
| `Trees.cpp` `PlayAt` (growing) | `Tree::ApplyWaterSpell` 0x74C500 | 3D options, track 0, at the tree, through the channels; **(approximate)**: the signature does not carry the tree and the channel has no owner (the original +0x20 = the tree, 0x74C4D4) |
| `Rocks::Tap` | `Rock::InterfaceTap` 0x6E751D | options: InGame, 130 + `Counter::RockTap`, owner the rock, 3D, track 0, at the hand's point (state +0xC8) |
| `DefaultWorldCameraModel` | `CameraModeNew3::FlyToPosFoc` 0x45899B, `Update` 0x45E305 | `PlaySoundEffect(0, 46 + (GetTickCount() & 3), 3, 0, 0, 2D, InGame)`. `SetFlight` (markers, script and hook flights) only if the camera is more than [0x9CE640]·1.5 = 150 from the destination (0x458967); the double click (`Update`) sounds with its own mark, without that test |
| `PhysicsObjects` (G_RockPast) | `PhysicsObject::GameTurnUpdate` 0x645C12 | `PlaySoundEffect(0, 69 + GetTickCount() % 5, 2, 0, 0, 2D, InGame)` |
| `CollisionSounds::AttemptToAddSoundEvent` | 0x646919 | `PlayAnimationEffect(object, |g_camera − point|, {nivel, 0, A, B, 75}, 0, editor.sad, track = A ≠ 0x16, 0, 0)`: the row is chosen by the bank's table (`LHSampleGetAnimEffectNumber`), no longer by the resolved 99-row table; the channel belongs to the object and sounds at its point |
| `Buildings.cpp` | `Abode::ReactToPhysicsImpact` 0x406610, `ApplyEffectsDueToPhysicalDestruction` 0x40671D | `CollisionSounds::PlayAnimEffect` with the building as owner: hit {2/3, 0, 0x16, 0x10, 75} (p > 1000 / > 300), collapse {1, 0, 0x16, 9, 75}, track 0 |
| `MissionaryBoat` (was `PetitNavire`) | `PostDraw` 0x5E0465..0x5E04B9 | options: ScriptSfx 62/61/60, 2D, mode 2 |
| `PotResource::PlayPileSound` | fn_0066D1A0 0x66D26A | options: InGame, owner the pile, 3D, track 0, sample by GetTickCount (signature with the pile) |
| `DesignedScenery` (waterfall) | 0x5E3921 / 0x5E3BD5 | unchanged: the older `sound_tags::Create(TagDesc)` is already a core `tags::Create` (the original's marker is emulated with a point tag that is repeated as a thing tag) |

`audio::TickCount()` is the original's `GetTickCount` (ms of the process clock) for those that choose a sample with it.

Audible changes (all because of the original): picking up, uprooting, planting, mulching and hitting rocks sound **in 3D** where
it happens (before 2D and, except water and pots, outside the channels); when picked up a villager screams; a dead tree sounds when
picked up; mulching uses the counter and not randomness; the markers' woosh no longer sounds on short jumps (≤ 150); the
collisions choose their sample with the bank's table and follow the object; everything counts in the 16 channels and respects
GAudio's filters (e.g. G_RockPast, TreeFall, KnockRoof, HandThroughInfluence are userParam 1: they go silent with the script
widescreen).

### Effects CHL

`src/Audio/Services/ScriptSound.{h,cpp}` (`audio::script_sound`), called from `CHLApi.cpp` with the original's POPs:

| CHL | original | openblack |
|---|---|---|
| PLAY_SOUND_EFFECT(sample, bank, pos, withPos) | 0x70F7F0: default options, bank GAudio+0x3A8 + 4·bank, sample, **owner = the sample** (`Owner::Key`), is3D = withPos, track 0, point, +0x164 = 1 | `PlaySoundEffect` |
| STOP_SOUND_EFFECT(isSay, id, bank) | 0x70FA50: without isSay, `StopPlayingSoundEffect(id, id, bank)`; with isSay, the text's voice (table 0x942B38) on 0x270C (narrator 2) or on 0x270E and 0x270D; never 0x270F | `StopSoundEffect` |
| GAME_SOUND_PLAYING(sample, bank) | 0x710230 → fn_0042A280(sample, sample, bank) | `GameSoundPlaying` |
| ATTACH_SOUND_TAG(threeD, sample, bank, obj) | 0x710150 → `SoundTag::Create(thing, sample, track = threeD ≠ 0, 2, 0, 0, threeD, bank, 0)` 0x71E840 | `AttachSoundTag` |
| DETACH_SOUND_TAG(sample, bank, obj) | 0x7101D0 → `SoundTag::Remove(thing, sample, bank)` 0x71EBE0 | `DetachSoundTag` |
| SOUND_EXISTS | 0x710100 → GAudio::IsInstalled 0x426D30 → LHWaveIsInstalled | `audio::SoundExists()`: the audio is on an OpenAL device (`device::IsOpen()`) **(approximate)** |
| SET_GAME_SOUND(on) | 0x7100B0 | `audio::SetGameSound` |

The object of ATTACH/DETACH is given by `MusicThing` (GetScriptGameThing 0x70D220, approximated the same as for music). A bank
outside 1..10 neither sounds nor stops **(approximate**: the original indexes GAudio+0x3A8 without checking).

Test `test_script_sound` (10): PLAY(93, 5, p, 1) → 3D channel with owner `Key(93)` without track; GAME_SOUND_PLAYING true and
false after STOP(0, 93, 5); 2D without distance cutoff; 3D beyond its maximum does not start; bank 11 → nothing;
SET_GAME_SOUND(false) leaves only HelpSprites/Villagers; ATTACH(1, 126, 5, thing) → mode 2 tag owning the channel, 3D with
track, does not re-trigger while playing, DETACH releases the loop and the tag goes away when it ends; ATTACH 2D without track; without a thing,
nothing; SOUND_EXISTS false without a device.

In game (Land 1, `OPENBLACK_SFX_TRACE=1`): the script
`FollowUs` plays the singing stones (`PLAY_SOUND_EFFECT(49/50/54, 5, point, 1)`: Stone1/2/6 3D, owner the sample);
far away they are cut by their maximum (100) and with the camera next to them (`OPENBLACK_CAMERA_LOCK`) they sound on a channel. Also
heard are the woosh of the `OPENBLACK_CAMERA_FLY` flight (2D), the rock hits (3D with the rock as owner), the water
grab and the pickup (tags), the collisions of the thrown tree ({1/2, 0, 20, 9/12/16, 75} → G_Crash_Tree_*) and the mulching.

**Audit.** Checked in the disassembly 0x70F7F0 (POPs and fields +0x04 / +0x08 / +0x0C / +0x20 / +0x24 / +0x30..+0x38 /
+0x164), 0x70FA50 (isSay, table 0x942B3C/+0x40, 0x270C/0x270E/0x270D), 0x710150 / 0x7101D0 (order of POPs and arguments
of SoundTag::Create / Remove), 0x5D2800 (0x5D2881..0x5D295D: tag 10, screams 180/194/187 + rand 7 with IsAlive
0x402610), 0x74B730, 0x74BC60, 0x63AA13, 0x6E74B8, 0x458967, 0x45E119..0x45E305, 0x645BEE, 0x406511 (B = ebp = 0x10 from
0x40626C) / 0x406640, 0x66D1A0, 0x5D1933, 0x68F9E8, 0x5D1FC4, 0x5E0413: they add up. DeadTree inherits from Rock
(bw1-decomp DeadTree.h), not from Tree: it is not IsTree and it sounds when picked up. Only one camera comment fixed (the
double-click woosh flag is [esp+0x23], not [esp+0x4B]; the [esp+0x4B] case, distance 1000, remains pending). In game:
PLAY_SOUND_EFFECT(49/50/54, 5, point, 1) with the camera next to it → Scriptsfx 3D, track 0, owner `key
0x31/0x32/0x36`, on a channel. (Approximate, no audible change) the pickup and uproot tags use the Transform's point,
the original GetAltitude + the height of its MapCoords (0x71EB60).

## Voices on channels in openblack

Read in the disassembly: 0x5BB060..0x5BB8A7, 0x5BCD00, 0x5C36D0..0x5C3842, 0x5C52C0/0x5C5290, 0x5C6020..0x5C60DB,
0x5C6A7E..0x5C6AB4, 0x5C6E20, 0x70F8E0, 0x70F9B0, 0x710280, 0x710350, 0x710C40, 0x428850..0x428EE1 (CalcKey,
fn_00428A80, Analyse, four1), the HelpDude init 0x5C1EA1..0x5C1F24 and, in the DLL, 0x10012BF0..0x10012F13 (LHSampleStop),
0x10014C00 (LHSampleGetPlayPosition) and 0x10015180 (LHSampleGetPercentageDone).

### API (`src/Audio/Services/Voices.h`, `src/Audio/Services/Advisor.h`)

| function | original | what it does |
|---|---|---|
| `voices::RunTextVoice(narrator, voice)` | fn_005C5F90 0x5C6025..0x5C60DB | HelpSprites with narrator 2/3: `advisor::Stop(1)`, `Stop(0)` and `advisor::Say(0/1, sample, 0)`; any other voice with bank and sample: default options, owner 0x270F, +0x164 = 1, 2D, through `GAudio::PlaySoundEffect` (with its filters) |
| `voices::Say(text, withPos, alt, point)` | SaySoundEffect 0x70F8E0 | table 0x942B38; without a sample, nothing; owner alt ? 0x270D : 0x270F; 3D if withPos (the point only then); track 0; +0x164 = 1. No text and no advisor, even if it is HelpSprites |
| `voices::IsSaying(alt, text)` | 0x710280 | fn_0042A280(alt ? 0x270D : 0x270F, sample, bank) |
| `voices::CutByClick()` | 0x5C6AA4..0x5C6AAD | `StopPlayingSoundEffect(0, 0x270F, VILLAGERS)`: all villagers narration, with the 20 ms ramp |
| `voices::BankRegistered(bank)` | fn_005C62F0 0x5C631E | GAudio+0x3A8 + 4·bank ≠ 0 |
| `advisor::Init(bank)` | fn_005C3660 → fn_005BB060 | the HelpSprites bank and its number of samples in both advisors |
| `advisor::Say(advisor, sample, onlyIfSilent)` | HelpDudeControl::Say 0x5C36D0 | delay = v < 0 ? 0 : min((v + 1)·250, 500) ms with v = \|+0x3514\| − 0.95 (0x915438); SaySentence; +0x74 = 1 |
| `advisor::SaySentence(advisor, sample, onlyIfSilent, delay)` | HelpDude::SaySentence 0x5BB340 | g_speaker = advisor; with a sentence playing, nothing if onlyIfSilent and it is speaking, otherwise StopSentence; sample in 1..n; start at GetTickCount + delay |
| `advisor::Update(dt)` | HelpDudeControl loop 0x5C3B05 → Update1 0x5BDDA0 | per frame: UpdateSaySentence 0x5BB610 (PlaySample 0x5BB530: `LHSamplePlay` directly, owner 0x270C, without GAudio's filters; copies the PCM) and ApplyLipSync 0x5BCD00 |
| `advisor::IsTalking`, `TalkingOrJustStopped`, `PercentageDone`, `StopSentence`, `Stop` | 0x5BB760, 0x5BB730 (+200 ms), 0x5BB7C0, 0x5BB840, fn_005C3750 | as in the original, with g_speaker 0xD15AA0 and g_sentence 0xD15A9C as globals |
| `advisor::Interrupt(advisor, arg)` | fn_005C3780 (from fn_005C6720 → fn_005C4C20) | picks a HELP_TEXT_INTERRUPTION_* (0xE3F/0xE4C + rand 5, or 0xE44/0xE51 + rand 8 with arg 0 and LocalRand(2) = 0), Stop(advisor) and, outside the citadel, if PercentageDone < 0.9 (0x915440), says it |
| `advisor::AnyTalking()` | 0x5C6372..0x5C63A0 | +0x74 && fn_005BB730(0) \|\| +0x78 && fn_005BB730(1): the advisor branch of IsTextRead |
| `advisor::LipSyncKey`, `Amplitude`, `ComputeLipSyncKey` (was `CalcKey`), `Analyse`, `FastFourierTransform` (was `Four1`), `BandLevel` | +0x2F60, fn_005BB420, 0x428850, 0x428C60, 0x428D50, fn_00428A80 | the mouth, without drawing it (below) |
| `help::SpiritWhoTalks`, `help::ResolveScriptAdvisor` (was `ConvertScriptSpiritToHelpSpirit`) | 0x5C6E20, 0x710350 | SPIRIT_SPEAKS |
| `sample_play::PlayPosition`, `PercentageDone`; `SampleOutput::StopRamped`, `PlayPositionMs` | 0x10014C00, 0x10015180, 0x10012C50 | position in ms, fraction done, the LHSampleStop ramp |
| `audio::SetBankSampleCount` / `BankSampleCount` | LHBankGetNumberOfSamples | the upper bound for SaySentence |

### How it works

- **RUN_TEXT** (HelpSystem::SayText): the `sayVoice` hook calls `RunTextVoice` with the text's narrator.
  **TEXT_READ** with a voice: advisor → read when none is speaking or went quiet less than 200 ms ago; narration → +450 ms
  after the last time it sounded (the queries are connected).
- **Click** (`ProcessInterface`, with the script widescreen or the key): `spiritStop(1, 1)`, `spiritStop(2, 1)`
  (`advisor::Interrupt`) and then `CutByClick`. Only villagers is cut; the generic HelpSprites and Guidance narration
  continues. As in W120, the interruption sentence **is never said**: fn_005C3780 reads PercentageDone after Stop, which
  no longer leaves a speaker (it gives 1).
- **Advisors**: a single speaker (g_speaker). A SaySentence without onlyIfSilent with a sentence playing stops it and leaves the
  new one **without a speaker** (g_speaker is set at 0x5BB343 before the StopSentence at 0x5BB377, which clears it at 0x5BB89F):
  it is from the original, and the RUN_TEXT path does not go through there (it stops both first).
- **Lip-sync**: ApplyLipSync takes the time from `LHSampleGetPlayPosition` (ms) and calls ComputeLipSyncKey(dt, t) on the PCM
  (window 0x200): Analyse (triangular window with step 1/(n·32768), four1 from Numerical Recipes, modulus
  √((re²+im²)/n)) and the three AutoVoiceParams bands (200-400 Hz ×0.85, 400-700 ×1.1, 700-10000 ×10; threshold 0.05,
  level 2.5, rate 40·dt·0.18 downwards and double upwards, normalised if they add up to more than 1). If the sentence is no longer
  playing and t > 0.5 s, StopSentence. Not drawn: the pose (fn_005BF810), the mouth (fn_005BCBC0), the wave's AudioTags
  (BuildAudioTags 0x42AE70) and the gestures: there is no advisor model.
- **Lazy dialogue banks**: HelpSprites, villagers, VillagersBanter, SpellDialogue and Guidance are registered only
  with their headers (`PackFile::ReadAudioHeaders`) and each wave is read from the .sad when it is decoded, like
  `LHBankRegister(path, 0)` (about 107 MB that are no longer read at startup).
- **LHSampleStop with ramp**: all stops by sample, by owner or by channel (and the cutoffs of
  `LHSampleUpdate3DChannels`) go down to 0 in 20 ms, like the DLL; the game no longer waits those 20 ms (the fade goes on
  in the background). `StopAll` and the resets, abruptly.
- **CHL**: GAME_PLAY_SAY_SOUND_EFFECT (340), SAY_SOUND_EFFECT_PLAYING (458) and SPIRIT_SPEAKS (246) done.
  STOP_SOUND_EFFECT with isSay is `script_sound::StopSoundEffect`.

### Voices in game

Land 1 (`OPENBLACK_TEXT_TRACE`, `OPENBLACK_AUDIO_TRACE`, `OPENBLACK_SFX_TRACE`). The `FollowUs` intro speaks and progresses: the family's twelve voices
(`HELP_TEXT_DEFINITELY_NEWEST_INTRO_01..12`, villagers 57..68, 2D, owner 0x270F or 0x270D) with their
SAY_SOUND_EFFECT_PLAYING waits; then the RUN_TEXTs with voice "¡Has salvado a nuestro hijo!", "¡Gracias! ¡Gracias por tu
misericordia!" (woman, villagers 119/120), "¡Te alabamos!" (with click) and the advisors "Saludos.", "Somos tu
conciencia.", "Bueno.", "Y malo.", "Yin y Yang.", "Blanco y Negro.", "Como parte de ti, te guiaremos por este mundo."
(HelpSprites, owner 0x270C); then "Nuestra gente te rendirá culto.", "Ten la bondad de acompañarnos a nuestro
Pueblo." and "Te enseñaré cómo seguirlos.". The texts with a click are passed with `OPENBLACK_TEST_TEXT_CLICK=1` (the click, with
the widescreen, cuts the villagers voice, as in the original). It used to stop at HAS_CAMERA_ARRIVED (035);
since the script camera was ported it goes on into `Drag` and says text 5128 (HELP_TEXT_HAND_DEMO_DRAG_01, sample 497)
before stopping at the hand demo (HAND_DEMO_TRIGGER 336, pending; map-loading.md, "In game (Land 1)").

### (Approximate) and (inferred): voices

The advisors' visual part (HelpDude: model, flight, mouth from the lip-sync key) is ported:
[intro.md](intro.md#the-advisor-spirits). Still pending there: the gestures from the samples' cue labels.

- **(approximate)** the advisor's delay is always 0: +0x3514 belongs to the advisor's flight (not ported) and stays at
  the init's 0 (0x5C1A61).
- **(approximate)** QMixer's 20 ms ramp in four 5 ms gain steps, faded in the background instead of the game waiting the
  20 ms: the same fade, the cut instant may move by a few ms.
- **(approximate)** the advisor's PCM is decoded again (the DLL converts it with ACM on start with +0x164);
  four1/Analyse/CalcKey with the x87 registers in double.
- **(approximate)** only the Dialogue banks are lazy; the rest is read whole (the original registers them all
  that way).
- **(inferred)** spirit 1 (HelpSystem+0xC) is advisor 0 (fn_005C5250: +0x54 ≠ 1); in SPIRIT_SPEAKS, the
  local player is PLAYER_ONE; LocalRand with openblack's generator.

### Audit: voices

Checked in the disassembly 0x5C36D0 (delay |+0x3514| − 0.95 double 0x915438, (v+1)·250, cap 500), 0x5C3750, 0x5C3780
(+0x7C = ebp, 0x900D48 = another identical copy of 0x915D40, 0.9 double 0x915440), 0x5C5290/0x5C52C0, 0x5BB340,
0x5BB530, 0x5BB610, 0x5BB730/0x5BB760/0x5BB7C0/0x5BB840, 0x5BCD00, 0x428850 (bands, the strongest band, limits
dt·rate·0.18 and double), 0x428A80, 0x428C60, four1 constants (0x8C49F8, 0x8AB260, 0x8C49F0), init 0x5C1EA1..0x5C1F24,
0x5C6025..0x5C60DB, 0x5C62F0, 0x5C6340, 0x70F8E0, 0x70F9B0, 0x710280, 0x710C40, 0x710350 (table 0x710400), 0x5C6E20,
0x5C6A7E..0x5C6AAD, 0x5C6720 → 0x5C68A0 → 0x5C4C20 → 0x5C5250, and in the DLL 0x10012BF0, 0x10012C50, 0x10012DF0,
0x1001439D/0x100143BC, 0x10014C00, 0x10015180: they add up. Fixed: `advisor::Reset` claimed to be "HelpSystem / the map
change", without a source (HelpSystem::Reset 0x5C5580 does not touch HelpDudeControl): now it is only for the tests and
no game code calls it; removed the default argument of `PackFile::ReadBlocks`. Clarification ("the click cuts villagers
but not HelpSprites"): the click cuts villagers/0x270F with 0x42A210 and also **stops the advisor who is speaking**
(fn_005C3780 → fn_005C3750, without an interruption sentence in W120); the HelpSprites 0x270F narration (narrator ≠ 2/3)
is not cut. No leaks: the advisor only decodes PCM (no AL buffer), `ReadWave` closes its stream; the voices have no
owning entity and `ClearMap` → LHSampleStopAll cuts them; the music thread does not touch `sample_play`.

## The miracles on channels in openblack

The magic code's files keep their logic intact. Read in the disassembly: 0x6745D0 (`AtomCore::StartSound`), 0x6D0F70..0x6D13A0
(`PSysSound`: ctor, destructor, `Get3DSoundPos` 0x6D1000, the loop fn_006D11A0), 0x72EBE0 (`FireEffect::ToBeDeleted`),
0x72EDC0/0x72EDE0/0x72EE70 (fire), 0x730760 (`ProcessList`), 0x5D2730/0x5D27B0 (gesture loop), 0x5CEC50
(`GInterface::Get3DSoundPos`), 0x6882F0/0x688560..0x68866A (recognised gesture), 0x683184..0x68327F (passing
fireball), 0x68CE90 / 0x68DE69 (`PHandFX`), 0x72A640 (`OneOffSpellSeed::InterfaceTap`), 0x5EC340 (`Living::MoveByTeleport`),
0x726490 (`PlayTapSound`), 0x77F4E0 (`PlayFullyChargedSoundFX`), 0x729C40, 0x7314E0/0x731AB0 (steam) and, in the DLL,
0x10014010 (`LHSampleIsPlaying(bank, owner, &info)`).

### New API (`Audio.h`)

| function | original | use |
|---|---|---|
| `NewObjectId()` | (openblack) the original compares owner pointers | a number for the `Owner::Object` of each ParticleSound (was `PSysSound`), FireEffect, PHandFX, gesture data, FireGraphic |
| `PlayingChannel(owner, bank)` | `LHSampleIsPlaying(bank, owner, LH_SampleInfo**)` 0x10014010: the first channel of the bank and owner (any sample), if it is in use; nothing with the audio off | ParticleSound 0x6D120A |
| `Volume(channel)` | `LH_SampleInfo` +0x38 | the ParticleSound fade 0x6D1223 |

### Each site

| site | original | now |
|---|---|---|
| `spell_sounds::StartSound` | 0x6745D0: global position (+ ground with SnapToGround), surface with USESURFACE (`ecs::sea_cells::GetSurfaceType`), Delayed → delay distance/347 (0x6747AE); otherwise, `SamplePlayAnimEffect(this, dist, {size, alignment, 1, surface, action}, 0, spells, track 1, 0, 0)` | `Owner::Object` registered with `RegisterObject` (its `Get3DSoundPos` 0x6D1000: the atom with ground if SnapToGround; without an atom, the last point) |
| `spell_sounds::ProcessTurn` | fn_006D11A0: without an atom, `LHSampleIsPlaying` directly (none → it is deleted), fade `max(vol − FadeStep, 0)` with `LHSampleSetVolume` on **that** channel, and once `SamplePlayAnimEffect(this, 0, key, 1 + SoftRelease, …)` (1 stop, 2 release the loop); with an atom, Looping or expired Delayed → again within 1200 of the camera | `PlayingChannel`, `Volume`/`SetVolume`, `PlayAnimationEffect` with `AnimAction::Stop`/`Release` |
| `FireSound` | fn_0072EDE0 per turn and slot: options InGame, sample 2, owner the fire, 3D, track 1, at `Get3DSoundPos` 0x72EE70; the .sad gives it loop −1 and mode 2 (FLAGS 0x7E0); fn_0072EDC0: if +0x38 & 0x20, `StopPlayingSoundEffect(fire, 2, InGame)` | `PlaySoundEffect(PlayOptions)` per turn; `StopSoundEffect(2, owner, InGame)`; `ToBeDeleted` empties the fire's first slot (and, in openblack, the other one if it had it) |
| `HandSystem::BeginApplyOnRelease` / `EndApplyOnRelease` | `SoundTag::Create(GInterface, 3, track 0, mode 2, −1, 0, 3D, IN_GAME, 0)` 0x5D275E; `SoundTag::Remove(this, 3, IN_GAME)` 0x5D27C8 | `tags::Create`/`tags::Remove` with the left hand's entity (**approximate**: its `Transform` instead of the newest point of the mouse buffer of `GInterface::Get3DSoundPos` 0x5CEC50); on release, the loop finishes its pass (`ToBeDeleted` 0x71ECB0) |
| `Gesture` (recognised) | fn_006882F0 (the record's state is `MyInterface`'s): 2D `PlaySoundEffect(0, 0x24, 3, 0, 0, 0, IN_GAME)`; another interface: 3D options, track 0, owner the atom's data, at the record's +0x3C | the same (`RecognisedGesture::fromInterface` / `handPosition`) |
| `Fireball::FlyBySound` | 0x683184: < 40 from the camera now and > 40 before (strict, 0x68322B), speed² > 400; `PlaySoundEffect(0, 0x40 + GetTickCount() % 5, 2, 0, 0, 0, IN_GAME)` | the same with `TickCount()` (before, a counter of its own and `>=` in the previous step) |
| `hand_fx::RemoveHandSpellVisuals` (was `DoRemoveFromHandVisual`) | options InGame, 0x77, owner the PHandFX, 2D (0x68CEE1) | `PlayOptions`, fixed `Owner::Object` |
| `hand_fx::AddSpellToHandVisuals` | `PlaySoundEffect(0, 0x23, 3, 0, 0, 0, IN_GAME)` 0x68DE7D | the same |
| `seed::SetPowerUp` (fn_00729C40) | PU 0/1/2 → `PlaySoundEffect(0, 10/11/12, 2, 0, 0, 0, SpellDialogue)` | the same; with the script widescreen it is removed by the userParam 1 filter, as in the original |
| `PlayFullyChargedSoundFX` | 0x77F4E0: table 0x77F5F8 by seed type (> 0x1D → 8), `PlaySoundEffect(0, voz, 2, 0, 0, 0, SpellDialogue)` | the same |
| `PlayTapSound` | fn_00726490: options InGame, 0x2A, no owner, 2D, pitch +0x48 from the table {100, 115, 130, 145, 155, 175, 190} with the index in 0..5 | the same; the mask +0x1C stays at 0 (the .sad of 42, FLAGS 0x402, does not have the pitch bit 0x1, so the options' pitch applies without a mask) |
| `one_off::InterfaceTap` | options InGame, 0x6D, owner the orb, 3D, track 0, at the +0xC8 of the interface state (0x72A6F4) | the same (**approximate**: the left hand's interaction point instead of +0xC8) |
| `teleport::MoveByTeleport` | `SoundTag::Create(MapCoords&, 0x27/0x26, track 0, mode 2, 0, 0, 3D, IN_GAME, 0)` 0x5EC358/0x5EC372: departure at the being's MapCoords, arrival at the argument's | `tags::CreateAtMapCoords` (before, a loose emitter with the arrival at height 0) |
| `FireGraphic` (steam) | fn_00731AB0 → fn_007314E0: options InGame, 0x35, owner the FireGraphic, 3D, track 0, at its +0x98 | the same (**inferred**: +0x98 = the position of the burning object; its writer has not been read) |
| "Audio Player" debug panel | — | "Sound" plays the sample in 2D through `PlaySoundEffect`; "Music" starts a MUSIC_TYPE like START_MUSIC (`ScriptStartMusic`) and stops it like STOP_MUSIC; the "Emitters" tab and its floating volumes disappear (the effects master is in "Channels") |

### Audible changes (because of the original)

- Everything from the miracles takes one of the 16 channels, with priority, stealing and QMixer's law; it passes GAudio's filters
  (script widescreen, SET_GAME_SOUND, citadel, interface states).
- The ParticleSound fade lowers the channel's 0..127 volume (before, a fraction of the emitter).
- The fire's crackle follows the object (track 1); when removed, the gesture loop finishes its pass instead of being cut.
- The recognised gesture of another interface, the steam and the teleport sound in 3D at their point.
- The passing fireball chooses its sample with `GetTickCount() % 5`.

### In game

Land 1: with a lightning bolt and a test fire, S_HandLightning_Final plays on a channel with `owner object 1` and
G_Fire_01 on its channel every turn without restarting (mode 2); the fireball plays S_Fireball_S_01 and
S_FireballHitSolid_L_01, and when the atom dies it is stopped and deleted (three fires take turns on the two slots); an
armed FIRE seed plays G_HandGesture_02 in 3D on the tag's channel while pressed and ends on release; the power-up band
G_SpellPowerUpBand plays in 2D (the PU 10 voice is filtered by the intro widescreen, as in the original); a tapped FIRE
icon plays G_ClickOnSpell_01 with pitch 100 and the voice HELP_TEXT_ANNOUNCER_VOICE_FIREBALL_01 when it charges.

### Audit: miracles

Reviewed against the disassembly: 0x726490 (the pitch table {0x64, 0x73, 0x82, 0x91, 0x9B, 0xAF,
0xBE} and the 0..5 clamp of 0x7264B7..0x726513, the pitch at +0x48 after the `push`), 0x6882F0 and 0x6885BB..0x68865B (2D with
mode 3 / 3D options with the atom's data as owner and track 0), 0x68CE90..0x68CEE1 and 0x68DE69..0x68DE7D, 0x72A660..0x72A6F4
(owner the orb, 3D, track 0, the point +0xC8, and the `ToBeDeleted` right after), 0x5EC340..0x5EC372 (the nine arguments
of `SoundTag::Create`), 0x72EDC0/0x72EDE0 (bit 0x20 before the trigger, track 1) and 0x72EC79..0x72ECE4 (only the first
slot, and the recalculated maximum), 0x5D274D..0x5D275E and 0x5D27C3..0x5D27C8, 0x6D1000 and 0x6D11A0..0x6D137A (the key
{+0x24, +0x28, 1, +0x20, +0x1C} in both places, the action `1 + SoftRelease`, the fade `max(+0x38 − FadeStep, 0)`,
the strict `< 1200²`), 0x674740..0x6747B4 (the distance/347 delay), 0x683184..0x68327F, 0x7314E0/0x731B0F,
0x77F4E0..0x77F5EE (`> 0x1D` → 8) and, in the DLL, 0x10014010 (the first channel of the bank and owner, +0x8C == 1, nothing with
+0x14 at 0). They all match. The `.sad` files confirm what the code claims: 42 has FLAGS 0x402 (without the pitch bit
0x1, so the mask +0x1C at 0 is correct) and voices 10/11/12 of SpellDialogue have userParam 1 (they are removed by the
widescreen, as the code says). There are no new ECS component dependencies in `src/Audio`, no
`CreateEmitter`/`PlayEmitter`/`AudioEmitter` outside, and each `RegisterObject` is undone: ParticleSound in `ProcessTurn`/`Clear`
(the `shared_ptr` stays alive in `g_Sounds` until the `UnregisterObject`), FireEffect in `sound::Free` (which `ToBeDeleted`
always calls, before `g_Pool` frees it) and in `sound::Clear` (before `g_Pool.clear()`); the new map goes through
`psys::manager::Clear` and `magic::OnLoadMap` → `ecs::fire::Clear` before `audio::ClearMap`.

Fixed in the audit:

- There were no tests. `test_sample_play` has three new ones (15 in total): `OwnerChannelAndTheFadeOfAPSysSound`
  (the first channel of the bank and owner, another bank or owner → none, the fade 127 → 97 → 0 with its gain, and when
  the wave ends there is no channel any more: the ParticleSound is deleted), `OwnerChannelNeedsActive` (0x10014018) and
  `TapSoundKeepsTheOptionsPitch`, which with the real 42 checks the **pitch 175 of placement 5** (what could not be seen in game) and that with the `.sad`'s bit 0x1 the bank's pitch would win.
- `ParticleSoundPosition` (was `PSysSoundPosition`) asks for `atom->drawn`, which 0x6D1000 does not look at: the comment now marks it as openblack's (the +0xF4
  is only written when drawing the atom).
- `Audio.h`: an `Owner::Object` that only plays channels without track does not need `RegisterObject` (PHandFX, steam, gesture).
- `ECS/Trees.cpp` (no owner): the growth of the watered tree drew the sample with `Locator::rng` although its comment
  cited `GetTickCount() % 9`; now it uses `audio::TickCount()` like 0x74C4B3..0x74C4C0.

In game (Land 1): with `OPENBLACK_TEST_FIRE` and the camera next to it, G_Fire_01 stays on **one** channel
(index 2) for 72 consecutive turns without restarting (mode 2) with `owner object 1`; the only handle change was at the beginning, with
the intro camera grazing the sample's maximum of 80: the fire takes one channel.

### (Approximate) and (inferred): miracles

- (Approximate) the gesture loop uses the left hand's entity instead of `GInterface`; the one-off orb sounds at
  the hand's interaction point instead of `GInterfaceStatus` +0xC8.
- (Inferred) `FireGraphic` +0x98 = position of the object.
- (Approximate, from the magic code, unchanged) `FireSound::Consider` does not stop the slot's sound when it is the same fire
  (fn_0072EDC0 at 0x72F82C); the owner's alignment in the PSysSound key (0x674678) is not filled in.

## Interface and hand in openblack

Read in the disassembly: `Abode::InterfaceTap` 0x406830..0x406966 (with `Abode::GetAbodeType` 0x4061F0 and `Abode::InterfaceValidToTap` 0x406820),
`fn_004082F0` / `fn_00408340` (the SetupBox click), `GGame::Loop` 0x54D009..0x54D078 (the Logo),
`fn_0x005e5cd0` 0x5E61A1..0x5E6230, `fn_00827820`, `fn_00827210`, `InfluenceCircle::Draw` 0x826C90 / `Reset` 0x826C50 /
`Add` 0x826FA0, `GGame::Update3DInfluence` 0x5552A0, `GInterface::StartGrab` 0x5D1740 and `SendTap` 0x5D38A0.

### What sounds now

| site | original | openblack |
|---|---|---|
| Click on a control of the game's own menus | `fn_004082F0` (from the SetupBox loop `fn_00408340`, codes 0xA/0xC at 0x408AA6, 0x408BBE, 0x408D6F): `G_MenuButton` InGame **159** 2D mode 3 + immersion 0x2C | `MenuClick()` in `src/Debug/Gui.cpp`: wraps each `MenuItem`, `Button`, `Checkbox` and `Selectable` of the menu bar (openblack's own dialogs). The immersion is not ported (there is no force feedback) |
| Knocking on the door | `Abode::InterfaceTap` 0x406830: only if `GetAbodeType() & 2` (LivingQuarters: houses A..F and mill), `G_KnockRoofMulti` **110 + [0xC4CC7C]** (0..8 rotating), 3D with track 0, owner the house, at the hand's point (status +0xC8) | `ecs::abodes::InterfaceTap` (`src/ECS/Abodes.{h,cpp}`), called from `HandSystem::Update` when the action button is pressed over a house: a house does not fit in the hand (`Object::ValidForPlaceInHand` 0x402870 = 0), so `StartGrab` taps it immediately, and the tap requires the hand inside the influence (`InterfaceMustBeInInfluenceForInteraction` 0x4028A0 = 1). It uses the counter `audio::Counter::KnockRoof` |
| Crossing an influence ring | `fn_0x005e5cd0` 0x5E61B0 per frame if not paused: `fn_00827820` checks the hand's point against the circles of `GGame::Update3DInfluence` (one per citadel and per town with influence) and, for each player whose "the hand is inside" changed, uses `fn_008277B0` to look for one of their circles whose edge the hand crossed between the previous point ([0xEA9EF0], which each call rewrites) and the current one; if there is one, it makes the ripple and sets [0xEB9A6C]; then `G_HandThroughInfluence_01` InGame **52** 3D (track 0, no owner) at the hand + immersion 6 | `influence::ProcessHandCrossing` (`src/ECS/Influence/InfluenceCircles.cpp`), called from `Game.cpp` after placing the hand if the game is not paused. The `.sad` gives it mode 1 (new channel per crossing), volume 40 and min/max 100/300: entering and leaving sound the same, and a circle that appears, grows or shrinks under a still hand does not sound (no edge is crossed). `influence::HandCrossedInfluence` is `fn_00827820` without the sound |
| Screams when picking up a villager | `GInterface::GenericPickup` 0x5D28C5..0x5D295D | already there (`HandHolding.cpp`): two tags, `G_PickUpObject` 10 and 180/187/194 + `GetRandomSample(7)` |

- G_HandThroughInfluence: `GGame::Update3DInfluence` makes the circles (on a turn that is a multiple of 10, with the
  dirty byte set) only while the player's border is shown; without it a crossing makes no ripple and no sound
  (0x8278E0..0x8278EC).

`Abode::GetAbodeType` 0x4061F0 reads the house's `GAbodeInfo` +0x120; openblack stores the `AbodeNumber` and the mesh, so
`abodes::TypeOf` looks up the record by those two (like `influence::AbodeInfoOf`) — **(inferred)**: all records
for the same house number carry the same LivingQuarters bit.

### (Approximate) and (inferred): interface and hand

- **(Approximate)** [0xEB9A1C] (that player's influence border has already appeared: a latch per map that is only
  set by `fn_00883120` when the fade of the border graphic reaches 1, 0x8831AD, and that is cleared by `fn_00828A50` from
  `LH3DIsland::Create` 0x803E85; `InfluenceCircle::Draw` only reads it, 0x826F15) is taken as set: openblack does not draw
  the influence border. Because of that, right after changing map, openblack may sound on a crossing that the original
  would keep silent until the border appears. The crossing ripple (fn_00827670 / fn_00827250) is not done either.
- **(Approximate)** with the hand off the land `Game.cpp` does not call `ProcessHandCrossing` (there is no point); the
  original keeps passing the last [0xE9A100].
- **(Approximate)** the hand's point for the crossing comes from `HandSystem::GetPlayerHandPositions()[0]`; the original uses
  [0xE9A100], which `GLandscape::Draw` 0x5E4395 fills from the hand of `MyInterface()`.
- **(Approximate)** the menu click: openblack has no SetupBox; its dialogs are the ImGui menu bar, and it sounds when a control says it has been pressed (like the original's code 0xA).
- **(Inferred)** all records of an `AbodeNumber` carry the same `ABODE_TYPE` (above).
- The "there is already state from the previous frame" latch ([0xEB9A68]) is process-static, as in the original (nothing
  clears it between maps).

### Audit: interface and hand

Checked in the disassembly: 0x406820 (`mov eax, 1`), 0x406830..0x40694A (Abode vt +0x8C4 = `GetAbodeType`
0x4061F0, `test al, 2`, 0x6E + [0xC4CC7C] wrapping to 0 at 9, +0x20 owner, +0x08 = 1, +0x0C = 0, point status
+0xC8), 0x4082F0 (0x429DA0 with owner 0, 0x9F, mode 3, loops 0, +0x10 0, 2D, bank +0x3AC; immersion 0x2C), 0x5E61A6
(pause), 0x5E61B0..0x5E621E (0x34 = 52, +0x20 = 0, 3D, track 0, sound only if [0xEB9A6C]), 0x827820, 0x8277B0,
0x827210 (strict), 0x5552A0, 0x5508A0 (stops at the type-3 player), 0x5D1740 (Abode vt +0x6FC = 0x402870,
+0x714 = 0x4028A0), 0x5D38A0 (+0x740 = 0x406820, packet 0x20), 0x828A50, 0x883120.

- **Fixed**: the crossing sounded every time a player's "inside" bit changed, also when a circle appeared or
  grew under a still hand; the original requires `fn_008277B0` (a circle of the player with the previous point [0xEA9EF0]
  and the current one on different sides of the border). Now that previous point is stored and the circle is searched for.
- **Fixed**: the comment and the wiki said that `InfluenceCircle::Draw` sets the latch [0xEB9A1C]; it only reads it.
- **Fixed**: the knock and crossing tests copied the call instead of exercising the code; now they call
  `ecs::abodes::InterfaceTap` (house and town centre, which stays silent) and `influence::ProcessHandCrossing` on the registry
  (entering, staying, leaving, growing circle, neutral town), and the ten-tap one does not depend on the starting value
  of the static counter.
- Unchanged: the menu click (asked for in the game's own menus), no new ECS dependency in
  `src/Audio`, no new AL source/buffer, no music thread. `audio::OnThingDeleted` is called by nobody (the channels follow `ownerPosition`, which with entt's versioned ids gives nullopt for a deleted entity).
- In game (`OPENBLACK_TEST_KNOCK="10,0,6,0.4"`, `OPENBLACK_SFX_TRACE=1`): the ten
  taps give 110..118 and 110, 3D track 0, owner the house (cut by the maximum 150 with the camera at 220).

### Tests and game

- `test_ui_sfx` (6 tests, 46 executables in total): the real user parameters from InGame.sad of the interface samples
  (0 in 42/134/159/160/173/205/40/41/119, 1 in 1/52/110..118, 2 in 46, 4 in 129) and the mode 1, volume 40 and
  min/max 100/300 of 52; **tapping a house ten times gives 110..118 and goes back to 110**, 3D at the hand's point with
  max 150; inside the citadel neither the `MenuButton` (parameter 0) nor the knock (1) sound but the woosh (2) does; 52
  takes a new channel per crossing (mode 1 from the `.sad`) and is cut beyond 300 from the camera.
- In game (Land 1, hook `OPENBLACK_TEST_KNOCK="10,0,6,0.4"` with
  `OPENBLACK_SFX_TRACE=1`): the ten taps give `InGame.sad/110, 111, 112, 113, 114, 115, 116, 117, 118, 110`, 3D with
  track 0, owner the house and at its point. During the **whole** Land 1 intro the script has the widescreen on (the
  narration continues at 75 s), so GAudio filters them by user parameter 1 — as the
  original would; the audible start is in the test.
- Picking up a villager **sounds twice** (`OPENBLACK_TEST_PICK_VILLAGER="0,6"` with
  `OPENBLACK_TEST_VIEW_VILLAGER="0,5,0"`): `InGame.sad/10 G_PickUpObject` → channel 113 and `InGame.sad/187 G_PickUpMan_01`
  → channel 132, the two 3D point tags on the villager (with the camera far away they are cut by their maximums 180 and 160).
- The influence crossing triggers by itself: two `InGame.sad/52` with **mode 1**, 3D, track 0 and no owner
  while the intro camera drags the hand across the edge of a town's circle (filtered by the
  widescreen, just like the original). The menu click cannot be tested without moving the mouse.

## Guidance and night voices in openblack

Read in the disassembly: the whole of SoundGuidance.cpp 0x71AA90..0x71D480, of SpookyVoices.cpp 0x72E130..0x72E8B0,
the callers (0x54E711..0x54E729, 0x5DC50D, 0x7506C0..0x7508F4,
0x4141A0, 0x406640..0x406786, 0x66F4D8..0x66F509, 0x63A9A0..0x63A9E6), HelpSystem::RunMessage 0x5C8CE0 /
StopHelpScriptsForNewHelp 0x5C8C40 / TriggerCategory 0x5C8280 / Reset 0x5C5580 / fn_005C6CF0, GScript::StartScript
0x6EB710, HelpSystemOn 0x6FBFD0, SetHelpSystem 0x6FC020, GRand 0x6DE570 / 0x6DE590, _LHRand 0x7DB600, the moon
phase fn_0086A7F0 and the tables 0x980128..0x9804D0, 0x999434.

### GGuidance (`src/Audio/Services/Guidance.{h,cpp}`, `audio::guidance`)

**Faithful.** A single GGuidance (the original has one per GInterfaceStatus, +0x30, and all callers use the one from
`MyInterfaceStatus`). What it reads from the game arrives through `GameQueries`; without a query, the neutral value of a
game without that system, and nothing sounds.

| function | original | what it does |
|---|---|---|
| `Init` | 0x71AC70 (GInterfaceStatus::Init 0x5DD1CB) | own options (Guidance bank, 2D, track 0, mode 2); lastPlayed[t] = turn − LocalRand(base) if it is > 0 (**with a second draw** for the value that is stored), otherwise 0; +0x98..+0xC8 = 0, +0xA8 = 30, the 7 "once" ones to 0, [0xC221CC] = 1. openblack calls it at the start of each land |
| `TimeSinceLastPlayed`, `Interval`, `PlayNow`, `CanPlaySpiritRemark` (was `HelpSpritesPlayNow`) | 0x71ADF0, 0x71AEE0, 0x71AF50, 0x71AFF0 | see above; `CanPlaySpiritRemark` = PlayNow(8) && PlayNow(t) (type 8 is never updated: it is a global gate) |
| `PlaySample` | 0x71C6F0 | **persistent** options: vol, pitch, +0x2C, sample (voice table 0x96BA38 if it is a text), owner = player no.; 3D: point, max, min = max·0.333 (0x8D8734), mask 0x180, track 0; **without a point it neither sounds nor marks**; 2D leaves the rest as the last 3D left it; lastPlayed = turn |
| `SpiritSay` (was `HelpSpiritSay`) | 0x71D270 | RunMessage(text, text, "MultiHelpJustTalkWithText" or "…NoText" for 31) + TriggerCategory(8); lastPlayed and +0x2C = turn **even if the script does not start** |
| `OneOff(k)` | fn_0071D0B0 | once per land, with probability (table 0x980440: 3308 0.02 · 3317 0.0002 · 3318 0.01 · 3321 0.01 · 3325 0.05 · 3326 0.1 · 3328 0.025) → SpiritSay(text, 32) |
| `GetRandomSample`, `RandomSampleForValue` (was `GetRandomSampleBasedOnValue`) | 0x71D300, 0x71D320 | HELP_SPRITES_GUIDANCE lists from info.dat (22 × 34; up to the first 0; 34 full ones count as 33) |
| `TimeSinceThingSeen`, `DesireSample`, `DesireScore` | fn_0071AE10, fn_0071AA90, fn_0071B410 | see below |
| `UpdateTownDesireRemarks` (was `ProcessTownDesireSFX`) | 0x71B020 (+0x71B130, +0x71B270) | every 10 turns: the town with a store and people nearest the camera (< 200) and its 17 desires {value +0x37C, type +0x380}; then the 6 worship sites of my citadel with worshippers (food, and +0x70 of the citadel) **override** the town if they score (the +0x70 one whenever it is not 0); value > 0.3 → x = v − rand(v/2), 3D at the town or the citadel, max 200·x; +0x98 = the sample |
| `UpdateHeartBeat` (was `ProcessHeartBeatSFX`), `HeartBeat`, `SetHeartBeatOverride`, `HeartBeatPulse`, `StopHeartBeat` | 0x71C190, 0x71C460, fn_0071C3F0, fn_0071C430/450, fn_0071C650 | every 10 turns v = Σ protection desire + ((+0xC8 + 0.001)/(q + 0.001) − 1) + ((+0xC4 + 0.001)/(p + 0.001) − 1) + Σ for each enemy creature whose nearest town is mine (d < 400) 1 − max(d − 100, 0)/400, clamped to 0..1; p, q smoothed 0.1; **every turn** (the `jne` at 0x71C1AD jumps to the call at 0x71C3C1) fn_0071C460(stored v): pitch, phase (+pitch·0.025·100 ms·0.001), pulse (1 − cos 2πφ)/2 |
| `RemarkOnMoonPhase` (was `HelpSpritesCheckMoonPhase`) | 0x71D1C0 (static) | countdown [0xC221D0]; at visual night, phase − π; at real night and \|·\| < 0.15 → OneOff(5) and 600000 turns; otherwise, ftol((phase − π)²·12000) |
| `MoonPhase` | fn_0086A7F0 | 2π(1 − frac(days·0.03386318)), days = time()/86400 − 10962 (integer) |
| `ProcessGameTurn` | GGame::ProcessTurn 0x54E711..0x54E729 | GSpookyVoices::Process, the moon, the desires and the heartbeat of GInterfaceStatus::Process 0x5DC50D (order **(approximate)**) |
| `PlayResourceDropRemark` (was `ResourceDropSFX`), `ResourceDropSample` | 0x71B570, 0x71B5F0 | the nearest town < 100; sum of its three values of the type ≥ 0.5 → PLEASED_x, < 0.25 → DISPLEASED_FOOD for food and **PLEASED** for wood and rain (the DISPLEASED ones for wood and rain are not used in W120); 3D, max 200 |
| `PlayTownAttackRemark` (was `TownAttackSFX`), `StrongestEffect`, `AttackerSample`, `WarnTownUnderAttack` (was `HelpSpritesTownBeingAttacked`) | 0x71B7C0, fn_0071BE40, 0x71BC20..0x71BD50, 0x71C870 | 10 ATTACK, + 10 FIRE if effect 0 dominates, + 10 times the attacker's sample (lightning / rock / creature); one at random; max 200·(min(+0xEC0·0.2, 1) + 1); and always the comment if the town is mine (PlayNow, not CanPlaySpiritRemark; aggression > 1) |
| `StartTotemRaiseSound`, `EndTotemRaiseSound` (were `StartRaiseTotemSFX`, `EndRaiseTotemSFX`) | 0x71BEB0, 0x71BED0 | the end plays nothing in W120 (it computes the alignment class and returns) |
| `PlayDiscipleRemark` (was `MakeDiscipleSFX`), `DiscipleText`, `AlignmentClass` | 0x71BF10, fn_0071AB70, fn_0071C690 | 2D, vol 85; table 0x98040C; disciple 10 says GOOD/EVIL_LIVE_HERE depending on the class (±0.55) |
| `PlayBeliefRemark`, `PlayBeliefSample` (were `BeliefSFX`, `BeliefSample`), `BeliefVisibility` | 0x437F40, fn_0071BF70, fn_0071C0D0 | only if the player's belief is below the highest; x = (b + 0.0001)/(max + 0.0001) − rand(x/3); visibility t0·(1 − d²) > 0.3 |
| `PlayVillageDeathRemark` (was `DeathInVillageSFX`) | fn_0071C810 | DEATH_IN_VILLAGE_06 + rand 5, 2D |
| the `Warn*` and `Remark*` functions (16, were `HelpSprites*`) | 0x71C930..0x71D070 | each with its condition (people, working store, < 300 from the hand GInterface+0x3B8, on screen) and its list (type − 9) |
| `UpdateAlignmentRemarks` (was `HelpSpritesAlignmentProcess`) | 0x71CEB0 | accumulated +0xC0 = 0.95·+0xC0 + change; past 2·the player's maximum change: same sign and \|a\| > 0.75 → good: type 29 list 20, evil: type 28 list 19 **(sic: the Enums.h names give them swapped)**; opposite sign and \|a\| > 0.4 → 25/16 or 26/17 |

**Randomness**: LocalRand(n) ∈ [0, n) (0 for 0, without a draw) and LocalFloatRand(x) = x·LocalRand(0xFFFF)·(1/65535) like the
original; the generator is `game_random`, openblack's single local stream **(approximate)**, except in the tests, which
use `SeededRandom` (was `LHRand`) with a seed.

**New queries** (`GameQueries.h`): `playgroundGame`, `multiplayerGame` (false), `helpLevel` (HelpSystem; 3 without it),
`localPlayerNumber` (PLAYER_ONE), `visualNight`, `handPosition` (the hand, **(inferred)** GInterface+0x3B8), `pointOnScreen`
(without it: false), `desireTowns`, `worshipSites`, `nearestTownAt` (assigned: `map_cells::GetNearestTown`), `townResourceNeeds`, `heartBeat` (without them: nothing; `heartBeat` is assigned, see [Chants and heartbeat](#chants-and-heartbeat-in-openblack)), `helpRunMessage`,
`helpTriggerCategory`, `profileName`.

**HelpSystem**: `+0x45F8` switch (Reset 0x5C55FC = 1; **SET_HELP_SYSTEM 253** done), `+0x45F4`
level (3 without a profile, 0x5C6DB6), **HELP_SYSTEM_ON 200** done (switch && level ≠ 0), `TriggerCategory` (+0x2D8, 9;
Reset sets them to 0), `+0x560`. `script_control::RunMessage` / `StopHelpScriptsForNewHelp`: it does not start if a task that is not
a help task (type without 0x42) has the dialogue; otherwise, it stops the help scripts (0x4A), pushes the two numbers as
float and starts the script with types 0x7F. `chlapi::ScriptVm` comes out in `CHLApi.h` with `pushFloat` and `startScript`.

**Connected in openblack**: `ProcessGameTurn` on the turn (Game.cpp, before `audio::ProcessTurn`); `Init` on each
`LoadMap`; the info.dat lists at startup; `PlayResourceDropRemark` in `pot_resource::AddResourceToPos` (new pile from the
local hand: RESOURCE_TYPE 1 → 2, 0 → 1) and in `HandSystem::DepositInStore` (with the point and the GetGuidanceResourceType of the **receiver**, the store: StoragePit inherits the 0 of GameThing 0x71BDD0, so the original runs PlayNow and GetNearestTown and says nothing). Today none of this
sounds in Land 1: the non-"always" types stay silent in the campaign's Land 1, the towns have no desires or resource
values (neutral queries). The heartbeat does sound (the heart is the temple).

### GSpookyVoices (`src/Audio/Services/SpookyVoices.{h,cpp}`, `audio::spooky`)

**Faithful** except for the name. Static object 0xDA0830: bank +0x8, options +0xC, sample +0x10, counter +0x14, count-
down +0x18. The info.dat info `GSpookyVoiceInfo` (5 entries, 0xDA0850) is read by nobody.

- **Soundex** (`SoundexDigit`, was `SoundExCode`, 0x72E4E0, jump table 0x72E54C): a e i o u 0, b f p v 1, c g j k q s x z 2, d t 3,
  l 4, m n 5, r 6 and **h w y the character itself** (entry 0x72E548 returns eax). Not a letter (`_isalpha`) = 0
  ((inferred): the "C" locale, ASCII only; "é", "ñ" give 0).
- `GetNextSoundexCode` 0x72E5C0: skips 0 codes until the end or a space; after a letter with a code, if the next one
  has the same, it **returns code + 1 without skipping it** (W120 quirk: "Curro" gives 7, 6, 0).
- `SoundsAlike` (was `PerformSoundexComparison`) 0x72E630: identical first character (case counts) and three equal codes;
  `SoundexOverlap` 0x72E6E0: any word of the name; `FindSoundAlikeName` (was `TrySoundex`) 0x72E7E0: the first of the 100 names (0x999434:
  4586..4685) that matches → the sample from the voice table 0x984D48 (another copy of 0x915D40). In the Spanish installation
  11 names resolve to an earlier one (Alfredo → Alberto, Manolo → Manuel, Jaime → Juan, María → Mario, Miriam →
  Mariano, Lucía and Luisa → Luis, Ángeles → Ángel, Julia → Julio, Rocío → Rosa, the second Alberto).
- `GetName` 0x72E740: profile name ([0xD4BF38] = PlayerProfile +0x200), then the network one ([0xD204D4]+0x70) and the
  `DefName` from the registry "Software\Microsoft\MS Setup (ACME)\User Info"; **openblack has no profiles**: `profileName`
  = `OPENBLACK_PLAYER_NAME` if set **(inferred)**; network and registry **not ported**. No name → sample 0 → it never sounds.
- `Init` 0x72E2A0 (GGame::InitOneTimeOnly, once), `UpdatePlayerName` 0x72E870 (GGame::Init, each land).
- `Process` 0x72E310: nothing on Lands 1 and 2 or without a sample; countdown of 100 calls; at night (fn_0072E3B0),
  r → ftol((1 − r³)·1000) < counter → `PlaySpookyVoice` (was `PlaySpooky`), otherwise OneOff(1); counter + 1. **Correction**: `PlaySpookyVoice` sets the
  counter to 0 (0x72E4D3), so after sounding it goes back to 1.
- `PlaySpookyVoice` 0x72E3F0: pitch 100·(1 + a³)^±1 with a = rand(0.65), +0x2C = rand(180), volume ·(1 + b³)^±1 with b = rand(0.8)
  **accumulated** (the options persist), 2D.

### Guidance and night voices in game

Land 1 at real night, `OPENBLACK_GUIDANCE_TRACE=1 OPENBLACK_PLAYER_NAME=Mario
OPENBLACK_TEST_GUIDANCE_SAY=80:3326` (and another run on turn 200):
`SpookyVoices: Init, name sample 95`, `Guidance: Init at turn 0`; on turn 80/200 `HelpSpiritSay(3326, type 32)
MultiHelpJustTalkWithText not started`: the land's script (task 19/22, type Script) has the dialogue and
`StopHelpScriptsForNewHelp` does not take it away, as in the original. Neither the desires nor the night voices sound in Land 1
(muted by type and by land). No new errors.

### (Approximate) and (inferred): Guidance and night voices

- **(approximate)**: x87 in double; the generator; the order of `ProcessGameTurn` relative
  to GInterfaceStatus::Process; the PlaySample points are world points (without the MapCoords rounding).
- The distances are `gutils::GetDistanceInMetres` (hypotenuse 0x74F680 with the 1/√ table of _FUN_0074f620
  on the points' 16.16 MapCoords) and [0xD01A38] is `game_clock::MsPerTurn()` (100, GGame::Init 0x54F4A5).
- **(inferred)**: GInterface+0x3B8 = the hand; ASCII `_isalpha`; the name
  via `OPENBLACK_PLAYER_NAME`; the local player = PLAYER_ONE.
- **Not modelled**: the options' +0x2C (90, rand(180) in the night voices): it is stored, SamplePlay does not use it.

### Audit: Guidance and night voices

Checked in the disassembly: the tables 0x980190 (33 rows), 0x980328, 0x98040C, 0x980440 and the constants
0x98011C..0x980188; Init 0x71AC70 (double draw), Interval 0x71AEE0, PlayNow 0x71AF50, PlaySample 0x71C6F0,
ProcessTownDesireSFX 0x71B020, CheckWorshipSiteDesiresSFX 0x71B300..0x71B407 (the need wins even if it scores less),
ResourceDropSFX 0x71B570 / 0x71B5F0, fn_0071BF70, fn_0071C810, MakeDiscipleSFX 0x71BF10, fn_0071C460, 0x71C190,
0x71C990..0x71CAE0, 0x71CEB0..0x71D063, HelpSpiritSay 0x71D270, GetRandomSample 0x71D300..0x71D3A4, the moon 0x71D1C0 /
0x86A7F0, RunMessage 0x5C8CE0 / 0x5C8C40, CHL 200 / 253 (0x6FBFD0 / 0x6FC020), and the whole of GSpookyVoices (0x72E280..0x72E87F,
the jump table 0x72E54C). Two corrections:

- **The heartbeat runs every turn**: in `ProcessHeartBeatSFX` the `jne` at 0x71C1AD (turn not a multiple of 10) jumps to
  0x71C3B8, which calls fn_0071C460 with the stored +0xA4; only the value is recalculated every 10 turns. The first port returned without
  beating, so the pitch was smoothed and the phase advanced 10 times more slowly. Test
  `HeartBeatRunsEveryTurnTheValueEveryTen`.
- **`DepositInStore` says nothing**: in Object::DoDeleteObjectAndTakeResource 0x63A940 `this` (edi) is the receiver and
  esi the object (GetPos 0x63AA2B and ToBeDeleted 0x63AAB1 go to esi); ResourceDropSFX receives edi+0x14 and the
  GetGuidanceResourceType of edi (vt +0xE0), which for StoragePit is GameThing's 0x71BDD0 = 0. The first port passed the tree's
  point and "wood" (PLEASED_WOOD invented as soon as there is `townResourceNeeds`); now the store's point and `None`:
  PlayNow and the town search run and nothing sounds, as in the original.

In game (Land 3 at real night, `OPENBLACK_PLAYER_NAME=Mario OPENBLACK_TEST_GUIDANCE_SAY=90:3326`): `SpookyVoices: Init, name sample 95`, `Guidance: Init at turn 0`, and on turn 90
`HelpSpiritSay(3326, type 32) ... not started` (in Land 3 too the intro script has the dialogue). No new
errors.

## A single engine

Outside `src/Audio` nobody uses `AudioManager` any more (it does not exist) nor
OpenAL (`grep` of `AudioManager|Locator::audio|AL/al.h|alGen|alSource` in `src`, `test` and `apps`: only `Device.cpp`).

**Removed**: `AudioManager.{h,cpp}`, `AudioManagerInterface.h`, `AudioManagerNoOp.h`, `AudioPlayer.{h,cpp}`,
`AudioPlayerInterface.h`, `AlCheck.{h,cpp}`, `SoundGroup.h` and `Locator::audio`. What they still did moves to:

| before | now |
|---|---|
| `AudioPlayer::Initialize` (device, context, OpenAL Soft logging, `AL_INVERSE_DISTANCE_CLAMPED`), `AudioManagerNoOp` if it fails | `audio::device::Open()` from `InitializeEngine` (`LH_AudioSystem::Create`); without a device, `NullSampleOutput` |
| `~AudioManager` (channel sources, buffers, context) | `audio::device::Close()` from `ShutDownServices`, after `audio::Shutdown` and `music::Shutdown` |
| `AudioManager::Update` (finite loops of the channels) | `sample_play::UpdateFrame()` at the start of `audio::UpdateFrame()` (same place in the frame) |
| `AudioManager::UpdateListener` → `AudioPlayer::UpdateListener` | `device::SetListener(camera, 0, forward, up)` in `sample_play::UpdateChannels` (LHListenerUpdate 0x10003850 from fn_004270D0 0x4271EF: QSWaveMixSetListenerPosition 0x1000398E / Orientation 0x100039A7; velocity 0 once, 0x10015C1A) |
| `AudioManager::GetSampleOutput` | `device::Output()` |
| `AudioManager::GetSound`, `CreateSoundGroup`/`AddToSoundGroup`/`GetSoundGroups` (bank list of the panel and of LHAtmos) | `banks::Count/Path/Samples(BankId)`, `BankGroup`, `FindBank` |
| `SoundExists` checking whether the output was `NullSampleOutput` | `device::IsOpen()` |

**Device** (`src/Audio/Device/Device.{h,cpp}`, `audio::device`, layer 0): `Open`, `Close`, `IsOpen`, `Output`,
`SetListener`, `ListenerPosition`; sources (`CreateSource`, `DeleteSource`, `SetSourceBuffer/Pitch/Gain/Looping/Relative/
Position/Distance/Rolloff`, `Play/Stop/PauseSource`, `SourceStatus`, `SourceSampleOffset`, `SourceSecondOffset`,
`SourceBuffersProcessed`, `QueueSourceBuffer`, `UnqueueSourceBuffer`); buffers (`CreateBuffer`, `SetBufferLoopPoints`
with `AL_SOFT_loop_points`, `DeleteBuffer(s)`). It is the only file that includes OpenAL and the only `alCheckCall`. The axis
change (x ↔ z, openblack's world left-handed, OpenAL right-handed) is done only here: before it was repeated in `AudioPlayer`,
`AlSampleOutput` and `MusicStream`. Users: `AlSampleOutput` (16 channels; voices and advisors go through them),
`WaveBuffers` and `MusicStream` (6 tracks).

**Banks** (`src/Audio/Game/Banks.{h,cpp}`, `audio::banks`, layer 1, LHBankRegister 0x10002240): the registry
(`RegisterBank`, `SetBankSampleCount`, `BankSampleCount`, `Bank(SfxBank)` = GAudio+0x3A8 + 4·type from 0x9CB3F8,
`FindBank`, `BankGroup`, `SampleId`, which were in `AudioSystem`) and the loading:
- `banks::LoadAll()`, at the end of `audio::Init` (GAudio ctor 0x426D40 → fn_00429CB0; (approximate) the 14 ambience ones also here, although the original registers them later, InitAtmos 0x428EF0 → fn_00428F30 from GGame::FinishInitialisation, with nothing playing in between): each .sad
  of `Audio\` in file system order, like the loop there was in `Game.cpp`. Same contents: anim-effect
  tables (`anim_effects::RegisterTables`, 0x10002778..0x100029AB), wave names for the voice table (banks 6, 7,
  10), empty samples skipped (`continue`), music banks (.mpg waves) out.
- The dialogue ones (types 6..10) remain **lazy**: only headers, and `banks::ReadWave` reads the wave on first use
  (0x10011420 → fn_100032D0; previously `wave_buffers::ReadWave`).
- `banks::MusicBankOf(MusicType)` (0x9C9748, GAudio+0x2C + 4·type) registers each music bank on first use (previously in
  `MusicStream.cpp`); `music::GetBank` tells the engine about it; `banks::ReleaseMusicBanks()` in `music::Shutdown`.
- `LHAtmos` (`AtmosBanks::Register`) looks up its 14 banks with `FindBank("/<file>.sad")` and reads their samples with
  `banks::Samples`.
- `AnimEffectTable::Load(path)` only reads the file for an unregistered bank (tools and tests).

**Debug panel** (`src/Debug/Audio.cpp`): the bank list comes from `audio::banks`; the "Channels" tab shows
the device (LHWaveIsInstalled), the 16 channels, the advisors (`advisor::Speaker/Sentence/IsTalking/SentenceTime`,
owner 0x270C) and the 6 LHMusic tracks (state +0x24, bank +0x58, chunk +0x48/+0x4C, volume +0x34 → +0x30).

**Verification** (Land 1, 1800 frames, the five traces `OPENBLACK_AUDIO/SFX/MUSIC/ANIM/TEXT_TRACE`, before and
after): same startup lines (Atmos 15
loops and 400 one-shots, voice table 6974/1922/1328/227, two empty samples, WELCOME_DANCE without a file), the same music
(`intro.sad`, MUSIC_TYPE_SCRIPT_INTRO) and the same event types; the differences are in numbers (anim-effect
draws and a few fewer turns in the second pass, which depends on real time). No OpenAL errors.

## The structure: layers, one generator, one clock

Thin wrappers without the ECS, a single DLL generator and a single clock, `src/Audio` in layers. The
architecture and the file map are [above](#openblack-audio-architecture).

**1. No ECS in `src/Audio`.** `grep '#include "ECS/' src/Audio` gives nothing. The public signatures do not change
(`AnimationSounds::Fire/PlayFromTable/Update`, `lantern_sounds::SetOn/ProcessTurn/Clear`, `spell_sounds::*`); what
they read from the registry arrives through new `GameQueries` queries, registered by `src/ECS/AudioQueries.cpp`:

| wrapper | read | now |
|---|---|---|
| `AnimationSounds::Fire` (fn_00516510) | `Transform`, `Villager` (life, stage, sex, house), `LivingAction`, `sea_cells` | `animatedThing(entity)` and `audio::SurfaceType` (`surfaceType`) |
| `AnimationSounds` (loading of SmallSounds.SAS, LoadAllAnimations 0x550180) | the clips from the resources (`ecs::ClipId`) | `animationClipName(index)` |
| `lantern_sounds::ProcessTurn` | `StreetLantern`, `Transform`, `Rocks::Height` | `streetLanterns()` (same registry order) |
| `SoundMap` (CameraWeather, Dump) | `weather::atmos`, the hand (`HandSystem` + `Transform`), `sea_cells` | `weatherSmooth(point)`, `handPosition()` (it was already there), `SurfaceType` |
| `SpellSounds` (USESURFACE 0x674661) | `ecs::sea_cells::GetSurfaceType` | `audio::SurfaceType` |

`audio::GetSurfaceType` (SoundMap.h) no longer exists: it duplicated `ecs::sea_cells::GetSurfaceType`. Its only outside
caller, `Particles/Rules/Fireball.cpp` (fn_006A1F90), now calls `ecs::sea_cells::GetSurfaceType`. `CameraWeatherInfo`
moves from `SoundMap.h` to `GameQueries.h`. The hooks that move the camera (`OPENBLACK_AUDIO_TEST_VIEW` / `_ANIM` /
`_LANTERN`) move out of `src/Audio` to `ecs::audio_queries::RunTestHooks(turn)`, which `Game.cpp` calls after
`audio::ProcessTurn` (previously `AnimationSounds::RunTestHooks`; the lantern one counts its calls as it used to count those
of `lantern_sounds::ProcessTurn`).

**2. One DLL generator and one clock.** `LHaudiodllR.dll` has its own CRT linked in: `rand` 0x1001E7EB (seed ·
0x343FD + 0x269EC3, `(seed >> 16) & 0x7FFF`, per-thread seed in `_getptd` 0x100206F1 +0x14) and `srand` 0x1001E7DE,
separate from runblack.exe's. It is used, on the game thread, by: the pitch of LHSamplePlay (0x100127DF, directly), LHAtmos
(0x100016F4, 0x100017DC, 0x10001C2A, 0x10001C5A, 0x10001E74, directly) and `LH_AudioSystem::Rand()` 0x10015740 (the anim-effect
lists, 0x100146CF / 0x100147D8 via `Rand(n)` 0x10015710). Before there were three generators (`SamplePlay`,
`AtmosBanks` and `tags::RandomSample`, all three on top of `Locator::rng`); now:
- `sample_play::Rand()` is that `rand` (or `Backend::rand` in the tests), `SeedRand()` its `srand(time(0))`: on the first
  LHSamplePlay (0x10011497, flag [0x1005645C]), on the first `LH_AudioSystem::Rand()` (0x10015749, [0x10056464]) and when
  registering each bank with ambience records (LHBankRegister 0x10002765 -> fn_10001610 0x10001635). The power-on
  fn_10001840 0x10001843 seeds once at DLL startup (0x10015BDD, with +0x90) and the bank seedings
  overwrite it before LHAtmos draws: it is not emulated. `AtmosBanks` uses it.
- `sample_play::AlternatingRandom()` (was `AudioSystemRand`) = `LH_AudioSystem::Rand()`: `rand() / 2`, plus 0x3FFF every other time (the flag
  [0x1003C124] starts at 1 and toggles on each call, 0x10015768..0x10015783). **Audible change, because of the original**:
  the draws from an anim-effect list alternate between its upper half and its lower half (footsteps, saw, leaf
  rustle...). `Random(n)` = `AlternatingRandom() · n / 32767` (0x10015717..0x1001572B) can no longer give n (the maximum is
  0x7FFE): the (approximate) of the upper bound is removed.
- `tags::RandomSample` (GRand::LocalRand 0x71ED40) uses `guidance::LocalRand`, the only GRand::LocalRand in `src/Audio`;
  `guidance::LocalRand` / `LocalFloatRand` are `game_random::LocalRand` / `LocalFloatRand` (the single local stream,
  0x6DE570 / 0x6DE590).
- `audio::TickCount()` (Audio.h) and the music thread (`MusicStream`, which had its own copy) read
  `device::TickCount()`, which reads `game_clock::TickCount()` (the GetTickCount of all of openblack, `src/GameClock.h`).

**3. Layers.** `git mv` of the 60 files to `Device/`, `LH/`, `GAudio/` and `Services/`; `Audio.h` and `GameQueries.h`
stay at the root (the path and API of `Audio.h` do not change). The
`#include`s of the whole repository (src, test) use the new path (`"Audio/LH/SamplePlay.h"`); CMake does not change
(`file(GLOB_RECURSE ... *.cpp / *.h)` in `src/CMakeLists.txt` picks up the subfolders; a reconfigure is needed). Later renamed: `GAudio/` → `Game/` and `LH/` → `Engine/`.
`Banks` moves to `GAudio/` (it is the registry that GAudio does, fn_00429CB0, with the tables 0x9CB3F8 / 0x9C9748).

**Verification**: build and the 57 tests. Land 1, 1800 and 5000 frames with the audio, ambience, lantern,
tag and anim-effect traces, and `OPENBLACK_AUDIO_TEST_VIEW="90,0"` + `OPENBLACK_AUDIO_TEST_ANIM=354` (saw), before and after: same startup lines (115 clips with
sound, 201 + 3 rows, 15 loops and 400 ambience one-shots, 12 lanterns with their tag and their height), the same
GSoundMap dump (surface of the camera and of the hand), the camera hook on turn 90 and the same anim-effect
keys (animal footsteps group 18/32/36, villagers group 1 with voices 1/2/3 and the saw 30/31, tree
leaves); the distribution of the samples in each list changes because of the alternation in `LH_AudioSystem::Rand`. No new
errors.

**Audit.** checked in the disassembly 0x10015710, 0x10015740 ([0x10056464],
[0x1003C124]), 0x1001E7EB / 0x1001E7DE, 0x10011483..0x100114A6 ([0x1005645C]), 0x100127DF (direct rand, inside the
"NONE" name branch, which in openblack is always taken), 0x10001635 / 0x10002765, 0x10001843 / 0x10015BDD, 0x100146CF /
0x100147D8 (with 1 sample Rand is not called: the flag does not change, the same in openblack), 0x71D950 (6 outside the map),
0x71ED40 / 0x6DE570, 0x5165BC, 0x5166B1 / 0x5166CC, 0x51675D and 0x73494E. Changes: the comment on the power-on
fn_10001840 (it is from DLL startup, not "the same second"); `streetLanterns` uses `ecs::object::GetHeight` (0x638120)
instead of `Rocks::Height`, which only forwarded it; new test `DllRand.CrtSequenceAndAlternation` (the CRT sequence
from seed 1, the alternation of 0x10015740 and that `Random(n)` never gives n). In game:
115 clips, 201 + 3 rows, 15 loops / 400 one-shots, the saw with voices 1/2/3 in the view hook, 12 lanterns
with the same heights (4 x 1.29 and 8 x 4.95) and the lantern hook.

## The shared APIs

`src/Audio` uses the shared MapCoords conversions, GUtils distances, turn clock and sky type instead of copies of its own: `map_coords` (`src/3D/MapCoords.h`), `gutils`
(`src/Common/GUtilsDistance.h`), `game_clock` (`src/GameClock.h`), `sky_type` (`src/3D/SkyType.h`) and `ecs::object`
(`src/ECS/ObjectMetrics.h`).

| site | before | now |
|---|---|---|
| `SoundMap` CalculateRadiusPointAndDistance 0x71D834..0x71D855 | `ToMapCoord` (x · 6553.6, its own) | `map_coords::ToFixed` |
| `SoundMap` AtmosMapTypeInfo::Add 0x71D514..0x71D5A4 | `float(x) · 10 / 65536` (two roundings above 2^24) | `map_coords::ToMetres` (fild; fmul 10; fmul 2^-16: one rounding) |
| `SoundMap` UpdateFromMap 0x71D76B / 0x71D790 | `r / 10 · 65536` and `>> 16` | `gutils::ConvertMetersToWholeDistance` (fn_0074DC80, twin of 0x74DCE0) and `map_coords::SignedCellOf` (movsx) |
| `SoundMap` Dump 0x71D990 | `(x >> 16) & 0xFFFF` | `map_coords::CellOf` |
| `ThingMusic` SetPlayPosition 0x4298D9 | `MapCoordsRoundTrip` (in double) | `map_coords::Quantise` (product in float, like the 24-bit FPU of fn_007DEE00); `MapCoordsRoundTrip` is removed |
| `tags::CreateAtMapCoords` 0x71EB60 | only (x, z, height) in metres | additionally `CreateAtMapCoords(const map_coords::MapCoords&, ...)`: x, z via `ToMetres` (0x71EB8A / 0x71EBA6); the metres one remains for points that are already in metres from a MapCoords (`magic::ToMap`), without quantising again |
| `Guidance` Distance (GetDistanceInMetres 0x74CD70 / GetInfo 0x74CD50 / fn_00605CD0) | `std::hypot` of the points **(approximate)** | `gutils::GetDistanceInMetres` (16.16 MapCoords and the 1/√ table): 10 m gives 9.9975 m |
| `SoundTags::CheckDelay` 0x71E766..0x71E79B, `Guidance` heartbeat 0x71C4C7 | `k_MsPerTurn = 100` **(inferred)** | `game_clock::MsPerTurn()` ([0xD01A38], GGame::Init 0x54F4A5) |
| `audio::ProcessTurn` | `(skyType, turn)` from `Game.cpp` (`DayNightClock::SkyType()` of the turn) | `ProcessTurn()`: the turn is `game_clock::Turn()` (g_game+0x205A40, 0x54E997) and the sky type `sky_type::Frame()` ([0xFA26BC]: CalculateVolumes reads it at 0x71DDF1 and only DrawSky writes it with fn_0086A2C0, so it is the one from the last drawn frame) |
| `lantern_sounds` (query `streetLanterns`) | — | it already uses `ecs::object::GetHeight` 0x638120: nothing to change |

The real clocks in `src/Audio` were already one (`device::TickCount` = `game_clock::TickCount`); the music
thread's `steady_clock` is its wait (Sleep of 120 ms) and that of the `OPENBLACK_MUSIC_TEST` hook. `GameMusic` has no conversions
of its own: the distance in `ThingMusicInRange` (0x429479..0x4294C1) is 3D on LHPoint, not GUtils,
and `nearestTown` / `town` use the camera's MapCoords and `gutils::GetDistanceInMetres`. The position of a thing (`thingPosition`, `Game.cpp`) is still its float point,
without the rounding of its MapCoords **(approximate)**: it is the owner of all 3D channels and it is not changed here.

**Fidelity change (with its address).** The cell that AddAtmosType compares is its **centre**, not its corner:
fn_00601F40 (UpdateFromMap 0x71D7C2) puts the cell in the high words and in the low ones (GMap+0x28 >> 1) · GMap+0x2C
(g_game+0x59E0 / +0x59E4 / +0x59E8; GMap::Init 0x6014C0, at g_game+0x59B8, sets +0x28 = 8 and +0x2C = +0x30 = 0x2000;
no other code reads or writes them through g_game) = 0x8000, half a cell. The distance to the nearest type and its point
(`nearestX/Z`, the one HeightFade looks at) are those of the centre (x · 10 + 5). In Land 1 the dump does not change (JUNGLE 0.950 with
120 cells: the camera is less than 20 m from the jungle).

**The alignment calls the advisors.** `alignment::ProcessForPlayer` (`src/ECS/Effects/Alignment.cpp`)
does what GAlignment::ProcessForPlayer 0x4141A0 does: for the MyInterfaceStatus player (IsMemberOfThisPlayer 0x64D750;
PLAYER_ONE **(inferred)**, like `localPlayerNumber`), every turn and before Process 0x414140, also with nothing pending,
`audio::guidance::UpdateAlignmentRemarks(maximum · pending, alignment, maximum)` (0x4141CD..0x4141D9: vt +0x40 ×
+0xC, unclamped; the alignment from before the change, GetAlignmentValue 0x64D6A0; the maximum, GPlayer+0x64 +0x10). In
Land 1 the advisors are muted by land (PlayNow 0x71AF6F), so nothing new sounds.

**Verification**: build and the 57 tests (new `ThingMusic.PlayPositionQuantised` in `test_game_music` and
`SoundTagTest.MapCoordsTagIsTheMapPoint` in `test_sound_tags`; `GuidanceTest.TownDesireEveryTenTurns` now expects the
GUtils distance). Land 1, 1800 and 5000 frames (`OPENBLACK_ATMOS_TRACE=50`, lanterns, guidance, alignment, tags and
`OPENBLACK_AUDIO_TEST_VIEW="90,0"`, before and after): the same
GSoundMap dump (cells 159/224, 158/222 and 157/221, GRAVEL, 121 cells, JUNGLE 0.950, NIGHT 0.050), the 12 lanterns with their
heights, the view hook on turn 90 and the same startup errors. No new errors.

**Audit.** Checked in the disassembly: UpdateFromMap 0x71D76B (fn_0074DC80: fdiv 10; fmul 65536;
__ftol), 0x71D77A / 0x71D784 (fn_00605490 / fn_00605400 subtract / add r to x and z), 0x71D790 (movsx) and 0x71D7C2;
fn_00601F40 (low words (g_game+0x59E0 >> 1) · g_game+0x59E4 / +0x59E8); GMap::Init 0x6014C0 (+0x28 = 8, +0x2C =
+0x30 = 0x2000), called from GGame::Init 0x54F650 with `lea ecx, [ebx + 0x59B8]`; neither the 7 methods of GMap nor any
other code write those words, and only fn_00601F40 reads them through g_game. AtmosMapTypeInfo::Add 0x71D514..0x71D5A4
(receiver − cell, strict `fcom`, `__ftol` of the cell's metres to +0x8 / +0xA); CalculateRadiusPointAndDistance
0x71D834..0x71D855; CalculateVolumes 0x71DDF1 (`fld [0xFA26BC]`); CheckDelay 0x71E766 and the heartbeat 0x71C4C7 (`fimul`
of [0xD01A38], which GGame::Init 0x54F4A5 sets to 0x64); EndTurn 0x54E997 (`cmp [+0x205A40], 5; jbe`); ProcessForPlayer
0x4141A0..0x4141E1; HelpSpritesAlignmentProcess 0x71CEDA; GPlayer::GetMaxAlignmentChangePerGameTurn 0x64B670 = `mov
eax, [ecx+0x64]; fld [eax+0x10]` (so the maximum that the guidance reads is the same as that of vt +0x40: it is no longer (inferred));
GetInfo 0x74CD50 / 0x74CD70 / fn_00605CD0 (used by the towns' desire 0x71B19D and 0x71B2D8 after MapCoords(LHPoint)
of the camera 0x71B14A / 0x71B289); SoundTag::Create(MapCoords) 0x71EB71..0x71EBB2; SetPlayPosition 0x4298D9. Everything
adds up. Fixed: the CheckDelay comment still said "100 ms (inferred)". Added the trace line
`(openblack) Sound map nearest:` (with `OPENBLACK_ATMOS_TRACE`): distance and point of the nearest cell of each type
present. In Land 1 (with `OPENBLACK_AUDIO_TEST_VIEW="90,0"` and
`"90,0,80"`) the points end in 5 (JUNGLE 3.905 @ (1585, 2225), COUNTRYSIDE 74.224 @ (1635, 2175)): they are cell
centres; the volumes and the startup errors do not change.

## Owners, the 24-bit FPU and the double constants

### Owner helpers (`Audio.h`)

| function | what it does |
|---|---|
| `StopOwner(Owner)` | all of the owner's channels in **any** bank: `LHSampleStop(bank, owner, 0)` 0x10012C50 (sample 0 = any, 0x10012C76; `sample_play::StopOwner`) for each registered bank 1..`banks::Count()`, each channel with QMixer's 20 ms ramp. With the audio off it stops nothing (each LHSampleStop ends at the first channel, 0x10012CA8). The original does not have this loop: its callers stop bank by bank. |
| `NewOwner()` | an owner of its own: `Owner::Object(NewObjectId())`, never equal to a game owner (things are `Owner::Thing`; the CHL numbers and the voices 0x270C..0x270F, `Owner::Key`; the other objects take their id from the same counter). A 2D or 3D sound without tracking (`PlayOptions::track` = 0, or `PlaySoundEffectAt` 0x42A100 with track 0) needs nothing more; a 3D one with tracking (`PlaySoundEffectAt` 0x42A040 tracks all 3D, +0x0C = is3D) follows `RegisterObject(owner.id, position)` and is stopped on the next turn (owner gone, LHSampleStop 0x1001439D) if nothing is registered or it returns nullopt. When done: `StopOwner` and `UnregisterObject(owner.id)`. |
| `FindSample(bank, "name.wav")` | (openblack) the bank's sample whose .sad name (+0x00, only the file name) is that one, case-insensitive; its number is the +0x104. The original only uses numbers; it is for the tools. No unit test (it needs the banks registered) |

A caller looks up its bank with `FindBank(path)` (or registers its own with `RegisterBank`) and plays with
`PlaySoundEffect` / `PlaySoundEffectAt` and that owner.

### The game's FPU runs at 24 bits

`fn_007DEE00` (`fninit`, `and cw, 0xFCFF` at 0x7DEE0D; from `GGame::InitOneTimeOnly`, `EndTurn` 0x54E964 and
`Process3dEngine` 0x54E426) leaves the precision control at 00: **every `fadd`/`fsub`/`fmul`/`fdiv`/`fsqrt` on the game
thread rounds to a float's mantissa**, also inside LHaudiodllR and QMixer when the game calls them
(LHSamplePlay, LHaudio's conversion to polar 0x100122BC). **But not on QMixer's pump thread** (audit):
QSWaveMixSetPolarPosition 0x180040B0 only stores the polar point; the conversion to Cartesian 0x1800AA70 is done by
QSWaveMixPump 0x18003900 (…→0x180084A0→0x1800AA70), and QSWaveMixPump is called every 20 ms by LHaudio's `timeSetEvent`
(0x10015800) on winmm's timer thread, with the control word Win32 starts threads with (0x27F,
53 bits) **(inferred: nothing on that thread changes it)**; the only pass on the game thread (QSWaveMixPlay) is redone by the
next pump. What does not round: loading a double constant (`fld`/`fmul qword` uses it
whole), `fcomp qword` (compares with the exact double) and `fsin`/`fcos`/`fpatan` (full precision). The rule when
porting:

- comparison with a qword constant: in double (`static_cast<double>(x) > -0.6`);
- arithmetic with a qword constant: the step in double and the result to float (`static_cast<float>(double(a) * c)`);
- float arithmetic: in float, step by step, in the order of the x87 stack.

The DLL's music thread (`_lhbeginthread`, `MusicEngine.cpp`) does not go through fn_007DEE00: it starts with the system's
control word (53 bits); `MusicEngine` stays in double **(inferred)**.

### Audit of the double constants

Sweep of all FPU instructions with a `qword ptr [constante]` operand in
runblack.exe, LHaudiodllR.dll and QMixer.dll, cross-referenced with the addresses cited in `src/Audio`, `CollisionSounds`,
`FireSound`, `Particles/Rules/Sound`, `SoundAction` and `AudioQueries`:

| address | previous reading | real double | site | change |
|---|---|---|---|---|
| 0x8CF7D8 / 0x9375E8 (0x69EEC4 / 0x69EEDC) | 0.6f / 0.3f in float | 0.59999999999999998 / 0.29999999999999999 | `SpellSounds.cpp` `SizeFromThrow` | comparison in double: 0.6f and 0.3f end up **above** (size 1 and 2, before 2 and 3) |
| 0x8C4A08 (0x429000) | −0.6f | −0.59999999999999998 | `AtmosBanks.cpp` ProcessBanks | comparison in double (with floats it gives the same: no float falls between −0.6f and −0.6) |
| 0x10030450 (0x10012363 … 0x10012510) | 0.318471 (6 digits of `%g`) | 0.31847133757961782 (1 / 3.14) | `QMixerLaws.cpp` `PolarRelative` | the whole value, `atan · 180 · c` in that order, angles and distance in float, steps at 24 bits |
| 0x18037658 · 0x18036550 (QMixer 0x1800AA85) | π / 180 in double | π (double) · 0.0055555557f, in double (53 bits: the pump thread) | `PolarRelative` | k and el = k · elevation in double; az = k · azimuth stored in float; flat, up, right, ahead stored in float (the audit corrected an earlier 24-bit model) |
| 0x980520 / 0x980518 (0x71DEE1 / 0x71DEE7) | 15f and 1 / 30f in float | 15 and 0.033333333333333333 | `SoundMap.cpp` wind | `fild` of the integer sum, `fsqrt`, `− 15`, `· (1/30 double)`, each step to float (emulated: 0 differences in the 5924 values; the old formula differed in 3756) |
| 0x9A3BE8 / 0x8D45D8 / 0x8AB680 (fn_0086A7F0) | already in double, but everything in double | 0.03386318012808897 / 6.2831854820251465 / 1 | `Guidance.cpp` `MoonPhase` | each step to float (emulated: the same on the 8 days tested; the double model gave 3.3044245 instead of 3.3044248 on day 10976) |
| 0x8C49F8, 0x8AB260, 0x8C49F0, 0x8AB680, 0x8C2C48 (four1 0x428D50) | already in double, everything in double | 6.2831853071795898, 0.5, −2, 1, 0 | `Advisor.cpp` `FastFourierTransform` | steps to float; sin(θ) unrounded (fsin) |
| 0x915438 (0x5C36E5) | double | 0.94999998807907104 (= 0.95f) | `Advisor.cpp` `Say` | in float (the 24-bit subtraction is the float one) |
| 0x915440 (0x5C3822) | double | 0.90000000000000002 | `Advisor.cpp` (advisor text, 0x5C381D) | it already compared in double: no change |
| 0x9804D0 (0x71D210) | double | 0.15000000596046448 | `Guidance.cpp` RemarkOnMoonPhase | it already compared in double: no change |
| 0x8CF2B8 (fn_0071C400) | double | 0.40000000000000002 | `Guidance.cpp` HeartBeat | already `float(double(o · 100) · 0.4)`: no change |
| 0x8C49E0 (fn_00427200 0x427227..0x427259), 0x10030468 (LHSamplePlay 0x100114D1 …) | — | 5000 (exact) | — | no change of value; see pending |

And, by the same 24-bit rule, in float what was modelled in double without qword constants: `Guidance`
(`Cube`, `LocalFloatRand`, `Interval`, the random list, `DesireSample`, the desire value, `UpdateHeartBeat`,
`HeartBeat`, `PlayBeliefSample`, `BeliefVisibility`, `UpdateAlignmentRemarks`, the moon calculation), `SpookyVoices`
(the probability 0x72E34D, pitch and volume 0x72E3F6..0x72E4CD), `GameMusic::DiscreteAlignment` 0x414730 and `Advisor`
(`Analyse`, `BandLevel`, `ComputeLipSyncKey`, `Amplitude`). Checked without change: `SpellSounds` `SizeFromRadius` (0x69F4CA /
0x69F4F6) and `SizeFromImpactSpeed` (fn_006A1630 0x6A16A9 / 0x6A16CF) compare floats with strict `<` (the
**(inferred)** removed). Without qword constants: `AnimationSounds`, `LanternSounds`, `CollisionSounds`, `FireSound`,
`Particles/Rules/Sound`, `SoundAction`, `AudioQueries`. Outside the audio (untouched): the 1.5 qword of
fn_005E5830 0x5E5A6C (night lights) and `ReadSpeedFactor` fn_005C6CB0 (`HelpSystem.cpp`, in double; at 24 bits it would be
float step by step).

The values were checked against Unicorn emulations of the original code (control word 0x7F or Unicorn's 0, both at
24 bits; the polar position, the moon fn_0086A7F0 and the wind), and QMixer also with 0x7F, 0x27F and 0x37F (Unicorn
respects the precision control: 0x7F gives other digits); with 0x27F the double model of `PolarRelative` gives the 9
points identical.

### Tests and game

- `test_audio_laws` `RelativeAxesExact`: 9 points against the emulation (QMixer at 53 bits), to 2·10⁻⁷ relative + 10⁻⁶ (the 24-bit model of QMixer fails by 9·10⁻⁵ at (−300.5; 210.25; −15.5)).
- `test_spell_sounds` `sizeClasses`: 0.6f → 1, 0.3f → 2, the float below → 2 / 3.
- `test_guidance` `PhaseFromTheRealClock`: six days against the emulation, exact.
- `test_sound_tags` `StopOwnerInEveryBankAndModOwners`: two banks, two owners from `NewOwner`; `StopOwner` stops only the
  owner's, in both banks; a 3D one with tracking follows `RegisterObject` and the one without a registration is stopped.
- 64/64 tests. Land 1 until frame 5900 and with `OPENBLACK_ATMOS_TRACE=20`: the same startup errors as before (36);
  ambience and bells as before.

### (Approximate) and (inferred): FPU

- **(approximate)** the step in double and then to float can round twice on a tie (float · double constant);
  `sin`/`cos`/`atan` are the library's, not the x87's 64-bit ones.
- **(inferred)** the FPU of the DLL's music thread at 53 bits (it does not go through fn_007DEE00), and that of winmm's
  timer thread that pumps QMixer, also at 53 bits.
- fn_00427200 0x427227..0x42726D sets to 0 each coordinate of the channel's point (+0x50/54/58) whose absolute value
  exceeds 5000 before asking for the owner's position (ported as `audio::GuardSoundPoint`, see
  [the citadel interior](#the-citadel-interior-in-openblack)). LHSamplePlay compares |x| with 5000 (0x100114D1 …) for a
  log warning.

### Audit: FPU and constants

Checked in the disassembly: fn_007DEE00 (`and cw, 0xFCFF` 0x7DEE0D; fn_007DEE20 then calls `_controlfp` with the
mask 0x8001F, which does not touch the precision; callers EndTurn 0x54E964/74/84, Process3dEngine 0x54E426/0x54E4D1,
InitOneTimeOnly, RenderLoadingFrame, GAudio 0x426F66 / 0x427061), 0x69EEC4 / 0x69EEDC (`test ah, 0x41`: strict `>`),
0x429000, 0x71DEE1 / 0x71DEE7 (int8 squared, `fild`, `fsqrt`), 0x86A86B..0x86A888, 0x5C36E5, 0x69F4CA / 0x69F4F6,
0x6A16A9 / 0x6A16CF, four1 0x428DCA..0x428ED4 (stack order the same as the code), HeartBeat 0x71C491..0x71C546,
CalcKey 0x4289C2..0x4289F0, LHaudio 0x100122BC..0x10012522 (order of `fadd`, `__ftol` and `fdivr` the same as the code),
LHaudio's five double constants (0x10030440..0x10030460), LHSampleStop 0x10012C50 / 0x10012C76 / 0x10012CA8 and
0x1001439D, QMixer 0x18037658 / 0x18036550 and 0x1800AA85..0x1800AAFC. Everything adds up except:

- **Fixed**: QMixer's half of `PolarRelative` does not run on the game thread but on the pump thread (see above),
  at 53 bits: `k` and the elevation in double, az / flat / up / right / ahead stored in float; `RelativeAxesExact` with the
  values from the 53-bit emulation (0x27F). The difference is a few float ulps (inaudible), but now it is the original's.
- Unchanged: `StopOwner` / `NewOwner` (there are no callers in the game; the `Owner::Object` owners of the rest all come
  from `NewObjectId`, so they do not clash), no new ECS dependencies in `src/Audio`, no new AL sources/buffers, the music
  thread untouched, other owners' public signatures the same; the new tests check emulated values.

## What sounds and when

Each topic page says what sounds and when. Here there is only the engine:
- Picking up and dropping, piles, pots: [objects-and-resources.md](objects-and-resources.md) ("Sounds").
- Sounds of the animation clips and the banter: [animation.md](animation.md#clip-sounds).
- Hits, collisions and throws: [physics.md](physics.md#sounds-dust-and-the-look-of-impacts).
- Street lanterns (SoundTag, at night): [day-night-weather.md](day-night-weather.md) ("Night lights").
- Trees: leaves in [trees.md](trees.md#drawing), falling in [trees.md](trees.md#dropping-and-replanting).
- Miracle particles (SOUND_ACTION, PSysSound, spells.sad): [particles.md](particles.md) ("Sound of the particles").
- Water (the hand in the water, hits, drowning, boat, waterfall and ark, sea, coast and lake ambience):
  [water.md](water.md#water-audio). Spells: [particles.md](particles.md) and [miracles.md](miracles.md).

## State in openblack

Whoever touches `src/Audio` reads this page first.

**Done**
- A single engine (`src/Audio`, API `Audio.h`, layers Device / LH / GAudio / Services) that replaces the AudioManager:
  QMixer's 16 channels, volume laws, anim-effects, 6-track music on its thread, banks, SoundTags, ambience,
  SoundMap, voices and advisors, Guidance, night voices, audio and script CHL (dialogue, widescreen, camera
  locks), miracles, interface and hand; the audio reads the game only through `GameQueries` (`src/ECS/AudioQueries.cpp`).
- Weather and alignment in ambience and music, the citadel interior, videos (`videoPlaying`,
  ambience silenced and music continuing underneath; the 11 sounds of fall.bik and `MusicStop(1)` at 43.9 s, checked).
- Tribe music per town (with fn_00741020), Guidance town search, town desires
  (`desireTowns`, `townResourceNeeds` on top of `ecs::town_desire`) and temple desires (`worshipSites` on top of `worship::citadel`
  / `worship::site`), single local randomness (`game_random`), the 24-bit FPU rule.

What is left is in [Pending](#pending).

**How to test** (see also [Test hooks](#test-hooks))
- The whole log goes to `openblack.log`.
- Desires: in Land 1 the guide spirit is silent on purpose (PlayNow 0x71AF6F); `-s Land2.txt OPENBLACK_GUIDANCE_TRACE=1
  OPENBLACK_CAMERA_FLY="2187,120,2200,2187,20,2260"` (town 94). Temple: `OPENBLACK_TEST_WORSHIP_PLAYER=1
  OPENBLACK_TEST_WORSHIP="1,0.5" OPENBLACK_CAMERA_FLY="2540,150,1740,2540,100,1800"`.
- Video: `OPENBLACK_TEST_VIDEO=fall OPENBLACK_TEST_MUSIC=54@5 OPENBLACK_SFX_TRACE=1 OPENBLACK_MUSIC_TRACE=1`; the test
  music must start after the map is loaded (ClearMap cuts it with LHMusicStop(0) 0x426CD3).

## Weather and alignment in openblack

With the weather (`src/ECS/Weather`) and the alignment (`src/ECS/Effects/Alignment`) in the base, the ambience and
the music read the real values. Only through those modules' public APIs and through `GameQueries` (the audio does not include the
ECS: `src/ECS/AudioQueries.cpp` registers it).

- **Weather** (`GameQueries::weatherSmooth`):
  `GCamera::Update` fills GCamera+0x80 with `LH3DAtmos::GetWeatherSmooth(camera position, 1)` 0x835180 and
  `GSoundMap` reads it (temperature, rain, snow, overcast, wind x/z: weatherFade, RAIN, WIND). openblack:
  `audio::CameraWeather()` asks for `weather::atmos::GetWeatherSmooth(camera origin, true)` when
  computing the sound map. `GetWeatherSmooth` has no state (bilinear between cells, the grid cache per
  frame), so asking for it again on the audio turn gives the same as GCamera+0x80 **(approximate: the original
  takes it in the frame's GCamera::Update, openblack on the audio turn with the camera at that moment)**.
- **GAudio+0x190** (`GameQueries::cameraAlignment`, `ecs::audio_queries`): `fn_0064AC30` (at the end of
  `GPlayer::ProcessPlayers` 0x64A697, once per turn) calls `fn_005E2240(x)` with x = clamp((alignment of the
  most influential player at the camera + 1) / 2, 0, 1) = `ecs::effects::alignment::GetInterfaceAlignment()`.
  fn_005E2240 (0x5E2240..0x5E2291, in float steps because of the 24-bit FPU): x < 0 → 0 (`fcom [0x8AA398]; test ah, 1`),
  x > 1 → 1 (`test ah, 0x41`), s = (1 − x) + (1 − x) (to [0xBF337C], the sky), +0x190 = 2 (0x8AB478) − s − 1. The sky
  uses the same x, so openblack's sky settings (`OPENBLACK_TEST_SKY_ALIGNMENT`, the debug
  slider) also reach the audio: if `Clouds::InfluentialPlayerAlignment()` is not 2x − 1, x = (value + 1) / 2.
  The audio no longer includes `3D/Clouds.h`: `atmos_banks::Alignment()` reads the query (0 without it, `GAudio::Reset`
  0x426CC2). The audio turn goes after the magic turn (Game.cpp), like `GGame::EndTurn` after `ProcessPlayers`.
- **Ambience group** (`ProcessAtmosBanks` 0x428FE0, 0x428FFA..0x42901E): for each bank,
  `LHAtmosSetGroup(bank, +0x190 > −0.59999999999999998 (double 0x8C4A08) ? 1 : 2)`; the loops and one-shots of another
  group fade out or do not start. Trace: `(openblack) Atmos group g (alignment a)` when it changes
  (`OPENBLACK_ATMOS_TRACE`).
- **Alignment music** (`ProcessAlignmentMusic` 0x4279C0, `fn_00427460`): `GetDiscreteAlignmentValue` 0x414730
  of +0x190 (0..6), the table 0x9C99F0 (0,0,1,1,1,2,2) and GENERIC_EVIL / NEUTRAL / GOOD = index + 1 (0x427579). Trace:
  `(openblack) alignment music type t (GAudio+0x190 a, discrete d)` on change (`OPENBLACK_MUSIC_TRACE`).
- **Town and tribe** (`ECS/AudioQueries.cpp`, `NearestMusicTown` / `KeptMusicTown`): fn_00427460 receives
  GetCamera()+0x14 (0x427A3B..0x427A46) = the MapCoords of `Camera::GetOrigin`, the same point as
  `GameQueries::camera` (GCamera::UpdateGameThingWithPosData 0x442EF0 takes them from LH3DTech::g_camera, 0x442EF3..0x442F35); its +8, the height that 0x4274C4 compares, is y − the altitude byte of the camera's cell × 0.67,
  without interpolation, or y outside the map / without a block (0x442F38..0x442FCE; before, openblack used the interpolated GetHeightAt);
  `nearestTown` = fn_00602160 (`map_cells::GetNearestTownWithCentre`, strict <, only those
  of +0x9A4) with townTriggerOffDistance; the tribe, the town's `Tribe` component (Town +0x5B8, 0x42753D /
  0x42755A); the distance, `gutils::GetDistanceInMetres` 0x74CD70 (0x4274EC, 0x427522); `town` = the town of
  GAudio+0x18C while it is valid (IsAvailable 0x4274AF). The logic (≤ 300 and height < 400, 0x4274C4..0x427535) was already
  in `GameMusic::AlignmentMusicType`. The other test of fn_00602160, fn_00741020 (0x6021B3), is
  `map_cells::TownHasCentre` (a TownCentre among the buildings +0x754, IsTownCentre vt+0x1E0 0x55DB70, or a planned
  building +0x9A8 with GetAbodeNumber 0x401260 == 0xC; vt+0x44 is GetAbodeNumber, not GetComputerSeen): a town without
  CREATE_TOWN_CENTRE gives tribe music.
- **Guidance**: `PlayResourceDropRemark` 0x71B570 looks for the town with `nearestTownAt` = `map_cells::GetNearestTown`
  0x6020E0(100, 0x98013C) at the point's MapCoords; its three values (`townResourceNeeds`, GetResourceDropSample
  0x71B5F0: Town +0xC4/+0x108/+0x19C… = TownDesire +0x90/+0xD4/+0x168 of desires 0, 1 and 10) come from
  `ecs::town_desire::GetField` (Raw + Boost + BoostA, in that order and in float). `desireTowns`: the towns from
  `map_cells::ForEachTown` with `GetSortedRawDesires` (+0x378: value +0x37C, type +0x380) and `GetRawDesire` 0x73E420.
  `worshipSites`: PLAYER_ONE's citadel (`worship::citadel::Of`), its six slots (`WorshipSitesOf`), +0x70
  (`StrainSoundFraction`; the cap at 0x71B319 lets a NaN through, test ah, 1), DancerCount > 0 and
  `worship::site::CalculateDesireForFood` 0x77C310. `heartBeat`: see [Chants and heartbeat](#chants-and-heartbeat-in-openblack).
- Temple check: in Land 2 the human's citadel starts without worship sites, so it is
  tested as player 1: `-s Land2.txt OPENBLACK_TEST_WORSHIP_PLAYER=1 OPENBLACK_TEST_WORSHIP="1,0.5"
  OPENBLACK_CAMERA_FLY="2540,150,1740,2540,100,1800" OPENBLACK_GUIDANCE_TRACE=1` → site 148 with dancers and
  food 1.0 → `text 4993 3D -> Guidance.sad 24` and `4992 -> 23` (NEEDWORSHIPPERS_FOOD).
- Desires check: in Land 1 nothing sounds, as in the original (PlayNow 0x71AF6F..0x71AF8F:
  TownDesire is not "always" on land 1 without multiplayer). `-s Land2.txt OPENBLACK_GUIDANCE_TRACE=1
  OPENBLACK_CAMERA_FLY="2187,120,2200,2187,20,2260"` (store of town 94 at 59.9) → `desire sample 4982 value
  0.346` → `type 0 text 4982 3D -> Guidance.sad 143` (DESIRE_EXPAND), and the value drops on repetition (t0 of DesireScore).
- Check (Land 1, `OPENBLACK_TEST_ALIGNMENT_MUSIC=1 OPENBLACK_TEST_TEXT_CLICK=1 OPENBLACK_MUSIC_TRACE=1`,
  `-n 60000`): `OPENBLACK_CAMERA_LOCK="1850,90,2620,1865,30,2650"` (town 0, NORSE, with a centre) → `alignment music
  type 23`, `MUSIC_TYPE_NORSE_TOWN_NEUTRAL`, **celt_neutral.sad** plays (22..24 point to the Celtic strings);
  `"2440,90,2560,2450,30,2580"` (town 4, AZTEC) → type 8, `AZTEC_TOWN_NEUTRAL`, **aztc_neutral.sad**.

**In-game check** (Land 1):
- `OPENBLACK_TEST_WEATHER="1818,2628,100,100"` with `OPENBLACK_CAMERA_LOCK="1775,60,2595,1830,45,2650"` and
  `OPENBLACK_ATMOS_TRACE=50`: `ATMOS_TYPE_RAIN Vol=1.000 Sent=127`, the `rainconst.wav` loop
  from rain.sad starts and `thunder_*.wav` one-shots sound; WIND 0 (6 m/s wind in the test storm).
- `OPENBLACK_TEST_SKY_ALIGNMENT=-1`: `Atmos group 2 (alignment -1.000)`; without it, group 1 with 0;
  with 0.8, group 1.
- With `OPENBLACK_TEST_ALIGNMENT_MUSIC=1` and `OPENBLACK_TEST_TEXT_CLICK=1` (the Land 1 script does
  `ENABLE_DISABLE_ALIGNMENT_MUSIC(0)` at the start), when intro.sad ends: with −1, `alignment music type 1 (… −1.000,
  discrete 0)`, `MUSIC_TYPE_GENERIC_EVIL` and evil.sad plays; with 0.8, `type 3 (… discrete 6)`, `GENERIC_GOOD`, good.sad.
  Without the hook, `Music Playing=NONE` after the intro (the script has it switched off), the same as before.

**Audit.** Reviewed in the disassembly: fn_005E2240 0x5E2240..0x5E2299, fn_0064AC30
0x64AC30..0x64ACAB, the call 0x64A697, ProcessAtmosBanks 0x428FFA..0x42901E (`push 2` at 0x429015), GAudio::Reset
0x426CC2, fn_00427460 0x427466 / 0x427579, ProcessAlignmentMusic 0x4279C0..0x427A46, ENABLE_DISABLE_ALIGNMENT_MUSIC
0x710120 (g_game+0x250090 +0x94), GetDiscreteAlignmentValue 0x414730 and fn_00426C80; the callers of
ProcessAudioGameTurn 0x427080 (GGame::EndTurn 0x54E9A6, after ProcessTurn, and Temple::ProcessGameTurn 0x794A5A) and of
GAudio::Reset (GGame::Init 0x54F474, ClearMap 0x552D98). Everything adds up. Fixed: the clamp of fn_005E2240 with a NaN
(the `fcom` leaves C0 as unordered: `test ah, 1` takes it to 0, +0x190 = −1; openblack took it to 1); the formula is now
in `ecs::audio_queries::AudioAlignmentValue(x)` (was `GAudioAlignment`) and the group threshold in `atmos_banks::GroupFor(alignment)`, with new
tests (`AudioLaws.AlignmentCurve`, `AudioLaws.AtmosGroupByAlignment`: the float −0.6 is below the double
−0.59999999999999998, group 2). No new AL sources or buffers, no ECS components in `src/Audio`, the query is
read only on the game thread (neither the music thread nor its callbacks use it). In game
(with `-l stdout`): `Atmos group 2 (alignment -1.000)` / `Atmos group 1 (alignment
0.800)`; with −1 and the music hook, `alignment music type 1 (… discrete 0)`, GENERIC_EVIL and evil.sad. Notes: in the
citadel the original runs the audio turn from Temple::ProcessGameTurn without fn_0064AC30 (+0x190 keeps the
last value); `GetDiscreteAlignmentValue` with a NaN (0x414756 lets it through to `__ftol`) is not matched (it does not
happen: +0x190 is never NaN).

## Chants and heartbeat in openblack

The audio does not include the ECS: the game
answers through `GameQueries` (`src/ECS/AudioQueries.cpp`).

**Chants** (`GAudio::ProcessChantMusic` 0x427790, branch 6 of `ProcessMusic` 0x427E4D). Only `ProcessMusic` calls it;
worship does not call the audio (the dance, `Dance::ProcessDances` 0x50BB60, does not touch the music).
- Game side (`GameQueries::chantSite`, `ChantSite()` in AudioQueries.cpp):
  `map_cells::GetNearestCitadel` 0x602200 (150, 0x43160000) at the camera's MapCoords;
  `worship::citadel::FindNearestWorshipSite` = fn_004639A0 (100, 0x42C80000): of the six slots in order, the one that has
  dancers (fn_0077B960) with the dance centre (fn_0077CD90 = `GetSpecialPos(8)`) nearest,
  `GetDistanceInMetres` < best (strict). It returns the tribe (fn_0077C2E0: +0x8C → +0x10), the dancers
  (Dance +0x90), the centre as `GetLHPoint` and the `GetAltitude` of the centre and of the camera.
- Audio side (`GameMusic::ProcessChantMusic`): camera height = `GetAltitude(camera)` + MapCoords+8;
  |height − ground at the centre| < 100 (0x427855; a NaN passes, `test ah, 1`); type = `ChantMusicType` (fn_00427430:
  table 0x9C9A30 `{28,28,30,32,34,36,38,40,42}`, the African one with the Celtic one, + 1 with more than 8 dancers (`jbe`,
  unsigned), 5 from tribe 9); without a bank, nothing; options: vol 127, start GAudio+0x18[group − 1] read before
  `SavePositions` (0x4278FF / 0x42792D), sync 1, **no fade**, 3D, pitch 100, the position; `LHMusicPlay` every turn and
  `Set3DPosition` if it got a channel; "Music Playing=…"; then `ProcessMusic` sets NONE (0x427E95).
- `_vox` is played alone (the whole chant with voices; confirmed by the user): it is another bank, not a layer on top.
- **(inferred)**: every openblack worship site has its dance (it is created with it), so +0xA0 is never null; a
  centre without a mesh point stays at MapCoords 0, like the original's zeroed MapCoords.
- **(approximate)**: without a camera there is no chant (the original always has one).
- Tests: `ChantMusic.Table`, `GameMusicTest.ChantMusicAtTheDance` (test_game_music.cpp).

**Heartbeat** (`GameQueries::heartBeat`, `HeartBeat()` in AudioQueries.cpp; the calculation was already in `guidance`).
- `protectionDesire`: Σ `Town::GetRawDesire(3)` 0x73E420 of PLAYER_ONE's towns (`map_cells::TownsOf`, the
  order of the list +0xA50), summed in float.
- `believers` = `magic::players::BelieverFraction` 0x64B680: men + women (TownStats
  +0x54/+0x58 = Town +0x664/+0x668) of their towns divided by `WorldPopulation` (g_game+0x205A54: +1 in the Villager ctor
  0x74FAFF, −1 in SetDying 0x76A552 or in the dtor 0x74FBD4); 0 if either is 0. **(approximate)**: openblack does not keep the
  counter and counts the Villager entities (a villager who dies is deleted on the spot).
- `beliefShare` = `influence::InfluencePowerRatio` = fn_0064B700: **Σ +0x8C of the active players and the neutral one
  divided by one's own +0x8C** (`fdivr` 0x64B74F; 0 if one's own is 0). The query said the opposite (one's own divided by the
  sum); the comment was fixed, Guidance does not change. **(inferred)**: active (+0x8E0 ≠ 0,
  GetNextActivePlayerAndNeutral 0x550930) = the player has an entity (`magic::players::EntityOf`).
- GPlayer +0x8C = `influence::CalculateInfluencePower` 0x64AD00 (from GPlayer::Process 0x64971D, after the
  alignment; in openblack `influence::CalculateInfluencePowers` in slot 3 of `magic::ProcessTurnStart`):
  `Citadel::GetInfluence` (with heart) + Σ Town +0x5C8 + Σ radius +0x38 of the player's rings (also the anti ones),
  in float. +0x90 and the GameStats history have no reader: not ported. **(approximate)**: it is stored with the land
  (`InfluenceGlobals::power`), not in the GPlayer.
- `citadelHeart`: the citadel (+0xA48) with a built and living heart (`worship::citadel::HasLivingHeart`; 0x71C574..
  0x71C5A6) → its position. **(inferred)**: openblack's heart is the temple itself, built with it; its life is
  `ecs::life::LifeOf` of the temple (1).
- Nearby enemy creatures (0x71C2A6..0x71C379): **pending: creature**, the list is empty (they add 0).
- Test: `InfluenceTest.influencePowerAndRatio` (test_influence.cpp).

## The citadel interior in openblack

`g_game+0x205A28` (0x4282F0, the symbol says `HelpSystem::GetWideScreenControl`) is set to 1 by `GGame::GoInsideCitadel`
0x554004 and to 0 by `GGame::LeaveInsideCitadel` 0x553B1F. In openblack the citadel is the temple interior
(`Locator::temple`, `TempleInteriorInterface::Active`, opened and closed by `ENTER_EXIT_CITADEL` and the debug
window), as `StartCameraControl` (CHLApi.cpp) already reads it. `Game.cpp` registers `GameQueries::insideCitadel` with
that: `SetInsideCitadel` is that query. What the audio does inside (all disassembled again):

- **Filters** (now with data): `GAudio::PlaySoundEffect` 0x429F6D and `SamplePlayAnimEffect`
  0x42A554 only let the userParam 2 samples sound (`cmp di/bp, 2`); the 3D cutoff is measured from
  `LH3DTech::g_camera` (0x429EB1), which in openblack is the same camera (the temple one inside). The ambience is turned off
  (fn_00429100, `atmos_banks::SetTargets`) and the advisor does not interrupt (0x5C3810).
- **Music** (`GameMusic::ProcessCitadelMusic`, `ProcessCitadelMusic` 0x427B60, first branch of `ProcessMusic`
  0x427E2C): inside, on the first turn `LHSampleStopAll` (`sample_play::StopAll`, the 16 channels; the latch is the
  global [0xC56164], which `GAudio::Reset` does not touch and which goes back to 0 on leaving, 0x427C7F); type = 44 + fn_00426C80(
  `GetDiscreteAlignmentValue` 0x414730 of `GPlayer::GetAlignmentValue` 0x64D6A0 of the local player g_game+0x205A59),
  CITADEL_EVIL / NEUTRAL / GOOD, all three in `citadel.sad` (group 4); without a bank it returns 0 and `ProcessMusic` continues
  (0x427BDF). Options: bank, volume 127, start `pos[group − 1]` read (0x427BF6) **before** `fn_004281C0` saves the positions (0x427C28; the alignment saves first), sync 1, fade 1, 2D, pitch 100;
  `LHMusicPlay` **every turn** (the engine re-triggers the same bank, 0x1000DFD4), "Music Playing=%s" and
  returns 1 → `ProcessMusic` sets "Music Playing=NONE", +0x180 = 0, +0x1C = −1 (on leaving the alignment music starts
  again). The alignment is the new query `GameQueries::localPlayerAlignment` (`ecs::audio_queries`:
  `ecs::effects::alignment::Get(PLAYER_ONE)`; 0 without it), not GAudio+0x190. It replaces the `citadelMusic` query.
- **Leaving** (`audio::LeaveCitadel`, from `TempleInterior::Deactivate`): `LeaveInsideCitadel` 0x553B25 → Temple
  `fn_00793D00` (if its engine was started, Temple+0x24): `StopPlayingSoundEffect(2 G_Fire_01, owner 0, InGame)`
  0x793D48 and `(12 G_WaterFlow, 0, InGame)` 0x793D59. Entering has no audio call of its own (0x553E10..0x55405E;
  `Temple::InitEngine` 0x793C60 does not touch the audio).
- **fn_00427200** (the game's 3D function that LHaudio calls at 0x1001438C, 0x1001487B and 0x10014B91): each coordinate
  with |v| > 5000 becomes 0 (`fabs; fcomp qword 5000.0` 0x8C49E0, `test ah, 0x41`: equal, less and NaN stay), in the
  channel's stored point (+0x50, 0x427222..0x42726D, after copying it for the default case) and in the point
  it returns (0x427349..0x42738E); the distance it returns (0x427399..0x427400) is that of the point **without** the cap plus
  the offset to `g_camera`. `audio::GuardSoundPoint`, in `sample_play::UpdateChannels` (LHSampleUpdate3DChannels
  0x10014310: it stops the channel if that distance is not less than the maximum +0x6C, 0x100143AF; otherwise,
  LHSampleSet3DPosition with the capped point) and in `audio::OwnerSoundPoint` (the start of the anim-effects).
- **HelpSystem** `ReadSpeedFactor` fn_005C6CB0: with the game thread's FPU at 24 bits (fn_007DEE00) each step
  rounds to float (the qword constants 0.5, 1, 0.80000000000000004 and 0.20000000000000001 whole, the dwords 4 and 3);
  it returns float; a NaN goes to the first branch (`fcom`, `test ah, 0x41`). E.g.: 0.1 → 2.5999999 (not 2.59999999404);
  0.7 → 0.68000001.

**API**: `audio::LeaveCitadel()` and `audio::ProcessCitadelTurn()` (Audio.h), `audio::GuardSoundPoint(p)` (AudioSystem.h),
`GameQueries::insideCitadel` (now registered), `GameQueries::localPlayerAlignment` (new), removed
`GameQueries::citadelMusic`; `GameMusic::GetCitadelSamplesStopped()` ([0xC56164], for the tests).

**Tests**: `GameMusicTest.CitadelMusicInsideTheCitadel`, `GameMusicTest.CitadelMusicWithoutItsBank`,
`GameSfx.GuardSoundPoint`, `SamplePlayTest.TrackedPointGuardedAt5000`, `HelpSystem.ReadSpeedFactor` (values at 24 bits),
`AudioManager.EachTurnReachesTheServiceAsItsOwnCall`, `PausedTurnTimer.*` (test_game_clock).

**The world pauses inside the citadel.** In the original:
- Entering (`GoInsideCitadel`), the pause flag is kept (g_game+0x2502C4 = bit 2 of g_game+0x14, 0x553F61..0x553F72) and,
  in a single player game, `PauseGame(1)` (0x553F83). Leaving (`LeaveInsideCitadel`), after the sequence mode 0
  (0x553B1F) and fn_00793D00, `PauseGame(g_game+0x2502C4)` (0x553B44..0x553B4B) gives the kept flag back. `PauseGame`
  stops the game timer and starts it again from now, so the world goes on from the turn it stopped at, without catching
  up.
- While paused, the world's turns wait (`LocalTimerSaysDoATurn` says no). In sequence mode 1 (or with the paused
  cinematic), `ProcessNetworkPackets` (0x54CC66..0x54CCFF) has a turn every 100 ms of `GetTickCount`: the static
  [0xCD3AE8] is set to now the first time (0x54CC98), then a turn is due once now − it > 100 (0x54CCA9, `jbe`), which
  moves it on by 100 (0x54CCB0), or to now when it is still more than 200 behind (0x54CCC1..0x54CCCA). It is never
  reset. In sequence mode 1 that turn is `Temple::ProcessGameTurn` 0x794A30 (0x54CCFF).
- `Temple::ProcessGameTurn`, only while paused (0x794A35): `GDebug::ClearMessages(-1)` (0x794A42),
  `ScriptDLL::LookIn(0x18)` (TempleHelp | TempleSpecial, 0x794A4F), `ProcessAudioGameTurn` (0x794A5A) and
  `HelpSystem::Process` (0x794A6F: the hand state, the icons, the spirits, the tooltips, the bubble, the conditions and
  the banter). The paused end of turn (`AtmosProcess(0)`) goes on in between.
- The tooltip's icon is kept while paused inside the citadel (fn_005C9D00 0x5C9D4A, `cmp g_game+0x205A28, 1`), and the
  helpers' frame step (fn_005557E0 0x5557E5) is the real frame time in sequence mode 1.

openblack:
- `TempleInterior::Activate` keeps `game_clock::IsPaused()` and calls `game_clock::Pause(true)` before the sequence mode
  1; `Deactivate` gives the kept flag back after the sequence mode 0. `game_clock::Pause` is `PauseGame`'s: the timer
  stops and starts again from now, the turn number stays, and there is no burst of turns on leaving.
- `Game::Update`, while paused in sequence mode 1: `game_clock::PausedTurnTimer` (the rule of [0xCD3AE8], on
  `game_clock::TickCount()`, so the fixed step makes it deterministic), then `Game::ProcessTempleTurn`:
  `LookIn(TempleHelp | TempleSpecial)`, `audio::ProcessCitadelTurn()` (ProcessAudioGameTurn alone: no sound map, no
  sound tags, no turn 5 gate), the room's tooltip (`TempleInteriorInterface::ProcessTurn`, its Submit), `help::Process`
  and the `OPENBLACK_AUDIO_TEST_CITADEL` hook. The tooltips age once a temple turn: the temple interior's own 100 ms
  tooltip turn on the frame time (raffclar's) is gone, and `help::Process` no longer runs from the world's turns inside.
  Not ported: `GDebug::ClearMessages` and the paused cinematic. `audio::Paused()` still runs once a frame while paused,
  as for every openblack pause ([Pending](#pending)).
- `help::tooltips::Frame` keeps the icon while paused inside the citadel; the spirits' frame step reads
  `game_clock::IsInsideCitadel()` (the sequence mode) rather than the temple being active.
- **(inferred)** The escape menu, opened inside the temple by its options room, does not pause or unpause the game:
  the temple holds the pause. The original's options room has not been read.
- What is audible: the citadel's music and filters on the temple's 100 ms turns, and nothing of the world, which is
  paused (no world turn: no sound map, tags, villagers or animals), as in the original.

### Audit: citadel interior

Reviewed against the disassembly: the whole of fn_00427200 (0x427209..0x42726D the uncapped copy and the cap of +0x50,
0x4272AD / 0x4272E1 the flag 0, 0x427321..0x42738E the returned capped point, 0x427399..0x427400 the uncapped
distance + offset), LHSampleUpdate3DChannels 0x10014310 (flag 0 → LHSampleStop and +0x18 = 0; `fcomp` +0x6C,
`test ah, 1`: a NaN does not stop; LHSampleSet3DPosition with the capped point), LHSampleSet3DPosition 0x10013AC0 (the QMixer
source at point + offset 0x10013E76..0x10013E99, +0x50 = the point 0x10013EBA), LHSamplePlayAnimEffect
0x10014B91 (the returned point goes into the options), ProcessCitadelMusic 0x427B60..0x427C8F (options +0x00/+0x04/+0x14/
+0x1C/+0x20/+0x24/+0x28; the bank it checks, GAudio+0xDC + 4·index, is that of type 44 + index), 0x4282F0
(`== 1`), ProcessMusic 0x427DF0..0x427EBB, 0x426C80, 0x64D6A0, 0x429F6D, 0x42A554, Temple fn_00793D00
(0x793D3C..0x793D59), StopPlayingSoundEffect 0x42A210, LeaveInsideCitadel 0x553B1F / 0x553B25, GoInsideCitadel
0x553E10..0x55405E (its calls: none for audio; fn_00463A50 → fn_00469E70 is for the citadel, not for sound),
fn_005C6CB0 and fn_005C61B0. Everything matches the code; nothing to fix.

- `ReadSpeedFactor`: the team's rule (step in double, result to float) could round twice in the qword `fmul`
  and `fadd`; checked **exhaustively** for the 2^23 floats of the second branch (b = 2 − 2r is exact; the
  double rounding only differs if the double falls on a float midpoint: none does) and the first branch is a
  single rounding (3 − 4r is exact in double): **faithful** for every READ_SPEED.
- No new AL sources or buffers, no ECS dependencies in `src/Audio`, no new music thread (`LHMusicPlay`
  from the game turn, like the other branches); the latch [0xC56164] is global as in the original.
- In game, measured before the world paused inside the citadel (to measure again: inside, the world's samples no longer
  start at all, and the hook's 80 turns inside are the temple's) (Land 1, `OPENBLACK_AUDIO_TEST_CITADEL="100,180"`, traces AUDIO/SFX/MUSIC/ATMOS): on entering `LHSampleStopAll` stops the
  tree creaks on channels 0, 2 and 4; inside no sample starts (all `filtered (inside the citadel…)`),
  citadel.sad (CITADEL_NEUTRAL) rises 4 → 127 while intro.sad goes down; on leaving `SFX: stop InGame.sad/2` and `/12`
  owner 0, `MUSIC_TYPE_SCRIPT_INTRO` comes back from chunk 1 abruptly (fn_00427CA0 passes fade 0, 0x427D41) and
  citadel.sad fades out, the ambience loops start again and the creaks sound again.

### The temple interior's sounds

All in InGame.sad, all user parameter 2 (they pass the citadel's filter), all 2D and without an owner but where said.
The code: `3D/TempleSounds.h`, `TempleDoors`, `TempleToggles::ClickOf`, `TempleScrolls`, `TempleInterior::HoldControl`.

| Event | Sample | Call | Mode, loops | Where |
|---|---|---|---|---|
| A doorway starts to swing open (its swing goes from ≤ 0 to > 0) | 60 G_CitadelDoorOpen_01 | `PlaySoundEffect(0, 60, 2, 0, 0, 0, InGame)` | 2, 0 | 0x794DAE (fn_00794D30) |
| A doorway turns round to close / is shut at once | 61 G_CitadelDoorClose_02 | `PlaySoundEffect(0, 61, 2, 0, 0, 0, InGame)` | 2, 0 | 0x794FED (fn_00794FB0), 0x794F8D (`InnerRoom::FastCloseDoor`) |
| A scroll starts to turn | 54 + GetTickCount() % 6, G_ScrollSqueak_01..06 | `PlaySoundEffect(0, 54 + t % 6, 2, 0, 0, 0, InGame)` | 2, 0 | 0x78429E (challenge), 0x7894C3 (creature), 0x78B64C, 0x79201E (save game) |
| The camera starts to look at a scroll (InnerCamera state ≠ 4) | 46 + (GetTickCount() & 3), G_Woosh_01..04 | `PlaySoundEffect(0, 46 + (t & 3), 3, 0, 0, 0, InGame)` | 3, 0 | 0x7957A6 (`InnerCamera::FocusOnSubMesh`) |
| A map button turned off / on | 63 G_CitadelButtonDown_01 / 62 G_CitadelButtonUp_01 | options: bank InGame, is3D 0, track 1 (the default), owner 0, mode 3, pitch below | 3, 0 | the ten callbacks at 0xC2A7A8 (0x79E940..0x79EDA0) |

- The scrolls' squeak: the flag (0xDD0BE8 in the challenge room, 0xDE0FA4 in the creature room) is cleared when the mouse
  does not move **and** when the clamped position does not change (0x7842AB), set after a squeak; a squeak only while
  it is clear. The pick is the clock, not a random stream: no random draw.
- The buttons, by their flag and tooltip: Temples (0xC2A4EC, 0xE91) 100 / 100; Creatures (0xC2A4F8, 0xE8F) down 110,
  up: 110 is written to the options' **owner** (+0x20) and the pitch stays 100 (0x79EAD5), an original slip, kept
  (`Owner::Key(110)`); Miracle activity (0xC2A4F0, 0xE93) 95 / 95; Influence (0xC2A4F4, 0xE92) 105 / 105, its on
  callback also sets `WorldRoom::ShowInfluence` and calls fn_0079DCD0; Challenges (0xC2A4FC, 0xE90) 108 / 108. The
  .sad has no pitch override for 62/63, so the caller's pitch plays (with deviation 0).
- From the .sad: 54..59 deviation 10 and mode 2 (0x400), 60/61 mode 2, 62/63 mode 3, 46..49 pitch 90 (0x1) deviation 10.

## Test hooks

| Hook | What it does |
|---|---|
| `OPENBLACK_MUSIC_TRACE=1` | One `music:` line per pass of the thread with each busy channel: bank, state, cur/target/sad, chunk, loops, queue, QMixer volume, gain and decoded samples. It also writes `game music: Music Playing=…` each time it changes |
| `OPENBLACK_TEST_MUSIC="<type>[,<type>@<s>][,stop@<s>][,cut@<s>]"` | The first track is played like the trailer (vol 127, no sync or fade, 2D). Each of the following ones, at `s` seconds, with sync and fade, like `ProcessCitadelMusic` (to hear a synchronised change). `stop` = `LHMusicStop(1)`, `cut` = `LHMusicStop(0)`. It is a hook, not a behaviour of the original |
| `OPENBLACK_TEST_MUSIC_VOLUME=<0..127>` | The music master at startup |
| `OPENBLACK_TEST_ALIGNMENT_MUSIC=<turn>` | From that turn, every turn, `ENABLE_DISABLE_ALIGNMENT_MUSIC(1)` and without the script widescreen filter (which Land 1 leaves on in openblack), to hear the alignment music. Not from the original |
| `OPENBLACK_TEST_SCRIPT_MUSIC="<type>[@<turn>]"` | A script START_MUSIC on that turn (30 by default) |
| `OPENBLACK_AUDIO_TRACE=1` | Each channel start, steal, stop and cutoff (`Sample play:`), each buffer created (`Wave buffer … N made`), the changes of the script widescreen, the anim-effects rejected by distance (`Anim effect: bank/n (wave) too far`) and the old `AudioManager` traces |
| `OPENBLACK_ANIM_TRACE=1` | The sounds of the clips and of the trees (`Animation sound: clip … -> editor.sad/n`, `key … -> editor.sad/n`, `no row`, `banter n too far`), with the same format as the old trace |
| `OPENBLACK_SOUND_TAG_TRACE=1` | Each tag created, deleted, released or with a delay, and every 50 turns the channel of each thing tag |
| `OPENBLACK_SFX_TRACE=1` | One `SFX:` line per call to `GAudio::PlaySoundEffect` (and to the tags), to `PlayAnimationEffect` and to `StopSoundEffect`: bank/sample (wave), 2D/3D, track, point, mode and loops it starts with, pitch, owner and what happened (channel, `culled`, `filtered (reason)`); and the script's `PLAY_SOUND_EFFECT(...)` |
| `OPENBLACK_AUDIO_TEST_VIEW="turn,n[,distance]"` / `OPENBLACK_AUDIO_TEST_ANIM=<clip>` | On that turn the camera looks at villager n from that distance (4), and all villagers play that clip in a loop (437 yawn, 354 saw, 369 sitting). In `src/ECS/AudioQueries.cpp` (`ecs::audio_queries::RunTestHooks`) |
| `OPENBLACK_AUDIO_TEST_LANTERN="turn[,distance]"` | On that turn (counted by the calls to `RunTestHooks`) the camera looks at the tip of the first street lantern from that distance (3) |
| `OPENBLACK_AUDIO_TEST_CITADEL="<enter>[,<leave>]"` | On those hook turns it enters / leaves the temple interior, like `ENTER_EXIT_CITADEL(1)` / `(0)`: the citadel music and filters. The hook turns are the world's (`RunTestHooks`, one per turn) and, while the world is paused inside the temple, the temple's own 100 ms turns (`RunCitadelTestHook` from `Game::ProcessTempleTurn`). Not from the original |
| `OPENBLACK_AUDIO_TEST_NO_WIDESCREEN=1` | The audio does not see the script widescreen (the Land 1 intro has it until a click), to compare without that filter. Not from the original |
| `OPENBLACK_TEST_SAMPLE_VOLUME=<0..127>` | The effects master at startup |
| "Channels" tab of the audio panel | Effects master (slider), LHWaveIsActive, live/created buffers and the 16 channels (sample, bank, owner, priority, volume, pitch, 3D/track/ambience, playing) |
| `OPENBLACK_TEXT_TRACE=1` | Each RUN_TEXT/TEMP_TEXT text in the log (`|` for each line break) |
| `OPENBLACK_AUDIO_TRACE=1` (voices) | `Advisor: dude n says HelpSprites m (s)` when each advisor sentence starts; `Wave of … read from <bank>` when a wave is read from a dialogue bank |
| `OPENBLACK_TEST_TEXT_CLICK=1` | Every turn, if a text is waiting for the click, it does the left click (`HelpSystem::ProcessInterface(true)`); see map-loading.md |
| `OPENBLACK_TEST_BW_ROOT=<install>` | For the tests with data: `test_audio_tables`, `test_music_bank`, `test_music_stream`, `test_game_music`, `test_voice_table`, `test_help_system`, `test_sample_play`, `test_anim_effects`, `test_sound_tags`, `test_script_sound`, `test_voices`, `test_spooky_voices` |
| `OPENBLACK_GUIDANCE_TRACE=1` | `Guidance:` for each Init, PlaySample (type, text or sample, 2D/3D, bank/sample, channel) and HelpSpiritSay (text, type, script, started or not); `SpookyVoices:` on Init and PlaySpooky |
| `OPENBLACK_TEST_GUIDANCE_SAY=<turn>:<HELP_TEXT>` | On that turn, `SpiritSay(text, 32)`: tests the `MultiHelpJustTalkWithText` script (not from the original) |
| `OPENBLACK_PLAYER_NAME=<name>` | The profile name that GSpookyVoices::GetName reads (openblack has no profiles) |
| "Music" debug window | Master, 6 channels, MUSIC_TYPE player, GameMusic and GScript state, list of objects with music |

Example: `OPENBLACK_TEST_MUSIC="3,1@20" OPENBLACK_MUSIC_TRACE=1` starts good.sad and after 20 s switches to evil.sad
synchronised (same chunk).

## Addresses of the original, moved out of the code

The code's comments describe what it does in plain words; the original's addresses and function names they used to quote are kept here, next to the openblack symbol each one corresponds to.

| Address or name | What it is | openblack |
|---|---|---|
| `LHBankRegister 0x10002383..0x100023BD` | the audio DLL's reading of the first three u32 of a .sad's bank info block | `pack::AudioBankInfo` |
| `LHBankRegister (LHaudiodllR 0x10002240)` | registers a .sad bank; fails without LHFileSegmentBankInfo (0x10002368); with inMemory = 0 it keeps the file open (b+0 = 1) | `pack::PackFile (audio bank)` |
| `LHIsMusicBank 0x10002EE0` | returns the bank's 3rd u32 (bank+4) | `pack::PackFile::IsAudioMusicBank` |
| `LHMusicPlay 0x1000DF60` | tests LHIsMusicBank for non-zero | `pack::PackFile::IsAudioMusicBank` |
| `LHaudiodllR 0x10011420 -> fn_100032D0` | reads a wave at its first play | `pack::PackFile (wave data)` |

## Pending

- A wave of an unknown format opens as silence of its data's length (kept from the old wave library): it could be
  refused instead. None of the game's banks has one.

**Not yet in openblack**
- The creature: the event queue, the 5-column key, the per-species banks, the local player filter of
  SET_CREATURE_SOUND, the dance and fight branches of GameMusic, and the nearby enemy creatures of the heartbeat
  (0x71C2A6..0x71C379; today they add 0).
- Villagers and buildings: baby 20 + rand 10, sacrifice 179 and the editor screams (with the ScriptSfx 179 quirk),
  buildings (mill 13, workshop 74/150, totem 11 and bell 30, scaffolding, roof) and the other original callers of
  `tags::Create` / `Remove` (mill, workshop, totem, creed, falling trees).
- Save and load: `GAudio::Save` 0x428310 / `Load` 0x428480, `ThingMusicInfo::Save` 0x429950 / `Load` 0x429AE0,
  `PSysSound::Save` 0x6D14A0 / `Load` 0x6D13A0.
- GConfirmation: START_ANGLE_SOUND 285 and StartPitchSound 348 are stubs; it needs `CameraModeNew3` 0x454900 / 0x454930.
- The `trailer.sad` music of the pre-intro when pre_intro.bik is played; the original's menu (autosave 20 and boxes on
  top of the world) if it is ported.
- The volcano (`LandscapeVortex` 0x5FEE5A); `G_OutOfBounds` 43 (fn_0045FA00, there are no camera bounds); the ambience
  point tag fn_0071E920 (the GWeather thunder); the tribal power of `DoPostCastThings` 0x72930A (27 + tribe, bank 9;
  never in normal play).
- The advisors: the gestures from the samples' cue labels ([intro.md](intro.md#the-advisor-spirits)) and
  `HelpDude::PlaySoundFX` 0x5C2800.
- The Guidance callers from other areas (the API is already there):
  - `Villager::VillagerDead` 0x7506C0: fn_0071CE70 (KillingPeople, if the local player killed it and the cause table
    0x99A368), fn_0071C810 (DEATH_IN_VILLAGE, my villager, 0x99A370), fn_0071CFE0 (cause 4), LosingVillagers (town with
    +0x618 > info+0x150, 0x99A36C) and LowOnPeople;
  - `Town::UpdateAggressor` 0x73C9B0 (TownAttackSFX, fn_0071C960, fn_0071C9F0), `Town::CalculateDesireForFood`
    0x747FA0 / 0x7481BC (LowOnFood/Wood), `TownDesire::Process` 0x745C8A (VillagerUnhappy); the town desires are
    already read (`desireTowns` and `townResourceNeeds` on top of `ecs::town_desire`), and these three calls go inside
    TownDesire.cpp;
  - `Abode::ApplyEffectsDueToPhysicalDestruction` 0x406781 (DestroyBuilding: +0x90 +0x18 < 0.4 and the player who
    broke it), `Object::InitialisePhysicsFromHand` 0x6372EA (MakeDiscipleSFX, TODO in HandHolding.cpp), the totem
    0x738620/0x738666, `GBelief::AddToBelief` 0x437F2A, the creature (0x45A772, 0x5039E7), fn_0071D100 (other hands,
    multiplayer), GatheringBox/EndGameBox/network (OneOff 3 and others), fn_0064AF80 (multiplayer taunts);
  - the registry `DefName` and the network name of GetName; `fn_0081F1D0` (point on screen).
- Interface and hand, with no place in openblack yet:
  - **Logo** InGame 160: `GGame::Loop` 0x54D011 starts it once ([0xBEC27C]) and **stops** it on returning from `DoLogo`
    0x5FA070 (0x54D073). openblack does not have that logo sequence, so adding it would sound different (it would sound
    without the logo); it is left for when it exists.
  - **`G_ClickOnSpell_01` 42** from the arena (`ArenaSpellIcon` fn_00425660) and from the leash post
    (`LeashObj::InterfaceTap` 0x464490): openblack has no arena or leash. The worship icon one (0x726430 →
    fn_00726490, with its pitch) is there (`Worship/WorshipSpellIcon.cpp`).
  - **Conquering a town** `G_TakeOverTown_01` 205 (fn_00649810, from `SetTownEmpty` 0x7410DA): `Town::owner` is only
    written when the town is created, there is no conquest.
  - **Command acknowledged** `G_AcknowledgeCommand` 1: its three sites (0x5D2E2C, 0x5D3044, 0x5D4162) are orders to the
    creature with the leash and the arena (packets 0xF/0x10 and `fn_0048A490`/`fn_0048A420`); openblack has no
    creature.
  - **Virtual influence** `G_VirtualInfluence_04` 129 (`GVirtualInfluence::Draw` 0x76D000, `Start` fn_0076CED0,
    `SetSoundFraction` 0x76CF90, `Stop` fn_0076CF60; 2D loop with pitch = fraction·100): openblack does not have the
    anchored hand stretch or the mana path sprites.
  - **Chest and scrolls**: `Reward::InterfaceTap` 0x6E5D00 (173 `G_ClickSignpost`, 40 `G_RewardSting`, 41
    `G_OpenChest`) and `ScriptHighlight::SetActivated` 0x70A630 / `InterfaceTap` 0x70AC70 (134 `G_ClickOnScroll`, 173):
    those objects do not exist.
  - From the real `Abode::InterfaceTap` everything that is not sound is missing: [0xC4CC6C] = the tapped town
    (0x40683F), `HowManyPeople::KnockKnock` 0x829690, `Villager::SetStateWhenTappedOnAbode` 0x752B80 of the inhabitants
    (+0xA0) and the hand's animation 0x39 (`CHand::StartFixedPosAnimation` 0x46C050).
  - The menu click does not sound on the controls that are dragged (the sliders of the game's own menus): the original
    does give them code 0xA on release. Nor does keyboard activation (code 0xC).
- The citadel interior: the doors, buttons, scrolls and the scroll woosh play since step T6 of M4 (see "The temple
  interior's sounds" below). Still silent: the creature room (`CreatureRoom::DrawAdditional` 0x78869A 175
  G_FireCreatureCave at the fire's point [0xC5E454] and 0x7886E0 177 G_WaterCreatureCave 3D at (160, −45, −30), both
  options with track 0 and owner 0, every frame the room is drawn; its vfunc 11 0x7871D5 / 0x7871EC stops them) until
  the room has its creature, the woosh of `ChallengeRoom` 0x782486 (fn_00782410, a challenge picked, 46 + tick & 3,
  mode 3, 2D: openblack's challenge room has no picking yet), the heart sparks 206 + c
  (0x468815, 0x468B32: the citadel heart does not exist in openblack) and the worship strain
  (`Citadel::SetWorshipStrainSoundFrac` 0x463850). Who plays 2 / 12 with owner 0 inside the temple (what fn_00793D00
  stops) is not in the inventory **(inferred: nobody in W120; the stop stays the same)**.
- Saving `AudioMusicMasterVolume` and `AudioSampleMasterVolume` to disk, and where the slider goes.
- While paused, the original also does GSoundMap::Update and ProcessSoundTags (bit 4 is set by PauseGame); openblack
  only does AtmosProcess(0), because its pause has no turn clock.
- 0x5165BC calls IsAlive on any animated thing; openblack only checks it on villagers
  (`AnimatedThing::Villager::alive`), so a dead animal still sounds **(approximate)**.
- The double-click woosh case with the flag at [esp+0x4B] (distance 1000).
- The `OPENBLACK_SFX_TRACE` trace does not say the address of the original site.

**Checks in the game**
- The heartbeat: not tested in game.
- The voice when dropping resources (ResourceDropSFX 0x71B570): connected, not tested in game (it requires dropping
  food or wood with the hand over a town on Land 2+).
- If another island is loaded with the temple open, openblack does not call `TempleInterior::Deactivate` and
  `insideCitadel` would stay at 1.
- The real 10 s minimise next to a town, to confirm with the user.

**Known approximations** (all with their comment in the code)
- The trees' `PlayAt` without the tree as owner (the original passes the tree, 0x74C4D4).
- `audio::TickCount` = milliseconds of the process clock like `GetTickCount`; the pile reads the clock once and not
  twice (0x66D1D7 / 0x66D1E7) **(approximate)**.
- The scream when picking up a villager: alive = `LifeOf > 0` (a valid entity is taken as available) **(inferred)**.
- The collision distance comes from the `GameQueries` camera (`LH3DTech::g_camera`) **(inferred)**.
- The `SetFlight` woosh measures from the current origin of the previous frame's camera (FlyToPosFoc reads the
  camera's position, +0x118) **(inferred)**.
- `SOUND_EXISTS` = OpenAL device **(approximate)**; banks outside 1..10 and texts outside the voice table neither sound
  nor stop **(approximate)**.
- `IsInScript` (vt +0x448) always false: openblack has no script villagers **(inferred)**.
- `GameThing::IsFunctional` and `Get3DSoundPos` ≠ 1 of a tag's thing = the entity no longer has a position
  **(inferred)**.
- The point of a tag without a thing on a new start of an anim-effect: the original reads the +0x50 of the newly
  allocated channel (0x427209, before LHSamplePlay writes the point), a stale value; openblack gives the tag's point
  **(approximate**; no caller starts an anim-effect with a tag as owner).
- The channel's loops (+0x40 of `LHSampleGetInfo`) are those from the start and 0 after `ReleaseLoop` (the DLL's pass
  counter is not read) **(inferred)**.
- The street lanterns receive their tag on the `ProcessTurn` following their creation (there is no
  `CallVirtualFunctionsForCreation` hook) and none has the UNAVAILABLE mark **(approximate)**.
- `PlayFromTable` has no track argument: the site (bend 0 / rustle 1) is distinguished by the key's soundId.
- One buffer per sample record and not per wave; no RAM/8 budget.
- GAudio's working options (+0x240) with the ctor's values in each variant (nobody else writes them: **inferred**).
- `OwnerUnavailable` = `GameQueries::thingPosition` empty (the entity is not valid or has no Transform).
- `Key` owners with track do not move (the voices pass track 0); `Tag` owners follow their thing.
- When minimised, the game keeps running and the ambience keeps sounding (in the original everything stops:
  `ProcessWindowMessages` does not return while `AltTabbedAway`).
- `OnThingDeleted` exists, but Game does not call it: the entities carry a version, so `thingPosition` already gives
  empty for a destroyed one on the next turn.
- Null g_game / HelpSystem (0x429E37..0x429E4F) are not modelled: openblack always has them.
- `LHSampleIsPlaying(info)` with the audio off (0x1001407A) and the fixed axes of `LHSampleSet3DPosition` (0x10013BCC):
  see the audit of the core.
- Stereo in 3D: OpenAL does not spatialise stereo buffers.
- In the music and text code: `MusicThing` (the script's object table) and `CHAR2WCHAR`; the script camera always taken
  and the citadel as the temple interior; `GameThing::IsAvailable` and the MapCoords in float; `LHWaveIsActive`;
  openblack's turn as `g_game+0x205A40`; the render camera as GGame's; out-of-range types and alignments; group 0;
  silence on a decoding failure; the empty queue in OpenAL; the unit of `lStart` and of the play position; the
  notification inside `FlushChannel`; the marker clock in double; the marker without a label; the scaled clock in real
  ms; the click with the left button on press; the narrator number as `_wtoi`.

**Open questions**
- Music: who sets ThingMusicInfo+0x20; `LandNumber` 6 (g_game+0x205A0C is the playground flag, `GGame::Init`
  0x54F75E); whether `GetDistanceInMetres` is 2D or 3D (0x74CCB0); who calls `LHMusicPause`; group 0 (`pos[-1]`).
- Loops: N or N+1 passes; today N+1 **(inferred)**.
- Text: the values 8/5 from info.dat; GUIDE and MONK; `GRand::LocalRand(14)`: 0..13 or 0..14 (0x6DE570); the advisors
  of `SpiritHome`; the key [0xE85410]; drawing the text (HelpText fn_005CCED0).
- Not dumped: `GSoundMap::Reset` 0x71D6D0.
- Bugs in the original that are copied (to confirm with the user): the sacrificed child asks for ScriptSfx 179, which
  does not exist; GConfirmation never says "no"; MONSTER_09 is missing; STOP_SOUND_EFFECT(isSay) does not stop 0x270F;
  WELCOME_DANCE has no file; GET_MUSIC_ENUM_DISTANCE pushes twice.
- `Confirmation.h k_No` / `k_Yes`: "[0x963BA0] HELP_TEXT_ROTATION_YES_01; LocalRand(1718 [0x963C48] - 1704)" and
  "[0x963C54] ROTATION_NO_01; LocalRand(1703 [0x963CFC] - 1689): never reached". audio.md says "HelpSprites 1686
  (BETTER_14), 1673..1685 and 1704..1717". It is unclear whether 1689..1702 / 1704..1717 are HELP_TEXT ids or
  HelpSprites samples, and where 1673..1685 comes from.
- `AtmosBanks.cpp cornerA` / `cornerB`: "///< [0x10037050]" / "///< [0x10037054]", both initialised to 1. Their meaning
  is not stated.
- `qmixer::PolarRelative` azimuth: "azimuth = y < 0.0f ? 180.0f : 0.0f (0x1001239C / 0x1001238F)", "azimuth = x > 0.0f ?
  90.0f : 270.0f (0x100123C9 / 0x100123E9, x is not 0 here)", "90 − atan(y/x)" (0x10012414..0x1001242E), "90 +
  atan(|y|/x)" (0x10012444..0x10012470), "270 − atan(|y|/|x|)" (0x1001249B..), "270 + atan(y/|x|)" (0x100124EE..). Also
  "elevation = z < 0.0f ? -90.0f : 90.0f (0x1001233F / 0x10012349)", otherwise atan(z / √(x² + y²)), and range = √(z² +
  x² + y²) (0x100122D5..0x100122EB). The branch conditions are not in the notes. QMIX_POLAR: azimuth +0x20, range +0x24,
  elevation +0x28.
- `guidance::ResourceDropSample`: "return need < k_ResourceDispleased ? pick(0x1355) : 0; // the same
  (0x71B6E8..0x71B711)" and "pick(0x1358) (0x71B660..0x71B689)". The notes do not say which resource each text block
  (4949 / 4952 + LocalRand(3)) belongs to.
- `Guidance.h Attacker`: "a creature MONSTER (each 10 + LocalRand(10))". The MONSTER base id is not given.
- `guidance::LocalFloatRand`: "return 0.0f; // 0x6DE597..0x6DE5AD". The condition is not stated (probably an argument
  of 0).
- `MusicEngine::MusicEngine`: "every channel free with sad volume 127, and its QMixer channel enabled in 2D (0x110)".
  This conflicts with `EnableChannel`: "QSWaveMixEnableChannel 2D (0x100, 0x1000E3F7) or 3D (0x110/0x111 | 0x100)". The
  2D flag at init is unclear.
- `AnimationSounds::PlayFromTable`: the bend key "{c, *, *, 10, 75}". What `c` is (column 0) is not said. audio.md's
  audit of the anim-effects and SoundTag says Trees.cpp passes column 1 = 2 against the original's 0.
- `GameMusic::ProcessAlignmentMusic` `_finishedType = 0; // 0x427A6E`: whether +0x24 is cleared when the 3500 silent
  turns end or when the type changes is not stated.
- drlibs is out of the build (done): its vcpkg.json entry and the CMake `find_path` are gone, since nothing used
  it after our own decoders replaced it.
- `.github/workflows/packaging.yml` (the AUR job) still builds `dr_libs-git`, because the external AUR
  `openblack-git` PKGBUILD lists it in makedepends; it goes when that PKGBUILD drops it.
- MPEG layer III (.mp3) decodes as dr_mp3 did, bit for bit on LAME-made files, Xing/Info and LAME delay and padding
  included; nothing in the game uses it. Layer I frames go through the layer II path and are not checked against
  dr_mp3.
