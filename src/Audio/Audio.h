/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <functional>
#include <optional>
#include <string_view>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "Audio/Engine/AnimEffects.h"
#include "Audio/Engine/SamplePlay.h"
#include "Audio/Game/AudioSystem.h"
#include "Audio/Game/BankTables.h"
#include "Audio/Services/Advisor.h"
#include "Audio/Services/Voices.h"

// The public audio API of openblack: the game includes only this header. No argument has a default: each caller passes
// what the original caller passes. Details: docs/bw1-notes/audio.md. The script's sound effects (PLAY /
// STOP_SOUND_EFFECT, GAME_SOUND_PLAYING, ATTACH / DETACH_SOUND_TAG) are in ScriptSound.h.
//
// Rules for the callers:
//  - nobody outside src/Audio calls OpenAL; every sample plays on the 16 channels through this header (the old
//    emitters of AudioManager, CreateEmitter / PlayEmitter / PlaySound / PlayMusic, and the AudioEmitter component are
//    gone), and there is one engine: one device (Device.h, the only caller of OpenAL), one bank loader
//    (Banks.h), no AudioManager and no Locator::audio;
//  - the audio includes no ECS component: the positions of the owners come from GameQueries (things) and from
//    RegisterObject (other objects). It does use the shared plain maths and clocks: map_coords (MapCoords.h),
//    gutils (GUtilsDistance.h), game_clock (GameClock.h) and sky_type (3D/SkyType.h).

namespace openblack::map_coords
{
struct MapCoords;
}

