/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SamplePlay.h"

#include <cstdlib>
#include <ctime>

#include <algorithm>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "Audio/Device/Device.h"
#include "Audio/Device/SampleOutput.h"
#include "Audio/Device/Sound.h"
#include "Audio/Engine/QMixerLaws.h"
#include "Audio/Game/AudioSystem.h"
#include "Camera/Camera.h"
#include "Debug/DebugEnv.h"
#include "ECS/Systems/AudioStateInterface.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::audio;
using namespace openblack::audio::sample_play;

namespace
{
/// One sample channel's info
struct ChannelState
{
	Channel handle {k_NoChannel}; ///< (openblack) the start the channel is on; 0 = never started
	entt::id_type sound {0};
	uint32_t bank {0};
	Owner owner {};
	int sample {0};
	int group {0};
	int pitch {100};
	int volume {127};
	bool atmos {false};
	bool is3D {false};
	bool track {false};
	uint32_t overrides {0}; ///< the sample's .sad overrides (0x01 pitch, 0x20 volume, ...)
	int loops {0};          ///< the loops it started with, 0 after ReleaseLoop (inferred: the library's pass count is not read)
	glm::vec3 position {};  ///< the point the owner gave (without the offset)
	glm::vec3 offset {};
	float maxDistance {0};
	int priority {0};
	bool ownerGone {false};
};

/// The library's generator (sample_play::Rand): the CRT seed (the game thread's, 1 at start) and the flags of its first
/// calls
struct LibraryRandomState
{
	uint32_t seed {1};
	bool playSeeded {false};
	bool systemSeeded {false};
	bool upperHalf {true}; ///< 1 in the library's data
};

struct State
{
	std::array<ChannelState, k_Channels> channels;
	uint32_t serial {0};
	/// 127 by default
	int mainVolume {qmixer::k_MaxVolume};
	/// on once the audio system is created
	bool active {true};
	Backend backend;
};
/// What this module keeps between calls (Locator::audioState)
struct SamplePlayState
{
	LibraryRandomState rand {};
	State play {};
};

SamplePlayState& SamplePlayData()
{
	return openblack::Locator::audioState::value().Get<SamplePlayState>();
}

bool Trace()
{
	static const bool k_Trace = debug_env::AudioTrace();
	return k_Trace;
}

SampleOutput* Output()
{
	auto& state = SamplePlayData();
	if (state.play.backend.output != nullptr)
	{
		return state.play.backend.output;
	}
	return device::Output();
}

Sound* Lookup(entt::id_type id)
{
	auto& state = SamplePlayData();
	if (state.play.backend.sound)
	{
		return state.play.backend.sound(id);
	}
	if (!Locator::resources::has_value())
	{
		return nullptr;
	}
	auto& sounds = Locator::resources::value().GetSounds();
	if (!sounds.Contains(id))
	{
		return nullptr;
	}
	auto handle = sounds.Handle(id);
	return handle ? &*handle : nullptr;
}

std::optional<glm::vec3> CameraPosition()
{
	auto& state = SamplePlayData();
	if (state.play.backend.camera)
	{
		return state.play.backend.camera();
	}
	if (!Locator::camera::has_value())
	{
		return std::nullopt;
	}
	return Locator::camera::value().GetOrigin();
}

size_t IndexOf(const ChannelState& channel)
{
	return static_cast<size_t>(&channel - SamplePlayData().play.channels.data());
}

/// The channel is in use (a sample is on it and has not finished)
bool InUse(const ChannelState& channel)
{
	const auto* output = Output();
	return channel.handle != k_NoChannel && output != nullptr && output->Playing(IndexOf(channel));
}

ChannelState* Find(Channel handle)
{
	if (handle == k_NoChannel)
	{
		return nullptr;
	}
	auto& channel = SamplePlayData().play.channels[(handle - 1) % k_Channels];
	return channel.handle == handle ? &channel : nullptr;
}

const char* OwnerName(const Owner& owner)
{
	switch (owner.kind)
	{
	case Owner::Kind::None:
		return "none";
	case Owner::Kind::Atmos:
		return "atmos";
	case Owner::Kind::SoundTag:
		return "sound tag";
	case Owner::Kind::Key:
		return "key";
	case Owner::Kind::Object:
		return "object";
	default:
		return "thing";
	}
}

/// `ramped`: Stop's 20 ms ramp to silence before the flush (SampleOutput::StopRamped: the channel is free at once, the
/// fade goes on without the caller waiting); StopAll flushes at once
void Halt(ChannelState& channel, const char* why, bool ramped)
{
	if (InUse(channel))
	{
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Sample play: channel {} stopped ({})", IndexOf(channel), why);
		}
		if (ramped)
		{
			Output()->StopRamped(IndexOf(channel));
		}
		else
		{
			Output()->Stop(IndexOf(channel));
		}
	}
}

