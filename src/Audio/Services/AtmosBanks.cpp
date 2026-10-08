/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AtmosBanks.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <vector>

#include <spdlog/spdlog.h>

#include "Audio/Device/Sound.h"
#include "Audio/Engine/SamplePlay.h"
#include "Audio/Game/AudioSystem.h"
#include "Audio/Game/Banks.h"
#include "Audio/GameQueries.h"
#include "Audio/Services/SoundMap.h"
#include "Debug/DebugEnv.h"
#include "ECS/Systems/AudioStateInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::audio;

namespace
{

/// A bank of the atmos mixer
struct Bank
{
	bool registered {false};
	uint32_t group {0}; ///< the bank's atmos group
	int32_t volume {0}; ///< the bank volume 0..127
};

/// A bank's loop sample (atmos frequency 0)
struct Loop
{
	size_t bank;
	entt::id_type sample;
	uint32_t group; ///< the sample's atmos group
	int32_t volume; ///< the .sad volume (flag 0x20) or 127
	int32_t fade {0};
	bool playing {false};
	Channel channel {k_NoChannel};
};

/// A loose sample (atmos frequency f > 0) in the time-ordered queue
struct Loose
{
	size_t bank;
	entt::id_type sample;
	uint32_t group;
	int32_t volume;
	int32_t frequency;
	uint32_t next; ///< the counter value at which it is due
};

/// A playing loose sample (the mixer's channel list)
struct LooseChannel
{
	Channel emitter;
	size_t bank;
	uint32_t group;
	int32_t volume;
};

struct AtmosBanksState
{
	bool initialised {false};
	std::array<Bank, k_AtmosTypeCount> banks;
	std::array<float, k_AtmosTypeCount> target {};
	std::array<float, k_AtmosTypeCount> current {};
	std::vector<Loop> loops;
	std::vector<Loose> queue;
	std::vector<LooseChannel> channels;
	uint32_t counter {0}; ///< the mixer's turn counter
	int32_t cornerA {1};  ///< x sign of the corner a loose sample at the listener is pushed to
	int32_t cornerB {1};  ///< its y sign
};

/// The AtmosBanks state (Locator::audioState)
AtmosBanksState& AtmosBanksData()
{
	return openblack::Locator::audioState::value().Get<AtmosBanksState>();
}

/// The C runtime's rand(), 0..32767, which the atmos mixer calls straight: the one generator of src/Audio
/// (sample_play::Rand)
int32_t Rand()
{
	return sample_play::Rand();
}

/// The Dump's bank lines: the turns of OPENBLACK_ATMOS_TRACE's period
bool Trace()
{
	return sound_map::TraceThisTurn();
}

/// (openblack) the loop starts and loose samples: every one while OPENBLACK_ATMOS_TRACE is set
bool TraceEvents()
{
	static const bool k_Trace = debug_env::AtmosTrace();
	return k_Trace;
}

bool Available()
{
	return Locator::resources::has_value();
}

/// A sample of a registered bank (the resources the bank registration filled, Banks.cpp)
const Sound& SoundOf(entt::id_type id)
{
	return Locator::resources::value().GetSounds().Handle(id);
}

/// The sample volume (the mixer's linear law, sample_play::QMixerGain)
void SetGain(Channel emitter, int32_t volume)
{
	sample_play::SetVolume(emitter, volume);
}

bool IsPlaying(Channel emitter)
{
	return sample_play::IsPlaying(emitter);
}

/// Stop a sample (an atmos channel stops even while the audio is switched off). Silence stops every kept channel
/// without asking whether it plays
void StopChannel(Channel& emitter)
{
	if (emitter != k_NoChannel)
	{
		if (TraceEvents() && IsPlaying(emitter))
		{
			const auto infos = sample_play::Channels();
			for (const auto& info : infos)
			{
				if (info.handle == emitter)
				{
					SPDLOG_LOGGER_INFO(spdlog::get("audio"), "(openblack) Atmos channel stopped: {}", SoundOf(info.sound).name);
				}
			}
		}
		sample_play::Stop(emitter);
	}
	emitter = k_NoChannel;
}

/// The list is walked from the head (on a reinsertion from the head's next, the head itself being popped afterwards)
/// and the entry goes before the first one due at the same time or later
void Enqueue(const Loose& loose)
{
	auto& state = AtmosBanksData();
	const auto at = std::lower_bound(state.queue.begin(), state.queue.end(), loose.next,
	                                 [](const Loose& other, uint32_t next) { return other.next < next; });
	state.queue.insert(at, loose);
}

uint32_t NextTime(int32_t frequency)
{
	return AtmosBanksData().counter + static_cast<uint32_t>(4 * frequency + Rand() * 12 * frequency / 32767);
}

/// Register the 14 banks with the atmos mixer
void Register()
{
	auto& state = AtmosBanksData();
	for (size_t i = 1; i < k_AtmosTypeCount; ++i)
	{
		const auto* name = k_AtmosTypes[i].bank;
		// the bank of Audio\SFX\Atmos registered by its file name (banks::LoadAll)
		const auto bank = FindBank(std::string("/") + name);
		if (bank == k_NoBank)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("audio"), "Atmos: no sound bank {}", name);
			continue;
		}
		state.banks[i].registered = true;
		// srand(time(0)) before the bank's draws. The audio library also seeds once at its init, before these:
		// overwritten, not emulated
		sample_play::SeedRand();
		bool anyLoose = false;
		for (const auto id : banks::Samples(bank))
		{
			const auto& sound = SoundOf(id);
			// atmos frequency >= 0 and a sample id > 0
			if (sound.atmosFrequency < 0 || sound.id <= 0)
			{
				continue;
			}
			// vol = the .sad's with flag 0x20, else 127
			const auto volume = static_cast<int32_t>(sound.volume127);
			const auto group = static_cast<uint32_t>(sound.atmosGroup);
			if (sound.atmosFrequency == 0)
			{
				state.loops.push_back({i, id, group, volume});
			}
			else
			{
				Enqueue({i, id, group, volume, sound.atmosFrequency, NextTime(sound.atmosFrequency)});
				anyLoose = true;
			}
		}
		// the first loose sample plays within 20 turns
		if (anyLoose && !state.queue.empty())
		{
			state.counter = state.queue.front().next - 20;
		}
	}
	SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Atmos: {} loops and {} loose samples registered", state.loops.size(),
	                   state.queue.size());
	state.initialised = true;
}

