/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AnimEffects.h"

#include <cstdlib>
#include <cstring>

#include <map>

#include <PackFile.h>
#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "Audio/Device/Sound.h"
#include "Audio/Engine/SamplePlay.h"
#include "Audio/Game/AudioSystem.h"
#include "Debug/DebugEnv.h"
#include "ECS/Systems/AudioStateInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::audio;

namespace
{
/// What this module keeps between calls (Locator::audioState)
struct AnimEffectsState
{
	/// The tables of each registered bank
	std::map<BankId, AnimEffectTable> tables {};
};

AnimEffectsState& AnimEffectsData()
{
	return openblack::Locator::audioState::value().Get<AnimEffectsState>();
}

/// The animation sounds' trace (AnimationSounds): only its own lines
bool AnimTrace()
{
	static const bool k_Trace = debug_env::AnimTrace();
	return k_Trace;
}

/// The core's trace (OPENBLACK_AUDIO_TRACE): every cull of an anim effect
bool AudioTrace()
{
	static const bool k_Trace = debug_env::AudioTrace();
	return k_Trace;
}
} // namespace

// ---- the tables (the miracles' AnimEffectBank, unchanged) -----------------------------------------------------------

void AnimEffectTable::Load(const pack::PackFile& file)
{
	for (const auto& header : file.GetAudioSampleHeaders())
	{
		// The sample header's override flags
		const uint32_t overrides = static_cast<uint32_t>(header.unknown10) | (static_cast<uint32_t>(header.unknown11) << 16);
		Sample sample;
		if ((overrides & 0x40u) != 0)
		{
			sample.loops = header.loop;
		}
		if ((overrides & 0x400u) != 0)
		{
			sample.playMode = static_cast<int32_t>(header.loopType);
		}
		sample.maxDistance = header.maxDist;
		samples.insert_or_assign(header.id, sample);
	}
	const auto& blocks = file.GetBlocks();
	const auto table = blocks.find("LHAudioAnimArrayTable");
	const auto lists = blocks.find("LHAudioWaveNumTable");
	if (table == blocks.end() || lists == blocks.end() || table->second.size() < 8)
	{
		return;
	}
	// u32 rows, u32 width, then rows x width s32. Every bank of the
	// game has width 6 (5 attributes and the list); another width is not read (openblack).
	int32_t count = 0;
	int32_t width = 0;
	std::memcpy(&count, table->second.data(), 4);
	std::memcpy(&width, table->second.data() + 4, 4);
	for (int32_t r = 0; width == 6 && r < count && 8 + (r + 1) * 24 <= static_cast<int32_t>(table->second.size()); ++r)
	{
		std::array<int32_t, 6> row {};
		std::memcpy(row.data(), table->second.data() + 8 + r * 24, 24);
		rows.push_back(row);
	}
	waves.resize(lists->second.size() / 4);
	std::memcpy(waves.data(), lists->second.data(), waves.size() * 4);
}

std::vector<int32_t> AnimEffectTable::FindList(const std::array<int32_t, 5>& key) const
{
	// A row matches when each of its first width - 1 columns is the wildcard or the key's; between two matching rows
	// the earlier one is kept only when it has more columns equal to the key, so the later wins a tie
	int best = -1;
	size_t bestRow = 0;
	for (size_t r = 0; r < rows.size(); ++r)
	{
		int exact = 0;
		bool match = true;
		for (size_t c = 0; c < 5 && match; ++c)
		{
			const auto value = rows[r][c];
			if (value == k_Wildcard)
			{
				continue;
			}
			match = value == key.at(c);
			++exact;
		}
		if (match && exact >= best)
		{
			best = exact;
			bestRow = r;
		}
	}
	if (best < 0)
	{
		return {};
	}
	// the last column: a dword index of LHAudioWaveNumTable, {count, sample numbers...}
	const auto list = static_cast<size_t>(rows[bestRow][5]);
	if (list >= waves.size() || waves[list] <= 0 || list + static_cast<size_t>(waves[list]) >= waves.size())
	{
		return {};
	}
	return {waves.begin() + static_cast<std::ptrdiff_t>(list + 1),
	        waves.begin() + static_cast<std::ptrdiff_t>(list + 1 + static_cast<size_t>(waves[list]))};
}

entt::id_type AnimEffectTable::SoundId(int32_t sample) const
{
	return entt::hashed_string(fmt::format("{}/{}", name, sample).c_str()).value();
}

const AnimEffectTable::Sample* AnimEffectTable::FindSample(int32_t sample) const
{
	const auto it = samples.find(sample);
	return it == samples.end() ? nullptr : &it->second;
}

// ---- the banks' tables -----------------------------------------------------------------------------------------------