/// The first channel of (bank, owner, sample), in use or not (Loops, Stop, ReleaseLoop, SetPitch)
ChannelState* First(uint32_t bank, const Owner& owner, int sample)
{
	for (auto& channel : SamplePlayData().play.channels)
	{
		if (channel.handle != k_NoChannel && channel.bank == bank && channel.owner == owner && channel.sample == sample)
		{
			return &channel;
		}
	}
	return nullptr;
}

/// The channel allocation of a start. `restart`: the channel was in use and is restarted; `leave`: mode 2 found its
/// sample playing, nothing is started
ChannelState* Allocate(const Options& options, const Sound& sound, uint32_t bank, int mode, bool& restart, bool& leave)
{
	restart = false;
	leave = false;
	auto& channels = SamplePlayData().play.channels;
	const int group = sound.cloneGroup;
	const auto firstFree = [&channels]() -> ChannelState* {
		for (auto& channel : channels)
		{
			if (!InUse(channel))
			{
				return &channel;
			}
		}
		return nullptr;
	};
	ChannelState* found = nullptr;
	switch (mode)
	{
	case 1:
		found = firstFree();
		break;
	case 2:
		for (auto& channel : channels)
		{
			if (channel.handle != k_NoChannel && channel.bank == bank && channel.owner == options.owner &&
			    (channel.sample == sound.id || (channel.group == group && group > 0)) && InUse(channel))
			{
				leave = true;
				return &channel;
			}
		}
		found = firstFree();
		break;
	case 3:
		// the same sample of the same bank and owner, playing or not
		for (auto& channel : channels)
		{
			if (channel.handle != k_NoChannel && channel.bank == bank && channel.owner == options.owner &&
			    channel.sample == sound.id)
			{
				found = &channel;
				restart = true;
				break;
			}
		}
		// the options' name is "NONE" (the default): the clone group
		if (found == nullptr && group > 0)
		{
			for (auto& channel : channels)
			{
				if (channel.handle != k_NoChannel && channel.bank == bank && channel.owner == options.owner &&
				    channel.group == group)
				{
					found = &channel;
					restart = true;
					break;
				}
			}
		}
		if (found == nullptr)
		{
			found = firstFree();
			restart = false;
		}
		break;
	default:
		break;
	}
	if (found != nullptr)
	{
		restart = restart && InUse(*found);
		return found;
	}
	// no free channel: the lowest priority (a channel at 0 at once), if lower than the sample's
	ChannelState* lowest = nullptr;
	uint32_t lowestPriority = 0xFFFFFFFFu;
	for (auto& channel : channels)
	{
		const auto priority = InUse(channel) ? static_cast<uint32_t>(channel.priority) : 0u;
		if (priority == 0)
		{
			lowest = &channel;
			lowestPriority = 0;
			break;
		}
		if (priority < lowestPriority)
		{
			lowest = &channel;
			lowestPriority = priority;
		}
	}
	if (lowest != nullptr && lowestPriority < static_cast<uint32_t>(sound.priority))
	{
		restart = InUse(*lowest);
		return lowest;
	}
	return nullptr;
}

Channel NextHandle(size_t index)
{
	auto& state = SamplePlayData();
	// serial * 16 + index + 1, never 0 and never entt::null as an entity
	++state.play.serial;
	if (state.play.serial >= 0x0FFFFFFFu)
	{
		state.play.serial = 1;
	}
	return state.play.serial * static_cast<Channel>(k_Channels) + static_cast<Channel>(index) + 1;
}
} // namespace

