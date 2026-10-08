/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <functional>
#include <optional>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

namespace openblack::audio
{
class Sound;
class SampleOutput;

/// A playing sample channel as the callers keep it: 0 = nothing plays. A handle names one start of one channel, so a
/// handle of a channel restarted since by another sample is no longer playing.
using Channel = uint32_t;
inline constexpr Channel k_NoChannel = 0;

/// The channel's owner: a pointer in the original, compared as a number. None (a tracked 3D channel then follows the
/// camera), the atmos mixer's (a tracked channel is stopped), a game thing, a sound tag (whose thing gives the
/// position), any other object with a sound position, or a plain number used as a key (the script's sample number,
/// the voice owners below: never tracked).
struct Owner
{
	enum class Kind : uint8_t
	{
		None,
		Atmos,
		Thing,
		SoundTag,
		Key,
		Object
	};
	Kind kind {Kind::None};
	entt::entity thing {entt::null};
	uint32_t id {0};

	[[nodiscard]] static Owner None() { return {}; }
	/// A game thing: its position through GameQueries::thingPosition; gone once the thing is no longer available
	[[nodiscard]] static Owner Thing(entt::entity e) { return {e == entt::null ? Kind::None : Kind::Thing, e, 0}; }
	/// The same as Thing (the name the water code's callers use)
	[[nodiscard]] static Owner Of(entt::entity e) { return Thing(e); }
	[[nodiscard]] static Owner AtmosMixer() { return {Kind::Atmos, entt::null, 0}; }
	[[nodiscard]] static Owner Tag(uint32_t tag) { return {Kind::SoundTag, entt::null, tag}; }
	[[nodiscard]] static Owner Key(uint32_t key) { return {Kind::Key, entt::null, key}; }
	/// A non-thing object with a position: audio::RegisterObject gives it
	[[nodiscard]] static Owner Object(uint32_t object) { return {Kind::Object, entt::null, object}; }
	bool operator==(const Owner& other) const { return kind == other.kind && thing == other.thing && id == other.id; }
};

/// The owner keys of the voices: the advisor's, the script's alternative voice, the one the script stops, and the 2D
/// text voice
inline constexpr uint32_t k_OwnerAdvisor = 0x270C;
inline constexpr uint32_t k_OwnerVoiceAlt = 0x270D;
inline constexpr uint32_t k_OwnerVoiceStop = 0x270E;
inline constexpr uint32_t k_OwnerVoice = 0x270F;

} // namespace openblack::audio