/// The targets are the sound map's volumes, or 0 inside the citadel
void SetTargets(bool insideCitadel)
{
	auto& state = AtmosBanksData();
	if (insideCitadel)
	{
		// 15 floats cleared: the 14 targets and current[0]
		state.target.fill(0.0f);
		state.current[0] = 0.0f;
		return;
	}
	state.target = sound_map::GetVolumes();
	// the 15th float copied is the sound map's receiver x: it lands in current[0], NONE's (no bank)
	state.current[0] = sound_map::GetReceiverX();
}

void ProcessBanks()
{
	auto& state = AtmosBanksData();
	const bool trace = Trace();
	const float alignment = atmos_banks::Alignment();
	const uint32_t group = atmos_banks::GroupFor(alignment);
	if (TraceEvents() && group != state.banks[0].group)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "(openblack) Atmos group {} (alignment {:.3f})", group, alignment);
	}
	for (size_t i = 0; i < k_AtmosTypeCount; ++i)
	{
		state.banks[i].group = group;
		float& current = state.current[i];
		const float target = state.target[i];
		// slow near the ends
		const float step = (current > 0.1f && current <= 0.8f) ? 0.04f : 0.02f;
		if (target > current)
		{
			current = std::min(current + step, target);
		}
		else
		{
			current = std::max(current - step, target);
		}
		const auto sent = static_cast<int32_t>(current * 127.0f);
		state.banks[i].volume = std::clamp(sent, 0, 127);
		if (trace)
		{
			std::array<char, 128> line {};
			std::snprintf(line.data(), line.size(), "%s Vol=%3.3f Sent=%d Step=%d", k_AtmosTypes[i].name,
			              static_cast<double>(current), sent, static_cast<int>(step * 100.0f));
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "{}", line.data());
		}
	}
}

/// Play a loop: atmos, 2D, owner -1, loops -1, mode 2, caller mask 0x20, volume 0
Channel PlayLoop(entt::id_type sample)
{
	const auto optionsOwner = sample_play::Owner::AtmosMixer();
	sample_play::Options options {
	    .sound = sample,
	    .atmos = true,
	    .is3D = false,
	    .owner = optionsOwner,
	    .volume = 0,
	    .loops = -1,
	    .mode = 2,
	    .callerMask = 0x20,
	};
	return sample_play::Start(options);
}

/// Play a loose sample: atmos, 3D, relative to the listener at (x, y, 0), owner -1, caller mask 0x20; the mode (3),
/// pitch, deviation and distance mapping are the .sad's or the defaults
Channel PlayLoose(entt::id_type sample, glm::vec3 position, int32_t volume)
{
	const auto optionsOwner = sample_play::Owner::AtmosMixer();
	sample_play::Options options {
	    .sound = sample,
	    .atmos = true,
	    .is3D = true,
	    .relative = true,
	    .owner = optionsOwner,
	    .volume = volume,
	    .position = position,
	    .callerMask = 0x20,
	};
	return sample_play::Start(options);
}