void sample_play::SetBackend(Backend backend)
{
	SamplePlayData().play.backend = std::move(backend);
	// (openblack) a new backend is a new audio system: the library's generator as at load (the tests start from it)
	ResetRand();
}

Sound* sample_play::GetSound(entt::id_type sound)
{
	return Lookup(sound);
}

std::array<ChannelInfo, k_Channels> sample_play::Channels()
{
	std::array<ChannelInfo, k_Channels> infos {};
	for (size_t i = 0; i < k_Channels; ++i)
	{
		const auto& channel = SamplePlayData().play.channels[i];
		auto& info = infos[i];
		info.handle = channel.handle;
		info.sound = channel.sound;
		info.bank = channel.bank;
		info.owner = channel.owner;
		info.sample = channel.sample;
		info.group = channel.group;
		info.priority = channel.priority;
		info.volume = channel.volume;
		info.pitch = channel.pitch;
		info.is3D = channel.is3D;
		info.track = channel.track;
		info.atmos = channel.atmos;
		info.playing = InUse(channel);
	}
	return infos;
}

Channel sample_play::Start(const Options& options)
{
	auto& state = SamplePlayData();
	// srand(time(0)) at the first play
	if (!state.rand.playSeeded)
	{
		SeedRand();
		state.rand.playSeeded = true;
	}
	auto* output = Output();
	auto* sound = Lookup(options.sound);
	if (output == nullptr || sound == nullptr)
	{
		return k_NoChannel;
	}
	const uint32_t bank = sound->bank;
	const uint32_t flags = sound->overrides;
	// the .sad's fields where its flag is set and the caller did not set them
	const auto fromSad = [flags, &options](uint32_t bit) { return (flags & bit) != 0 && (options.callerMask & bit) == 0; };
	const int mode = fromSad(0x400) ? sound->playMode : options.mode;
	bool restart = false;
	bool leave = false;
	ChannelState* channel = Allocate(options, *sound, bank, mode, restart, leave);
	if (channel == nullptr)
	{
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Sample play: {} (mode {}, owner {}): no channel", sound->name, mode,
			                   OwnerName(options.owner));
		}
		return k_NoChannel;
	}
	if (leave)
	{
		// mode 2: the playing channel is returned untouched
		return channel->handle;
	}
	const size_t index = IndexOf(*channel);
	if (restart)
	{
		Halt(*channel, "restarted", false);
	}

	const int volume = std::clamp(fromSad(0x20) ? sound->volume127 : options.volume, 0, qmixer::k_MaxVolume);
	const int loops = fromSad(0x40) ? sound->loops : options.loops;
	// the pitch: the .sad's with flag 0x1, else the options', with the deviation
	const int pitch =
	    qmixer::StartPitch(fromSad(0x1) ? sound->pitch : options.pitch, sound->pitchDeviation, sample_play::Rand());
	// the distance mapping {min, max, scale}: the .sad's with flags 0x80 / 0x100 / 0x200
	const auto startMinDistance = fromSad(0x80) ? sound->minDistance : options.minDistance;
	const auto startMaxDistance = fromSad(0x100) ? sound->mappingMaxDistance : options.maxDistance;
	const auto startScale = fromSad(0x200) ? sound->scale : options.scale;
	const auto startGain = qmixer::Gain(volume, state.play.mainVolume);
	const auto startPitch = qmixer::FrequencyRatio(sound->sampleRate, pitch);
	SampleOutput::Start start {
	    .gain = startGain,
	    .pitch = startPitch,
	    .is3D = options.is3D,
	    .minDistance = startMinDistance,
	    .maxDistance = startMaxDistance,
	    .scale = startScale,
	    .loops = loops,
	};
	// the source position is pos + offset; a relative one is heard in the listener's frame: AL's (right,
	// up, -ahead), which the output receives as (z, y, x)
	glm::vec3 position = options.position + options.offset;
	if (options.is3D && options.relative)
	{
		const auto p = qmixer::PolarRelative(position);
		position = glm::vec3(-p.z, p.y, p.x);
	}
	start.relative = options.relative;
	start.position = options.is3D ? position : glm::vec3(0.0f);
	if (!output->Play(index, *sound, start))
	{
		channel->handle = k_NoChannel;
		return k_NoChannel;
	}

	channel->handle = NextHandle(index);
	channel->sound = options.sound;
	channel->bank = bank;
	channel->owner = options.owner;
	channel->sample = sound->id;
	channel->overrides = sound->overrides;
	channel->volume = volume;
	channel->pitch = pitch;
	channel->atmos = options.atmos;
	channel->is3D = options.is3D;
	channel->track = options.track;
	channel->loops = loops;
	channel->position = options.position;
	channel->offset = options.offset;
	channel->maxDistance = start.maxDistance;
	channel->ownerGone = false;
	// the priority and clone group of the sample (the options' name is "NONE")
	channel->priority = sound->priority;
	channel->group = sound->cloneGroup;
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"),
		                   "Sample play: {} mode {} owner {} channel {}{}, volume {} -> gain {:.4f} (master {}), pitch {}, "
		                   "loops {}, {} at ({:.2f}, {:.2f}, {:.2f}){}",
		                   sound->name, mode, OwnerName(options.owner), index, restart ? " (restarted)" : "", volume,
		                   start.gain, state.play.mainVolume, pitch, loops, options.is3D ? "3D" : "2D", options.position.x,
		                   options.position.y, options.position.z, options.relative ? " relative" : "");
	}
	return channel->handle;
}

