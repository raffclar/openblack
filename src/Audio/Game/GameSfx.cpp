/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The variants of PlaySoundEffect, the stop / loop / query wrappers, and the global cyclic counters of their callers
// (GameSfx, the third audio layer).

#include <array>

#include <spdlog/spdlog.h>

#define LOCATOR_IMPLEMENTATIONS
#include "Audio/Audio.h"
#include "Audio/AudioManager.h"
#include "Audio/Device/Sound.h"
#include "Audio/Game/Banks.h"
#include "ECS/Systems/AudioStateInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::audio;

namespace
{
/// The shared play options as the variants fill them: the fields they do not write keep their defaults (inferred:
/// nothing else writes those options between two calls)
sample_play::Options Variant(Owner owner, glm::vec3 position, int sample, int mode, int loops, bool extra3DFlag, bool is3D,
                             BankId bank)
{
	const auto optionsSound = SampleId(bank, sample);
	sample_play::Options options {
	    .sound = optionsSound,
	    .is3D = is3D,
	    .track = is3D, // a 3D one follows its owner
	    .extra3DFlag = extra3DFlag,
	    .owner = owner,
	    .position = position,
	    .offset = glm::vec3(0.0f),
	    .loops = loops,
	    .mode = mode,
	};
	return options;
}

/// The sound position of the owner: a thing, a registered object or a tag; nothing for the others (a key passed as a
/// thing would be a bad pointer in the original)
std::optional<glm::vec3> SoundPosition(const Owner& owner)
{
	switch (owner.kind)
	{
	case Owner::Kind::Thing:
	case Owner::Kind::Object:
	case Owner::Kind::SoundTag: // its thing's
		return OwnerSoundPosition(owner);
	default:
		return std::nullopt;
	}
}

struct CounterRule
{
	int count;
	bool maskAfter; ///< the tree mulch: c = (c + 1) & 3, then base + c
};
/// Counter -> its count (the cyclic counters)
constexpr std::array<CounterRule, static_cast<size_t>(Counter::_Count)> k_CounterRules = {{
    {9, false},  // KnockRoof
    {5, false},  // CitadelSparkEffect
    {5, false},  // CitadelSparkDamage
    {4, false},  // CreatureRockTap
    {3, false},  // CreatureSquash
    {10, false}, // HandInWater
    {4, true},   // TreeMulch
    {4, false},  // RockTap
    {4, false},  // ScaffoldCombine
    {4, false},  // ScaffoldTap
}};
/// What this module keeps between calls (Locator::audioState)
struct GameSfxState
{
	std::array<int, static_cast<size_t>(Counter::_Count)> counters {};
};

GameSfxState& GameSfxData()
{
	return openblack::Locator::audioState::value().Get<GameSfxState>();
}
} // namespace

Channel audio::AudioManager::PlaySoundEffect(Owner owner, int sample, int mode, int loops, bool extra3DFlag, bool is3D,
                                             SfxBank bank)
{
	// the registered bank of the type
	return PlaySoundEffect(owner, sample, mode, loops, extra3DFlag, is3D, Bank(bank));
}

Channel audio::AudioManager::PlaySoundEffect(Owner owner, int sample, int mode, int loops, bool extra3DFlag, bool is3D,
                                             BankId bank)
{
	// nothing for sample 0
	if (sample == 0)
	{
		return k_NoChannel;
	}
	glm::vec3 position(0.0f);
	if (is3D)
	{
		// an unavailable owner or none plays nothing; else at its sound position
		if (OwnerUnavailable(owner) || owner.kind == Owner::Kind::None)
		{
			return k_NoChannel;
		}
		if (const auto at = SoundPosition(owner))
		{
			position = *at;
		}
	}
	return PlaySoundEffectAt(owner, position, sample, mode, loops, extra3DFlag, is3D, bank);
}

Channel audio::AudioManager::PlaySoundEffectAt(Owner owner, glm::vec3 position, int sample, int mode, int loops,
                                               bool extra3DFlag, bool is3D, SfxBank bank)
{
	// the registered bank of the type
	return PlaySoundEffectAt(owner, position, sample, mode, loops, extra3DFlag, is3D, Bank(bank));
}