/// One pass of the atmos mixer
void Process()
{
	auto& state = AtmosBanksData();
	auto& banks = state.banks;

	// 1. the loose channels of a group other than their bank's fade by 5 a turn
	std::erase_if(state.channels, [](const LooseChannel& channel) { return !IsPlaying(channel.emitter); });
	for (auto& channel : state.channels)
	{
		if (channel.group != 0 && channel.group != banks[channel.bank].group && channel.volume - 5 >= 0)
		{
			channel.volume -= 5;
			SetGain(channel.emitter, channel.volume);
		}
	}

	// 2. the loops
	for (auto& loop : state.loops)
	{
		const auto& bank = banks[loop.bank];
		if (loop.playing && !IsPlaying(loop.channel))
		{
			loop.playing = false;
			loop.channel = k_NoChannel;
		}
		if (loop.playing && bank.volume == 0)
		{
			// a dry stop: the bank's volume has already faded
			StopChannel(loop.channel);
			loop.playing = false;
		}
		if (!loop.playing && bank.volume != 0 && (loop.group == 0 || loop.group == bank.group))
		{
			loop.channel = PlayLoop(loop.sample);
			loop.playing = loop.channel != k_NoChannel;
			loop.fade = 0;
			if (TraceEvents() && loop.playing)
			{
				SPDLOG_LOGGER_INFO(spdlog::get("audio"), "(openblack) Atmos loop start: {} of {}", SoundOf(loop.sample).name,
				                   k_AtmosTypes[loop.bank].name);
			}
		}
	}
	for (auto& loop : state.loops)
	{
		const auto& bank = banks[loop.bank];
		if (loop.group != 0 && loop.group != bank.group)
		{
			if (!loop.playing)
			{
				continue;
			}
			if (loop.fade <= 0)
			{
				StopChannel(loop.channel);
				loop.playing = false;
				loop.fade = 0;
				continue;
			}
			loop.fade = std::max(0, loop.fade - 5);
		}
		else if (loop.playing)
		{
			// +5 a turn, unclamped until it reaches the loop's volume
			loop.fade = loop.fade < loop.volume ? loop.fade + 5 : loop.volume;
		}
		if (loop.playing)
		{
			SetGain(loop.channel, bank.volume * loop.fade / 127);
		}
	}

	// 3. at most one loose sample a turn: the head of the queue
	++state.counter;
	if (state.queue.empty() || state.queue.front().next > state.counter)
	{
		return;
	}
	Loose loose = state.queue.front();
	state.queue.erase(state.queue.begin());
	const auto& bank = banks[loose.bank];
	if (bank.volume != 0 && (loose.group == 0 || loose.group == bank.group))
	{
		// integers -2..2 (rand() * 4 / 32767), pushed off the listener
		float x = static_cast<float>(2 - Rand() * 4 / 32767);
		float y = static_cast<float>(2 - Rand() * 4 / 32767);
		if (std::abs(x) + std::abs(y) <= 1.0f)
		{
			x *= 4.0f;
			y *= 4.0f;
			if (x == 0.0f && y == 0.0f)
			{
				x = static_cast<float>(5 * state.cornerA);
				y = static_cast<float>(5 * state.cornerB);
				const int32_t a = state.cornerA;
				state.cornerA = -a;
				state.cornerB = -a * state.cornerB;
			}
		}
		const int32_t volume = bank.volume * loose.volume / 127;
		// relative to the listener at (x, y, 0): x to the right, y ahead, z up (sample_play::PolarRelative), so
		// the loose samples lie flat around the listener, 2 to 7.1 units away
		const auto emitter = PlayLoose(loose.sample, glm::vec3(x, y, 0.0f), volume);
		if (emitter != k_NoChannel)
		{
			state.channels.push_back({emitter, loose.bank, loose.group, volume});
		}
		if (TraceEvents())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "(openblack) Atmos loose sample: {} of {} at ({:.0f}, {:.0f}) volume {}",
			                   SoundOf(loose.sample).name, k_AtmosTypes[loose.bank].name, x, y, volume);
		}
	}
	// rescheduled even when it did not sound
	loose.next = NextTime(loose.frequency);
	Enqueue(loose);
}
} // namespace

uint32_t atmos_banks::GroupFor(float alignment)
{
	// the float compared with the double -0.59999999999999998: below, equal or unordered -> group 2, else 1
	return static_cast<double>(alignment) > -0.6 ? 1 : 2;
}

float atmos_banks::Alignment()
{
	// read as it is (by ProcessAtmosBanks and the alignment music): the game writes it once a turn, and
	// GameQueries::cameraAlignment gives that value. Unset: 0 (the audio's reset).
	const auto& query = Queries().cameraAlignment;
	return query ? query() : 0.0f;
}

void atmos_banks::UpdateBanks()
{
	if (!Available())
	{
		return;
	}
	if (!AtmosBanksData().initialised)
	{
		Register();
	}
	// the targets, then ProcessAtmosBanks
	SetTargets(sample_play::IsInsideCitadel());
	ProcessBanks();
}

void atmos_banks::Mix()
{
	if (!Available() || !AtmosBanksData().initialised)
	{
		return;
	}
	Process();
}

void atmos_banks::Silence()
{
	auto& state = AtmosBanksData();
	if (!state.initialised || !Available())
	{
		return;
	}
	for (auto& loop : state.loops)
	{
		StopChannel(loop.channel);
		loop.playing = false;
	}
	for (auto& channel : state.channels)
	{
		StopChannel(channel.emitter);
	}
	state.channels.clear();
}

void atmos_banks::Clear()
{
	// the audio's reset on a new map: Silence, every sample stopped and the alignment back to 0; the banks, their
	// targets and current volumes are kept (the registration runs once)
	Silence();
}