void sample_play::Stop(entt::id_type sound, Owner owner)
{
	const auto* info = Lookup(sound);
	if (info == nullptr)
	{
		return;
	}
	auto* channel = First(info->bank, owner, info->id);
	// nothing while switched off, unless an atmos channel
	if (channel == nullptr || (!SamplePlayData().play.active && !channel->atmos))
	{
		return;
	}
	Halt(*channel, "LHSampleStop", true);
}

void sample_play::Stop(Channel handle)
{
	const auto* started = Find(handle);
	if (started == nullptr || (!SamplePlayData().play.active && !started->atmos))
	{
		return;
	}
	if (auto* channel = First(started->bank, started->owner, started->sample); channel != nullptr)
	{
		Halt(*channel, "LHSampleStop(info)", true);
	}
}

void sample_play::StopOwner(uint32_t bank, Owner owner)
{
	auto& state = SamplePlayData();
	for (auto& channel : state.play.channels)
	{
		if (channel.handle == k_NoChannel || channel.bank != bank || !(channel.owner == owner))
		{
			continue;
		}
		// the first one met while switched off ends the whole stop
		if (!state.play.active && !channel.atmos)
		{
			return;
		}
		Halt(channel, "LHSampleStop, any sample", true);
	}
}

void sample_play::StopAll()
{
	for (auto& channel : SamplePlayData().play.channels)
	{
		if (!channel.atmos)
		{
			Halt(channel, "LHSampleStopAll", false);
			// a ramped stop of this channel still fading out is cut too, as if its 20 ms had passed
			if (auto* output = Output(); output != nullptr)
			{
				output->CutFade(IndexOf(channel));
			}
		}
	}
}

bool sample_play::IsPlaying(entt::id_type sound, Owner owner)
{
	const auto* info = Lookup(sound);
	if (info == nullptr || !SamplePlayData().play.active)
	{
		return false;
	}
	const auto* channel = First(info->bank, owner, info->id);
	return channel != nullptr && InUse(*channel);
}

bool sample_play::IsOwnerPlaying(uint32_t bank, Owner owner)
{
	auto& state = SamplePlayData();
	if (!state.play.active)
	{
		return false;
	}
	for (const auto& channel : state.play.channels)
	{
		if (channel.handle != k_NoChannel && channel.bank == bank && channel.owner == owner)
		{
			return InUse(channel);
		}
	}
	return false;
}

Channel sample_play::OwnerChannel(uint32_t bank, Owner owner)
{
	auto& state = SamplePlayData();
	// nothing while switched off; the first channel of the bank and owner, whatever its sample; its handle when in use
	if (!state.play.active)
	{
		return k_NoChannel;
	}
	for (const auto& channel : state.play.channels)
	{
		if (channel.handle != k_NoChannel && channel.bank == bank && channel.owner == owner)
		{
			return InUse(channel) ? channel.handle : k_NoChannel;
		}
	}
	return k_NoChannel;
}