Channel audio::AudioManager::PlaySoundEffectAt(Owner owner, glm::vec3 position, int sample, int mode, int loops,
                                               bool extra3DFlag, bool is3D, BankId bank)
{
	// nothing for sample 0
	if (sample == 0)
	{
		return k_NoChannel;
	}
	const auto options = Variant(owner, position, sample, mode, loops, extra3DFlag, is3D, bank);
	// a 3D one of an unavailable owner plays nothing
	if (is3D && OwnerUnavailable(owner))
	{
		return k_NoChannel;
	}
	return PlaySoundEffectOptions(options);
}

Channel audio::AudioManager::PlaySoundEffectAt(Owner owner, glm::vec3 position, glm::vec3 offset, int sample, bool track,
                                               int mode, int loops, bool extra3DFlag, bool is3D, BankId bank)
{
	// nothing for sample 0
	if (sample == 0)
	{
		return k_NoChannel;
	}
	auto options = Variant(owner, position, sample, mode, loops, extra3DFlag, is3D, bank);
	options.offset = offset;
	options.track = track;
	// is3D and track and an unavailable owner: nothing
	if (is3D && track && OwnerUnavailable(owner))
	{
		return k_NoChannel;
	}
	return PlaySoundEffectOptions(options);
}

void audio::AudioManager::StopSoundEffect(int sample, Owner owner, SfxBank bank)
{
	StopSoundEffect(sample, owner, Bank(bank));
}

void audio::AudioManager::StopSoundEffect(int sample, Owner owner, BankId bank)
{
	// sample 0 = any sample of the owner in the bank
	if (SfxTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "SFX: stop {}/{} owner kind {} {}", BankGroup(bank), sample,
		                   static_cast<int>(owner.kind),
		                   owner.kind == Owner::Kind::Thing ? static_cast<uint32_t>(owner.thing) : owner.id);
	}
	if (sample == 0)
	{
		sample_play::StopOwner(bank, owner);
		return;
	}
	sample_play::Stop(SampleId(bank, sample), owner);
}

void audio::AudioManager::StopAllSoundEffects()
{
	sample_play::StopAll();
}

void audio::AudioManager::ReleaseLoop(Owner owner, int sample, SfxBank bank)
{
	ReleaseLoop(owner, sample, Bank(bank));
}

void audio::AudioManager::ReleaseLoop(Owner owner, int sample, BankId bank)
{
	sample_play::ReleaseLoop(SampleId(bank, sample), owner);
}

bool audio::AudioManager::IsPlaying(Owner owner, int sample, SfxBank bank)
{
	return IsPlaying(owner, sample, Bank(bank));
}

bool audio::AudioManager::IsPlaying(Owner owner, int sample, BankId bank)
{
	return sample_play::IsPlaying(SampleId(bank, sample), owner);
}

bool audio::AudioManager::IsPlaying(Owner owner, SfxBank bank)
{
	return sample_play::IsOwnerPlaying(Bank(bank), owner);
}

void audio::AudioManager::SetPitch(BankId bank, Owner owner, int sample, int percent)
{
	sample_play::SetPitch(SampleId(bank, sample), owner, percent);
}

void audio::AudioManager::SetVolume(Channel channel, int volume)
{
	sample_play::SetVolume(channel, volume);
}

bool audio::AudioManager::IsPlaying(Channel channel)
{
	return sample_play::IsPlaying(channel);
}

Channel audio::AudioManager::PlayingChannel(Owner owner, BankId bank)
{
	return sample_play::OwnerChannel(bank, owner);
}

int audio::AudioManager::Volume(Channel channel)
{
	return sample_play::Volume(channel);
}

int audio::AudioManager::NextCounter(Counter counter)
{
	auto& state = GameSfxData();
	const auto index = static_cast<size_t>(counter);
	if (index >= state.counters.size())
	{
		return 0;
	}
	auto& value = state.counters[index];
	const auto& rule = k_CounterRules[index];
	if (rule.maskAfter)
	{
		// g = (g + 1) & 3, then the sample is 155 + g
		value = (value + 1) & (rule.count - 1);
		return value;
	}
	// the sample is base + g, then ++g and back to 0 at the count
	const int current = value;
	value = value + 1 == rule.count ? 0 : value + 1;
	return current;
}