void anim_effects::RegisterTables(BankId bank, const pack::PackFile& file)
{
	auto& state = AnimEffectsData();
	if (bank == k_NoBank || state.tables.contains(bank))
	{
		return;
	}
	AnimEffectTable table;
	table.name = BankGroup(bank);
	table.Load(file);
	if (table.rows.empty())
	{
		return;
	}
	if (auto logger = spdlog::get("audio"))
	{
		SPDLOG_LOGGER_DEBUG(logger, "Anim effects: {} rows in {}", table.rows.size(), table.name);
	}
	state.tables.emplace(bank, std::move(table));
}

const AnimEffectTable* anim_effects::Tables(BankId bank)
{
	auto& state = AnimEffectsData();
	const auto found = state.tables.find(bank);
	return found != state.tables.end() ? &found->second : nullptr;
}

void anim_effects::Clear()
{
	AnimEffectsData().tables.clear();
}

// ---- picking and playing anim effects -------------------------------------------------------------------------------

int anim_effects::Number(const AnimKey& key, BankId bank)
{
	// The audio system active and a bank
	if (bank == k_NoBank || !sample_play::IsActive())
	{
		return 0;
	}
	const auto* table = Tables(bank);
	if (table == nullptr)
	{
		return 0;
	}
	const auto list = table->FindList(key);
	if (list.empty())
	{
		return 0;
	}
	if (list.size() == 1)
	{
		return list[0];
	}
	// The audio system's random number in [0, count)
	return list[static_cast<size_t>(sample_play::Random(static_cast<int>(list.size())))];
}

Channel anim_effects::Play(Owner owner, float distance, int sample, bool track, BankId bank, float minDistance,
                           float maxDistance)
{
	// The audio system active and a bank
	if (bank == k_NoBank || !sample_play::IsActive())
	{
		return k_NoChannel;
	}
	const auto id = SampleId(bank, sample);
	const auto* sound = sample_play::GetSound(id);
	if (sound == nullptr)
	{
		return k_NoChannel;
	}
	// The global 800 and the sample's .sad max distance: the caller's distance must not be more
	if (!(distance <= k_MaxDistance) || !(distance <= sound->maxDistance))
	{
		const float limit = distance > k_MaxDistance ? k_MaxDistance : sound->maxDistance;
		if (AnimTrace() && bank == Bank(SfxBank::VillagersBanter))
		{
			// the animation sounds' own trace line (AnimationSounds: banter only)
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Animation sound: banter {} too far ({:.1f} > {})", sample, distance,
			                   limit);
		}
		else if (AudioTrace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Anim effect: {}/{} ({}) too far ({:.1f} > {})", BankGroup(bank), sample,
			                   sound->name, distance, limit);
		}
		return k_NoChannel;
	}
	sample_play::Options options {
	    .sound = id, // bank and sample
	    .is3D = true,
	    .track = track,
	    .extra3DFlag = false,
	    .owner = owner,
	    .callerMask = 0,
	};
	if (minDistance > 0.0f)
	{
		options.callerMask |= 0x80u;
		options.minDistance = minDistance;
	}
	if (maxDistance > 0.0f)
	{
		options.callerMask |= 0x100u;
		options.maxDistance = maxDistance;
	}
	// The game's 3D position for the owner; none = nothing plays
	const auto position = OwnerSoundPoint(owner);
	if (!position)
	{
		return k_NoChannel;
	}
	options.position = *position;
	return sample_play::Start(options);
}

Channel anim_effects::PlayKey(Owner owner, float distance, const AnimKey& key, AnimAction action, bool track, BankId bank,
                              float minDistance, float maxDistance)
{
	// The audio system active, a bank, and the distance within the global 800
	if (bank == k_NoBank || !sample_play::IsActive() || !(distance <= k_MaxDistance))
	{
		return k_NoChannel;
	}
	const auto* table = Tables(bank);
	if (table == nullptr)
	{
		return k_NoChannel;
	}
	const auto list = table->FindList(key);
	if (list.empty())
	{
		return k_NoChannel;
	}
	switch (action)
	{
	case AnimAction::Play:
	{
		// One sample of the list, then the same as Play
		const int sample =
		    list.size() == 1 ? list[0] : list[static_cast<size_t>(sample_play::Random(static_cast<int>(list.size())))];
		return Play(owner, distance, sample, track, bank, minDistance, maxDistance);
	}
	case AnimAction::Stop:
		// For each sample of the list the first channel of (bank, owner, sample) stops
		for (const auto sample : list)
		{
			sample_play::Stop(SampleId(bank, sample), owner);
		}
		return k_NoChannel;
	default:
		// The same, releasing the loops
		for (const auto sample : list)
		{
			sample_play::ReleaseLoop(SampleId(bank, sample), owner);
		}
		return k_NoChannel;
	}
}