namespace openblack::audio::sample_play
{

/// The audio library's sample channels (channel allocation and start). The game-side filters in front of them are in
/// AudioSystem / GameSfx.
///
/// The audio system has 16 channels. A channel remembers its bank, its owner, its sample, its clone group and the
/// priority it started with. The play mode (3 by default, or the .sad's mode) picks the channel:
///  - 1: a free channel;
///  - 2: nothing at all (not even a new position) while a channel of the same bank and owner plays the same sample, or
///       one of the same clone group (> 0); else a free channel;
///  - 3: the channel of the same bank, owner and sample (restarted), else one of the same clone group (> 0) (restarted),
///       else a free channel;
///  - no free channel (and any other mode): the channel with the lowest priority, if it is lower than the sample's
///    (restarted); else the sample does not play.
/// Each channel owns one OpenAL source (SampleOutput), out of the ECS registry.
using Owner = audio::Owner;

inline constexpr size_t k_Channels = 16;

/// The play options, with the original's defaults
struct Options
{
	/// the bank's sample: openblack's "<bank>.sad/<id>" sound
	entt::id_type sound {0};
	/// set for the atmos mixer's channels, which the 3D update skips and StopAll leaves alone
	bool atmos {false};
	bool is3D {false};
	/// (default 1): UpdateChannels moves a 3D channel with its owner every turn
	bool track {true};
	/// the game's play variants copy their 6th argument here; it picks other 3D flags (unknown meaning, no effect in
	/// openblack)
	bool extra3DFlag {false};
	/// the position is relative to the listener (turned into polar coordinates)
	bool relative {false};
	Owner owner {};
	/// (default 127), 0..127
	int volume {127};
	/// the position and the offset added to it (the 3D cull measures position + offset too)
	glm::vec3 position {0.0f};
	glm::vec3 offset {0.0f};
	/// (default 100): percent of the wave's rate
	int pitch {100};
	/// 0 once, -1 for ever, N > 0 N more passes
	int loops {0};
	/// (default 3)
	int mode {3};
	/// (defaults 1, 9999, 0.3): the distance mapping; maxDistance is also the game's 3D cull when the .sad's max
	/// distance is 0
	float minDistance {1.0f};
	float maxDistance {9999.0f};
	float scale {0.3f};
	/// the fields the caller set, which the .sad does not override: 0x1 pitch, 0x20 volume, 0x40 loops, 0x80 min,
	/// 0x100 max, 0x200 scale, 0x400 mode
	uint32_t callerMask {0};
	/// the library converts the wave to PCM and keeps it in the channel for the advisor's lip-sync. Only the advisor
	/// reads that PCM, and it decodes its own copy (audio::advisor), so the flag has no effect on the channels
	bool keepPcm {false};
};

/// What the channels need from the rest of openblack. Unset members: the device output (device::Output), the sounds
/// of Locator::resources, Locator::rng, the camera of Locator::camera, and owners that are not moved.
struct Backend
{
	SampleOutput* output {nullptr};
	std::function<Sound*(entt::id_type)> sound;
	/// The library's rand(), 0..32767 (the tests give a fixed one). Unset: the library's own generator, emulated
	/// (sample_play::Rand)
	std::function<int()> rand;
	/// the listener's point (the camera), for a tracked channel without owner and the 3D culls
	std::function<std::optional<glm::vec3>()> camera;
	/// a Thing or Object owner's sound position, nullopt = gone (the channel stops); for a SoundTag owner its thing's
	/// point, nullopt = the channel keeps its point
	std::function<std::optional<glm::vec3>(const Owner&)> ownerPosition;
};

/// The backend (AudioSystem::Init, the tests); the library's generator starts again (ResetRand)
void SetBackend(Backend backend);
/// A sample's record by its sound id, through the backend (nullptr when unknown)
[[nodiscard]] Sound* GetSound(entt::id_type sound);

/// One channel as the debug panel shows it
struct ChannelInfo
{
	Channel handle {k_NoChannel};
	entt::id_type sound {0};
	uint32_t bank {0};
	Owner owner {};
	int sample {0};
	int group {0};
	int priority {0};
	int volume {0};
	int pitch {0};
	bool is3D {false};
	bool track {false};
	bool atmos {false};
	bool playing {false};
};
[[nodiscard]] std::array<ChannelInfo, k_Channels> Channels();

/// Plays a sample: the channel of the start, k_NoChannel when nothing started. In mode 2 with the sample already
/// playing, the playing channel (untouched).
Channel Start(const Options& options);

/// The first channel of (bank, owner, sample) stops with a 20 ms ramp to 0 (SampleOutput::StopRamped: the channel is
/// free at once and the fade goes on without the caller waiting, where the original waited 20 ms). Nothing while the
/// audio is switched off, unless the channel is an atmos one.
void Stop(entt::id_type sound, Owner owner);
/// On a channel the caller keeps: the first channel of its (bank, owner, sample) stops; nothing while switched off
/// unless an atmos channel
void Stop(Channel channel);
/// Every channel of the bank and owner, each with the 20 ms ramp
void StopOwner(uint32_t bank, Owner owner);
/// Every channel in use that is not an atmos one
void StopAll();
/// The first channel of (bank, owner, sample) is in use
[[nodiscard]] bool IsPlaying(entt::id_type sound, Owner owner);
/// The first channel of the bank and owner (any sample) is in use (only that first one is looked at); nothing while
/// switched off
[[nodiscard]] bool IsOwnerPlaying(uint32_t bank, Owner owner);
/// The first channel of the bank and owner (any sample): its handle when it is in use, else k_NoChannel; k_NoChannel
/// while switched off
[[nodiscard]] Channel OwnerChannel(uint32_t bank, Owner owner);
/// The volume 0..127 of the channel of a start (0 when that start is no longer on its channel)
[[nodiscard]] int Volume(Channel channel);
/// The play position in ms of the first channel of the bank and owner (the sample is not compared), -1 when it is not
/// in use or while switched off
[[nodiscard]] int64_t PlayPosition(uint32_t bank, Owner owner);
/// Position / length of the first channel of (bank, owner, sample) in use, 1 for none
[[nodiscard]] float PercentageDone(entt::id_type sound, Owner owner);
/// The channel of a start is still that start and in use
[[nodiscard]] bool IsPlaying(Channel channel);
/// The first channel of (bank, owner, sample) ends with its current pass (its remaining loops go to 0); if that first
/// one is not in use, nothing (no further search); nothing while switched off
void ReleaseLoop(entt::id_type sound, Owner owner);
/// The loops of the first channel of (bank, owner, sample), in use or not; 0 for none
[[nodiscard]] int Loops(entt::id_type sound, Owner owner);
/// The sound id of the channel of a start (0 when that start is no longer on its channel)
[[nodiscard]] entt::id_type SoundOf(Channel channel);
// ---- the library's random numbers (one generator for all of src/Audio) ----------------------------------------------
// The audio library links its own C runtime, so its sequence is neither the game's (audio::guidance::LocalRand) nor
// openblack's Locator::rng. Its callers: the pitch deviation of a start, the atmos mixer's loose samples and
// AlternatingRandom (the anim effect lists). The library seeds it with srand(time(0)): at the first start, at the first
// AlternatingRandom, at the atmos switch-on (not emulated: the bank registrations seed again before the atmos mixer
// draws) and at each registration of a bank with atmos records.

/// The library's rand(): seed = seed * 0x343FD + 0x269EC3, (seed >> 16) & 0x7FFF (0..32767); Backend::rand when the
/// tests set one
[[nodiscard]] int Rand();
/// The library's srand(time(0)) at the sites above
void SeedRand();
/// srand(time(0)) the first time; then rand() / 2, plus 0x3FFF on every other call (a flag, 1 at load, flips each
/// call), so the draws alternate between the upper half 0x3FFF..0x7FFE and the lower one 0..0x3FFF
[[nodiscard]] int AlternatingRandom();
/// AlternatingRandom() * count / 32767: 0..count - 1 (AlternatingRandom is at most 0x7FFE)
[[nodiscard]] int Random(int count);
/// The library's generator as at load (SetBackend): the seed 1 of the CRT, the flags of the first calls cleared, the
/// alternating flag = 1
void ResetRand();
/// The first channel of (bank, owner, sample) in use gets rate * percent / 100 (integers, no deviation); nothing for 0,
/// while switched off (unless an atmos channel) or when its pitch is already that
void SetPitch(entt::id_type sound, Owner owner, int percent);
/// On a channel: the first channel of its (bank, owner, sample) gets 0..127 (QMixer's law); nothing while switched off
/// (unless an atmos channel), when not in use or when it already has that volume
void SetVolume(Channel channel, int volume);
/// 0..127 (more is ignored, the same value too), re-applied to the channels in use
void SetMainVolume(int mainVolume);
[[nodiscard]] int MainVolume();

/// Once per game turn: every playing tracked 3D channel that is not an atmos one goes to its owner's sound position +
/// offset, the camera for no owner; a gone owner or the atmos one stops it, and so does a camera farther than its max
/// distance. Then the listener follows the camera (position, forward and up, once a turn).
void UpdateChannels();

/// false = StopAll and inactive, true = active again; nothing restarts
void Switch(bool on);
[[nodiscard]] bool IsActive();
/// Every channel's bank, owner and sample cleared, only while active (so the game's audio reset, which calls it while
/// switched off, clears nothing)
void ClearChannelOwners();
/// (openblack) every channel stopped and its OpenAL source deleted: a new map, the audio closing. The channels keep
/// their info, as in the original.
void ReleaseSources();
/// (openblack) the channels owned by the thing see it gone at the next UpdateChannels, as an unavailable thing does,
/// even if its entity number is reused before
void OnThingDeleted(entt::entity thing);

/// Once a frame: the device output's loop counting (QMixer counts the finite loops as it mixes; audio::UpdateFrame)
void UpdateFrame();

// ---- the water code's names (inside src/Audio and the tests only: the game calls audio::) ---------------------------
// The emitter handles are the Channel numbers as entities.

/// Start
entt::entity Play(const Options& options);
/// SetVolume on a handle of Play
void SetVolume(entt::entity emitter, int volume);
/// UpdateChannels
void ProcessTurn();
/// audio::SetGameSound
void SetGameSound(bool enabled);
/// audio::SetScriptWideScreen
void SetScriptWideScreen(bool on);
/// GameQueries::insideCitadel
[[nodiscard]] bool IsInsideCitadel();
/// GameQueries::videoPlaying
[[nodiscard]] bool IsVideoPlaying();
/// qmixer::Gain with the main volume 127 (the law test_audio_laws checks)
[[nodiscard]] float QMixerGain(int volume);
/// qmixer::DistanceGain
[[nodiscard]] float DistanceGain(float minDistance, float maxDistance, float scale, float distance);
/// qmixer::PolarRelative
[[nodiscard]] glm::vec3 PolarRelative(glm::vec3 position);
/// ReleaseSources
void Clear();

/// A Channel as the water code's entity handle and back
[[nodiscard]] inline entt::entity AsEntity(Channel channel)
{
	return channel == k_NoChannel ? entt::entity {entt::null} : static_cast<entt::entity>(channel);
}
[[nodiscard]] inline Channel AsChannel(entt::entity emitter)
{
	return emitter == entt::null ? k_NoChannel : static_cast<Channel>(emitter);
}

} // namespace openblack::audio::sample_play