int sample_play::Volume(Channel handle)
{
	const auto* channel = Find(handle);
	return channel != nullptr ? channel->volume : 0;
}

int64_t sample_play::PlayPosition(uint32_t bank, Owner owner)
{
	auto& state = SamplePlayData();
	// -1 while switched off; the first channel of the bank and owner, whatever its sample; in use -> its play position
	// in ms, else -1
	if (!state.play.active)
	{
		return -1;
	}
	for (const auto& channel : state.play.channels)
	{
		if (channel.handle != k_NoChannel && channel.bank == bank && channel.owner == owner)
		{
			return InUse(channel) ? Output()->PlayPositionMs(IndexOf(channel)) : -1;
		}
	}
	return -1;
}

float sample_play::PercentageDone(entt::id_type sound, Owner owner)
{
	auto& state = SamplePlayData();
	// 1 when nothing matches; every channel of (bank, owner, sample) in turn; one met while switched off that is not an
	// atmos one ends it with 1; the first in use gives position / length: the play position in ms over the wave's
	// bytes * 1000 / (channels * rate * 2)
	const auto* info = Lookup(sound);
	if (info == nullptr)
	{
		return 1.0f;
	}
	for (const auto& channel : state.play.channels)
	{
		if (channel.handle == k_NoChannel || channel.bank != info->bank || !(channel.owner == owner) ||
		    channel.sample != info->id)
		{
			continue;
		}
		if (!state.play.active && !channel.atmos)
		{
			return 1.0f;
		}
		if (!InUse(channel))
		{
			continue;
		}
		const int64_t position = Output()->PlayPositionMs(IndexOf(channel));
		const auto lengthMs = static_cast<int64_t>(info->duration * 1000.0f);
		if (position < 0 || lengthMs <= 0)
		{
			continue;
		}
		return static_cast<float>(position) / static_cast<float>(lengthMs);
	}
	return 1.0f;
}

bool sample_play::IsPlaying(Channel handle)
{
	// The original answers false while switched off and looks at the first channel of the start's (bank, owner,
	// sample). (approximate) Here: the handle's own start, also while switched off: openblack's game runs on while
	// minimised (the original stops), and a caller asking every turn (the particle sounds) would start its sample again
	// on another channel each turn.
	const auto* channel = Find(handle);
	return channel != nullptr && InUse(*channel);
}

void sample_play::ReleaseLoop(entt::id_type sound, Owner owner)
{
	const auto* info = Lookup(sound);
	if (info == nullptr || !SamplePlayData().play.active)
	{
		return;
	}
	auto* channel = First(info->bank, owner, info->id);
	if (channel == nullptr || !InUse(*channel))
	{
		return;
	}
	Output()->ReleaseLoop(IndexOf(*channel));
	channel->loops = 0;
}

int sample_play::Loops(entt::id_type sound, Owner owner)
{
	// the first channel of the three, in use or not
	const auto* info = Lookup(sound);
	if (info == nullptr)
	{
		return 0;
	}
	const auto* channel = First(info->bank, owner, info->id);
	return channel != nullptr ? channel->loops : 0;
}

entt::id_type sample_play::SoundOf(Channel handle)
{
	const auto* channel = Find(handle);
	return channel != nullptr ? channel->sound : 0;
}

int sample_play::Rand()
{
	auto& state = SamplePlayData();
	if (state.play.backend.rand)
	{
		return state.play.backend.rand();
	}
	// MSVC's rand
	state.rand.seed = state.rand.seed * 0x343FDu + 0x269EC3u;
	return static_cast<int>((state.rand.seed >> 16) & 0x7FFFu);
}

void sample_play::SeedRand()
{
	// srand: the thread's seed = time(0)
	SamplePlayData().rand.seed = static_cast<uint32_t>(std::time(nullptr));
}