namespace openblack::audio
{
struct GameQueries;

// ---- banks and samples ----------------------------------------------------------------------------------------------
// SfxBank (BankTables.h) is the game's sound effect bank type; BankId any registered bank (AudioSystem.h): Bank(SfxBank),
// FindBank(path), SampleId(bank, number).

/// A sample: a bank and its 1-based number in the .sad
struct Sample
{
	BankId bank {k_NoBank};
	int number {0};
};

/// The bank of a creature species ("audio\sfx\creature\%s.sad", registered when the creature loads): k_NoBank when the
/// species has no bank
[[nodiscard]] BankId CreatureBank(std::string_view species);

/// The sample's max distance as in the .sad (raw), 0 for no bank
[[nodiscard]] float MaxDistance(Sample sample);

/// (openblack: the original addresses samples by number) the sample of `bank` whose .sad name is `wavName` (e.g.
/// "G_PickUpFood.wav", case ignored); nullopt when the bank has none
[[nodiscard]] std::optional<Sample> FindSample(BankId bank, std::string_view wavName);

// ---- owners --------------------------------------------------------------------------------------------------------
// Owner, k_OwnerAdvisor .. k_OwnerVoice (SamplePlay.h).

/// The sound position of an owner that is not a game thing (the particle sounds, the hand, the fire...): nullopt = gone
/// (a tracked channel stops)
using ObjectPositionFn = std::function<std::optional<glm::vec3>()>;
/// Owner::Object(id) gets its position from `position` (UpdateChannels, once a turn)
void RegisterObject(uint32_t id, ObjectPositionFn position);
void UnregisterObject(uint32_t id);
/// (openblack) A new id for Owner::Object, never given before: the original compares the owners' pointers (the particle
/// sounds, the fire effect, the hand effects, the gesture's atom data...), openblack gives each such object a number of
/// its own. An id whose channels are never tracked (track 0: the hand effects, the fire's steam, the gesture's atom data)
/// needs no RegisterObject: UpdateChannels only asks the tracked ones for their point.
[[nodiscard]] uint32_t NewObjectId();

// ---- PlaySoundEffect and its family ---------------------------------------------------------------------------------

/// The play options of a sample of a bank (the options variant of PlaySoundEffect: the guidance, the particle sounds
/// and the sound tags fill their own)
struct PlayOptions: sample_play::Options
{
	Sample sample;
};
/// Plays with options (the filters are documented in AudioSystem.h). Returns the channel (the original returns
/// nothing).
Channel PlaySoundEffect(const PlayOptions& options);

/// A 3D one without owner or with an unavailable owner plays nothing; at the owner's sound position, else at (0, 0, 0)
/// for a 2D one; then PlaySoundEffectAt
Channel PlaySoundEffect(Owner owner, int sample, int mode, int loops, bool extra3DFlag, bool is3D, SfxBank bank);
/// The same, with a bank
Channel PlaySoundEffect(Owner owner, int sample, int mode, int loops, bool extra3DFlag, bool is3D, BankId bank);
/// Nothing for sample 0; the options get the bank, owner, sample, extra3DFlag, is3D and track = is3D, the point, no offset,
/// the loops and the mode; a 3D one of an unavailable owner plays nothing; then the options variant
Channel PlaySoundEffectAt(Owner owner, glm::vec3 position, int sample, int mode, int loops, bool extra3DFlag, bool is3D,
                          SfxBank bank);
/// The same, with a bank
Channel PlaySoundEffectAt(Owner owner, glm::vec3 position, int sample, int mode, int loops, bool extra3DFlag, bool is3D,
                          BankId bank);
/// The sound tags' variant: like the one above with an offset and a track of its own; the unavailable owner test only
/// for is3D and track. (The 8th argument is extra3DFlag and the 7th the loops.)
Channel PlaySoundEffectAt(Owner owner, glm::vec3 position, glm::vec3 offset, int sample, bool track, int mode, int loops,
                          bool extra3DFlag, bool is3D, BankId bank);

/// The first channel of (bank, owner, sample) stops; sample 0 = every channel of the owner in the bank
void StopSoundEffect(int sample, Owner owner, SfxBank bank);
void StopSoundEffect(int sample, Owner owner, BankId bank);
/// sample_play::StopAll (not the atmos channels)
void StopAllSoundEffects();
/// sample_play::ReleaseLoop
void ReleaseLoop(Owner owner, int sample, SfxBank bank);
void ReleaseLoop(Owner owner, int sample, BankId bank);
/// The first channel of (bank, owner, sample) is in use
[[nodiscard]] bool IsPlaying(Owner owner, int sample, SfxBank bank);
[[nodiscard]] bool IsPlaying(Owner owner, int sample, BankId bank);
/// Any sample of the owner
[[nodiscard]] bool IsPlaying(Owner owner, SfxBank bank);
/// sample_play::SetPitch
void SetPitch(BankId bank, Owner owner, int sample, int percent);
/// sample_play::SetVolume on a channel the caller keeps (the atmos mixer, the particle sounds)
void SetVolume(Channel channel, int volume);
/// sample_play::IsPlaying on a channel the caller keeps
[[nodiscard]] bool IsPlaying(Channel channel);
/// Called straight by the particle sounds: the first channel of the bank and owner (any sample) when it is in use, else
/// k_NoChannel (also while switched off)
[[nodiscard]] Channel PlayingChannel(Owner owner, BankId bank);
/// The volume 0..127 of a channel the caller got (the particle sounds' fade), 0 for none
[[nodiscard]] int Volume(Channel channel);

/// The global cyclic counters some callers add to a first sample: the sample is base + counter, then the counter goes
/// up and back to 0 at its count; the tree mulch adds first and masks with 3
enum class Counter : uint8_t
{
	KnockRoof,          ///< 0..8: G_KnockRoofMulti 110 + c (tapping an abode)
	CitadelSparkEffect, ///< 0..4: G_CitadelSpark 206 + c (the citadel's spark effect)
	CitadelSparkDamage, ///< 0..4: G_CitadelSpark 206 + c (the citadel heart's transferred damage)
	CreatureRockTap,    ///< 0..3: G_RockTap 139 + c (the creature)
	CreatureSquash,     ///< 0..2: G_SquashAnimal 143 + c (the creature)
	HandInWater,        ///< 0..9: G_HandInWater 99 + c
	TreeMulch,          ///< (c + 1) & 3: G_TreeMulch 155 + c (an object deleted for its resource)
	RockTap,            ///< 0..3: G_RockTap 130 + c (tapping a rock)
	ScaffoldCombine,    ///< 0..3: G_ScaffoldCombine 201 + c
	ScaffoldTap,        ///< 0..3: G_ScaffoldTap 151 + c