int sample_play::AlternatingRandom()
{
	auto& state = SamplePlayData();
	// srand(time(0)) the first time
	if (!state.rand.systemSeeded)
	{
		SeedRand();
		state.rand.systemSeeded = true;
	}
	// rand() / 2 (rand() is never negative: rand() >> 1), + 0x3FFF while the flag is set, then the flag flips
	int value = Rand() >> 1;
	if (state.rand.upperHalf)
	{
		value += 0x3FFF;
	}
	state.rand.upperHalf = !state.rand.upperHalf;
	return value;
}

int sample_play::Random(int count)
{
	// AlternatingRandom() * n / 32767 (unsigned)
	const auto value = static_cast<uint32_t>(AlternatingRandom()) * static_cast<uint32_t>(count);
	return count > 0 ? static_cast<int>(value / 32767u) : 0;
}

void sample_play::ResetRand()
{
	SamplePlayData().rand = {};
}

void sample_play::SetPitch(entt::id_type sound, Owner owner, int percent)
{
	const auto* info = Lookup(sound);
	if (info == nullptr || percent <= 0)
	{
		return;
	}
	auto* channel = First(info->bank, owner, info->id);
	// nothing while switched off, unless an atmos channel; nor when not in use or for the same pitch
	if (channel == nullptr || (!SamplePlayData().play.active && !channel->atmos) || !InUse(*channel) ||
	    channel->pitch == percent)
	{
		return;
	}
	// the .sad fixes this sample's pitch
	if ((channel->overrides & 0x01u) != 0)
	{
		return;
	}
	Output()->SetPitch(IndexOf(*channel), qmixer::FrequencyRatio(info->sampleRate, percent));
	channel->pitch = percent;
}

void sample_play::SetVolume(Channel handle, int volume)
{
	auto& state = SamplePlayData();
	const auto* started = Find(handle);
	// nothing while switched off, unless the channel is an atmos one
	if (started == nullptr || (!state.play.active && !started->atmos))
	{
		return;
	}
	// the first channel of its (bank, owner, sample), in use
	auto* channel = First(started->bank, started->owner, started->sample);
	if (channel == nullptr || !InUse(*channel))
	{
		return;
	}
	// clamped to 0..127
	const int v = std::clamp(volume, 0, qmixer::k_MaxVolume);
	// nothing when it already has that volume
	if (channel->volume == v)
	{
		return;
	}
	// the .sad fixes this sample's volume
	if ((channel->overrides & 0x20u) != 0)
	{
		return;
	}
	channel->volume = v;
	Output()->SetGain(IndexOf(*channel), qmixer::Gain(v, state.play.mainVolume));
}

void sample_play::SetMainVolume(int mainVolume)
{
	auto& state = SamplePlayData();
	// the same value or more than 127 is ignored
	if (mainVolume == state.play.mainVolume || mainVolume < 0 || mainVolume > qmixer::k_MaxVolume)
	{
		return;
	}
	state.play.mainVolume = mainVolume;
	for (auto& channel : state.play.channels)
	{
		if (InUse(channel))
		{
			Output()->SetGain(IndexOf(channel), qmixer::Gain(channel.volume, mainVolume));
		}
	}
}

int sample_play::MainVolume()
{
	return SamplePlayData().play.mainVolume;
}

void sample_play::UpdateChannels()
{
	auto& state = SamplePlayData();
	auto* output = Output();
	const auto camera = CameraPosition();
	if (output == nullptr || !camera)
	{
		return;
	}
	// every tracked 3D channel follows its owner's sound position
	for (auto& channel : state.play.channels)
	{
		if (channel.atmos || !channel.is3D || !channel.track || !InUse(channel))
		{
			continue;
		}
		// the channel's point copied for the default, then each of its coordinates beyond 5000 cleared
		// (GuardSoundPoint)
		const glm::vec3 stored = channel.position;
		channel.position = GuardSoundPoint(channel.position);
		glm::vec3 at = *camera;
		switch (channel.owner.kind)
		{
		case Owner::Kind::None:
			break;
		case Owner::Kind::Atmos:
			// the atmos owner (-1) gives no position: the channel stops
			Halt(channel, "tracked, atmos owner", true); // with the ramp
			channel.owner = {};
			continue;
		case Owner::Kind::SoundTag:
		{
			// a sound tag is not a game thing: its position is its thing's, and with no thing (a point tag, or one whose
			// thing has died) it gives none, so the channel keeps its point; it never stops the channel
			const auto position = state.play.backend.ownerPosition ? state.play.backend.ownerPosition(channel.owner)
			                                                       : std::optional<glm::vec3> {};
			at = position ? *position : stored;
			break;
		}
		case Owner::Kind::Key:
			// a key is never tracked (the voices pass track 0): not moved (inferred)
			continue;
		case Owner::Kind::Thing:
		case Owner::Kind::Object:
		{
			if (!state.play.backend.ownerPosition)
			{
				continue;
			}
			const auto position = channel.ownerGone ? std::nullopt : state.play.backend.ownerPosition(channel.owner);
			if (!position)
			{
				// the thing is no longer available: the channel stops and loses its owner
				Halt(channel, "tracked, owner gone", true); // with the ramp
				channel.owner = {};
				continue;
			}
			at = *position;
			break;
		}
		}
		// the distance is from the unguarded point + the offset to the camera; at or beyond the channel's max distance it
		// stops (a NaN does not)
		if (glm::distance(at + channel.offset, *camera) >= channel.maxDistance)
		{
			Halt(channel, "tracked, beyond its max distance", true); // with the ramp
			continue;
		}
		// the point handed to the 3D position, guarded
		channel.position = GuardSoundPoint(at);
		output->SetPosition(IndexOf(channel), channel.position + channel.offset);
	}
	// QMixer's listener at the camera's position, forward and up; the velocity stays 0
	if (Locator::camera::has_value())
	{
		const auto& listener = Locator::camera::value();
		device::SetListener(listener.GetOrigin(), glm::vec3(0.0f), listener.GetForward(), listener.GetUp());
	}
	output->SetListener(*camera);
}

void sample_play::Switch(bool on)
{
	auto& state = SamplePlayData();
	if (!on)
	{
		StopAll();
		state.play.active = false;
		return;
	}
	state.play.active = true;
}

bool sample_play::IsActive()
{
	return SamplePlayData().play.active;
}

void sample_play::ClearChannelOwners()
{
	auto& state = SamplePlayData();
	if (!state.play.active)
	{
		return;
	}
	for (auto& channel : state.play.channels)
	{
		// sample, owner and bank
		channel.sample = 0;
		channel.owner = {};
		channel.bank = 0;
	}
}

void sample_play::ReleaseSources()
{
	if (auto* output = Output(); output != nullptr)
	{
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Sample play: {} channel sources released", output->Sources());
		}
		output->DeleteAll();
	}
}

void sample_play::OnThingDeleted(entt::entity thing)
{
	for (auto& channel : SamplePlayData().play.channels)
	{
		if (channel.owner.kind == Owner::Kind::Thing && channel.owner.thing == thing)
		{
			channel.ownerGone = true;
		}
	}
}

void sample_play::UpdateFrame()
{
	if (auto* output = Output(); output != nullptr)
	{
		output->Update();
	}
}

// ---- the water port's names -------------------------------------------------------------------------------------

entt::entity sample_play::Play(const Options& options)
{
	return AsEntity(Start(options));
}

void sample_play::SetVolume(entt::entity emitter, int volume)
{
	SetVolume(AsChannel(emitter), volume);
}

void sample_play::ProcessTurn()
{
	UpdateChannels();
}

void sample_play::SetGameSound(bool enabled)
{
	audio::SetGameSound(enabled);
}

void sample_play::SetScriptWideScreen(bool on)
{
	audio::SetScriptWideScreen(on);
}

bool sample_play::IsInsideCitadel()
{
	return audio::IsInsideCitadel();
}

bool sample_play::IsVideoPlaying()
{
	return audio::IsVideoPlaying();
}

float sample_play::QMixerGain(int volume)
{
	return qmixer::Gain(volume, qmixer::k_MaxVolume);
}

float sample_play::DistanceGain(float minDistance, float maxDistance, float scale, float distance)
{
	return qmixer::DistanceGain(minDistance, maxDistance, scale, distance);
}

glm::vec3 sample_play::PolarRelative(glm::vec3 position)
{
	return qmixer::PolarRelative(position);
}

void sample_play::Clear()
{
	ReleaseSources();
}