	_Count
};
/// The counter's value to add to the first sample, advancing it
[[nodiscard]] int NextCounter(Counter counter);

/// GetTickCount() of the original (milliseconds of the process's clock, wrapping at 2^32): the callers that pick a sample
/// with it instead of a random generator (a tree's drop: 83 + t % 3, a tree's water spell: 120 + t % 9, a physics
/// object's turn: 69 + t % 5, the camera's fly to a position: 46 + (t & 3), ...)
[[nodiscard]] uint32_t TickCount();

/// OPENBLACK_SFX_TRACE=1: one "SFX:" line in the log for each call of the family above, of PlayAnimationEffect and of
/// the tags' plays (bank / sample, 2D or 3D, the point, the mode and pitch it starts with, the owner and what came of it)
[[nodiscard]] bool SfxTrace();

// ---- the script's switches and the game's state filters -------------------------------------------------------------
// SetGameSound (SET_GAME_SOUND), SetScriptWideScreen, IsInsideCitadel, IsVideoPlaying: AudioSystem.h. The citadel and
// the interface states come from GameQueries::insideCitadel / interfaceState.

// ---- the music engine, for callers outside the game audio ------------------------------------------------------------

/// Stops every music channel: fade 1 sets every target to 0 (they fade out at -3 per 120 ms pass), anything else cuts
/// them at once. Its game callers: the falling spell at 43.9 s of fall.bik (fade 1) and the end of the pre-intro video
/// (fade 0). The script music keeps its own state (StartScriptMusic is not reset, as in the original)
void MusicStop(int fade);

// ---- the citadel -----------------------------------------------------------------------------------------------------
// GameQueries::insideCitadel (set when going inside the citadel, cleared when leaving).
// Entering has no audio call of its own; the music's citadel step stops every sample the first turn inside and plays
// citadel.sad.

/// The audio part of leaving the citadel (the temple's engine going): the stop of G_Fire_01 (2) and of G_WaterFlow (12),
/// owner none, in the in-game bank
void LeaveCitadel();

// ---- life cycle -------------------------------------------------------------------------------------------------------

/// The audio system's creation (after the 11 banks, which Game registers as it reads the .sad): the sample main volume
/// of the configuration (AudioSampleMasterVolume), the queries, the channels' backend
void Init(GameQueries queries);
/// The original would save the main volumes here (openblack has no settings file for them yet); the channels stop
/// and their sources go before the OpenAL context
void Shutdown();
/// The game's end of turn, unpaused: the sound map, the sound tags (the street lanterns' too), then after turn 5 the
/// audio game turn, else the atmos mixer without processing. The audio game turn, only while the audio is active: the
/// music, the atmos targets and the atmos banks, UpdateChannels (with the listener) and the atmos mixer unless a video
/// plays; always the purge of the things' music info. The turn is game_clock::Turn() and the sky type the sound map
/// reads is sky_type::Frame() (the last frame's, written by the sky drawing).
void ProcessTurn();
/// The temple's own turn while the game is paused inside the citadel: the audio game turn alone (the citadel's music,
/// the atmos, the channels and the listener), with no sound map, no sound tags and no turn 5 gate
void ProcessCitadelTurn();
/// The game's end of turn while paused: the atmos mixer without processing. (Pending: the paused end of turn of the
/// original also updates the sound map and the sound tags before that test; openblack's pause has no turn clock, Game
/// calls this once a frame, so they do not run while paused.)
void Paused();
/// Once a frame: the sample main volume of the configuration applied live (as the options dialog does) and the channels'
/// finite loops
void UpdateFrame();
/// A game thing deleted: the channels it owns see it unavailable at the next turn.
/// Game does not need to call it today: the entity handles are versioned, so GameQueries::thingPosition already answers
/// nullopt for a destroyed thing (the channel stops). It is for an owner that stays valid in the registry while the game
/// treats it as gone.
void OnThingDeleted(entt::entity thing);
/// The game's map clear and init reset the audio (call before the registry reset): the music groups and the game audio's
/// state cleared; the music stopped, the atmos mixer without processing, every sample stopped, the audio switched off,
/// the wait for no channel playing, ClearChannelOwners (a no-op while switched off), the audio switched on; the things' music
/// info released. Also the map's sound tags (deleted with the objects of the map clear) and, in openblack, the
/// channels' OpenAL sources.
void ClearMap();
/// The window's activation callback (deactivated when the window is minimised, reactivated when it is restored: Game
/// maps them to SDL's MINIMIZED / RESTORED, not to the focus): off = the samples stopped and inactive, the music stopped
/// and inactive; on = both active again, nothing restarts
void OnFocus(bool active);
/// sample_play::SetMainVolume (0..127) through the configuration's AudioSampleMasterVolume (the options slider:
/// slider * 127, truncated)
void SetSampleMainVolume(int volume);
/// sample_play::MainVolume
[[nodiscard]] int SampleMainVolume();

// ---- anim effects and SoundTags -------------------------------------------------------------------------------------

/// Anim effects (AnimKey and AnimAction: AnimEffects.h). `distance` is what the caller measured (the camera's distance
/// to the object, e.g. a tree's drawing) and gates the play (k_MaxDistance and the sample's max distance); the point is
/// the owner's. Play: the anim effect's row (no row, nothing), the filters of the sample's user parameter (1 not while a
/// script holds the wide screen, only 2 inside the citadel, the banks after SET_GAME_SOUND false, not 4 in the
/// interface states 0x10 / 0x16 / 0x17) and, when tracking, an unavailable owner; then the row's play with min / max
/// (> 0 overrides the .sad's). Stop / Release: straight to the row's samples, without filters. Returns the channel of a
/// play.
Channel PlayAnimationEffect(Owner owner, float distance, const AnimKey& key, AnimAction action, BankId bank, bool track,
                            float minDistance, float maxDistance);

/// Sound tags: a sample tied to a thing or a point, kept in a list (newest first) and processed once a
/// turn from the game's end of turn. The channel's owner is the tag itself. A tag of a thing (re)plays through
/// PlaySoundEffectAt every turn while active, so its play mode decides (2: nothing while it plays; 3: restarts); one
/// whose thing is gone becomes a tag of the dead object. A tag of a point plays once and goes when its sample stops; a
/// 3D one with a delay waits for the sound to reach the camera at 347 per second.
namespace tags
{
using TagId = uint32_t;
inline constexpr TagId k_NoTag = 0;
/// A tag of a thing: no offset; the tag's point is the thing's; the delay is kept only when is3D
TagId Create(entt::entity thing, int sample, bool track, int mode, int loops, bool extra3DFlag, bool is3D, SfxBank bank,
             int delay);
/// The same with an offset added by the channel (the street lantern: (0, the object's height, 0))
TagId Create(entt::entity thing, glm::vec3 offset, int sample, bool track, int mode, int loops, bool extra3DFlag, bool is3D,
             SfxBank bank, int delay);
/// A point tag (track ignored); it plays at once through PlaySoundEffect (owner the tag, track 0) unless it is 3D with
/// a delay
TagId Create(glm::vec3 point, int sample, bool track, int mode, int loops, bool extra3DFlag, bool is3D, SfxBank bank,
             int delay);
/// The point (x, the land's altitude + the height above the land, z) (the altitude from GameQueries::landAltitude), x and
/// z scaled by 10 / 65536 (map_coords::ToMetres), then the point tag.
/// (Not ported: the same point tag in an atmos bank, played at once unless 3D with a delay; its only caller is the
/// weather's thunder: sample 2 + GetTickCount() % 11, mode 2, 3D, type 12, delay 1. Pending with the weather's thunder.)
TagId CreateAtMapCoords(const map_coords::MapCoords& coords, int sample, bool track, int mode, int loops, bool extra3DFlag,
                        bool is3D, SfxBank bank, int delay);
/// The same for a MapCoords its caller already holds in metres (x, z = ToMetres of the 16.16 values, the altitude
/// above the land: magic::ToMap's map positions); x and z are not quantised again
TagId CreateAtMapCoords(float x, float z, float heightAboveLand, int sample, bool track, int mode, int loops, bool extra3DFlag,
                        bool is3D, SfxBank bank, int delay);
/// An active tag turned off stops its sample
void SetActive(TagId tag, bool active);
/// Every tag of the three is deleted (as Delete: a playing loop is released first)
void Remove(entt::entity thing, int sample, SfxBank bank);
/// The same, stopping the sample first when `stop`
void Remove(entt::entity thing, int sample, SfxBank bank, bool stop);
/// The thing forgotten; a sample that plays with loops has its loop released and the tag lives on until it stops;
/// otherwise the tag goes at once (its sample, if any, plays on)
void Delete(TagId tag);
/// first + the game's local random number below count (0 for count 0)
[[nodiscard]] int RandomSample(int first, int count);
/// The tag still exists (it has not gone in ProcessSoundTags or Delete)
[[nodiscard]] bool Exists(TagId tag);
} // namespace tags

/// SOUND_EXISTS: the wave device was made.
/// (approximate) openblack's: the audio is initialised on a real OpenAL device (device::Open succeeded).
[[nodiscard]] bool SoundExists();

// ---- voices ---------------------------------------------------------------------------------------------------------
// The voices of the texts and of the script (RUN_TEXT, SAY_SOUND, SAY_SOUND_EFFECT_PLAYING, the click's cut): Voices.h,
// audio::voices. The advisors (owner 0x270C, the lip-sync): Advisor.h, audio::advisor. STOP_SOUND_EFFECT with isSay:
// ScriptSound.h.

// The music has its own headers: MusicEngine.h / MusicStream.h (the music engine), GameMusic.h (the
// game's music, the script's music, the main volume), ThingMusic.h (the things' music info).

} // namespace openblack::audio
