/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameStats.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ECS/Systems/GameStatsSystemInterface.h"
#include "Locator.h"

using namespace openblack::game_stats;

namespace
{
/// Every player's stats and the shared statics (Locator::gameStatsSystem)
openblack::ecs::systems::GameStatsSystemInterface& GameStats()
{
	if (!openblack::Locator::gameStatsSystem::has_value())
	{
		std::fputs("game_stats: no game stats in the locator (Locator::gameStatsSystem)\n", stderr);
		std::abort();
	}
	return openblack::Locator::gameStatsSystem::value();
}

void Append(std::vector<uint8_t>& out, const void* data, size_t size)
{
	const auto* bytes = static_cast<const uint8_t*>(data);
	out.insert(out.end(), bytes, bytes + size);
}
} // namespace

void History::Add(float value)
{
	// The population and the influence histories follow the same rules
	buckets.at(count) += value / static_cast<float>(period);
	++samples;
	if (samples != period)
	{
		return;
	}
	++count;
	if (count == capacity)
	{
		// Adjacent buckets averaged in pairs into the first half
		count = 0;
		for (uint32_t i = 0; i < capacity; i += 2)
		{
			buckets.at(count) = (buckets.at(i) + buckets.at(i + 1)) * 0.5f;
			++count;
		}
		// As in the original, `count` BYTES are cleared from bucket[count], not `count` buckets
		std::memset(&buckets.at(count), 0, count);
		period = static_cast<uint16_t>(period << 1);
	}
	samples = 0;
}

Stats& openblack::game_stats::Of(size_t player)
{
	return GameStats().PlayerStats().at(player);
}

void openblack::game_stats::ClearAll()
{
	auto& gameStats = GameStats();
	gameStats.PlayerStats().fill(Stats {});
	auto& statics = gameStats.SharedStats();
	const uint32_t version = statics.version;
	statics = {};
	statics.version = version;
}

Statics& openblack::game_stats::GetStatics()
{
	return GameStats().SharedStats();
}

void openblack::game_stats::Init(size_t player, float alignment, float creatureValue, int32_t now, uint32_t townCount)
{
	auto& stats = Of(player);
	stats.alignmentAtStart = alignment;
	stats.creatureAtStart = creatureValue;
	auto& statics = GetStatics();
	statics.startTime = now;
	statics.townCount = townCount; // The town list's count
}

void openblack::game_stats::AddToTotalLinesOfCodeExecuted(uint32_t (*localRand)(int32_t))
{
	// The constants 10130 and 0.2 are doubles
	auto& statics = GetStatics();
	const double base = static_cast<double>(statics.townCount) * 10130.0;
	const auto range = static_cast<int32_t>(static_cast<int64_t>(base * 0.2)); // To 64 bits, then the low 32
	const uint32_t r = localRand != nullptr ? localRand(range) : 0;
	statics.linesOfCode += static_cast<uint32_t>(static_cast<int64_t>(static_cast<double>(r) + base));
}

void openblack::game_stats::TrackFrameRate(int32_t frameRate)
{
	auto& statics = GetStatics();
	const auto rate = static_cast<float>(frameRate);
	if (rate > statics.maxFrameRate) // A new maximum skips the minimum
	{
		statics.maxFrameRate = rate;
	}
	else if (rate < statics.minFrameRate)
	{
		statics.minFrameRate = rate;
	}
}

void openblack::game_stats::ObjectCreated()
{
	++GetStatics().objectsCreated;
}

void openblack::game_stats::ChildBorn(size_t player)
{
	++Of(player).births;
}

void openblack::game_stats::VillagerDied(size_t townPlayer, size_t killer)
{
	++Of(townPlayer).deaths;
	++Of(killer).killedVillagers;
}

void openblack::game_stats::TownTakenOver(size_t player, uint32_t population)
{
	Of(player).townTakenOverPopulation += population; // The town's adults + children
}

void openblack::game_stats::TownDesire(size_t player, float value)
{
	auto& stats = Of(player);
	stats.desireSum += value;
	++stats.desireCount;
}

void openblack::game_stats::BuildingBuilt(size_t player, uint32_t abodeTypeBits)
{
	auto& stats = Of(player);
	++stats.buildingsBuilt;
	if ((abodeTypeBits & 2u) != 0)
	{
		++stats.buildingsBuiltByKind.at(0);
	}
	else if ((abodeTypeBits & 4u) != 0)
	{
		++stats.buildingsBuiltByKind.at(1);
	}
	else if ((abodeTypeBits & 0x100u) != 0)
	{
		++stats.buildingsBuiltByKind.at(2);
	}
}

void openblack::game_stats::FoodEaten(size_t player, uint32_t amount)
{
	Of(player).foodEaten += amount;
}

void openblack::game_stats::WoodUsed(size_t player, uint32_t amount)
{
	Of(player).woodUsed += amount;
}

void openblack::game_stats::BuildingStoppedFunctional(size_t player)
{
	++Of(player).buildingStopped;
}

void openblack::game_stats::WorldBelief(size_t player, float belief)
{
	auto& stats = Of(player);
	if (belief > stats.maxWorldBelief)
	{
		stats.maxWorldBelief = belief;
	}
	else if (belief < stats.minWorldBelief)
	{
		stats.minWorldBelief = belief;
	}
}

void openblack::game_stats::Killed(size_t player)
{
	++Of(player).killed;
}

uint32_t openblack::game_stats::SpellsCast(const ecs::components::PlayerMagic& magic, uint32_t magicType)
{
	if ((magicType >= 1 && magicType <= 35) || magicType == 41)
	{
		return magic.castCount.at(magicType);
	}
	return 0;
}

void openblack::game_stats::DiscipleMade(size_t player, uint32_t disciple)
{
	// 1..5 -> disciples[0..4], 6 nothing, 7 -> [6], 8 -> [7]
	auto& disciples = Of(player).disciples;
	if (disciple >= 1 && disciple <= 5)
	{
		++disciples.at(disciple - 1);
	}
	else if (disciple == 7 || disciple == 8)
	{
		++disciples.at(disciple - 1);
	}
}

void openblack::game_stats::TrackPopulation(size_t player, uint32_t males, uint32_t females)
{
	auto& stats = Of(player);
	const auto track = [](uint32_t value, uint32_t& maximum, uint32_t& minimum) {
		if (value > maximum) // A new maximum skips the minimum
		{
			maximum = value;
		}
		else if (value < minimum)
		{
			minimum = value;
		}
	};
	track(males + females, stats.maxPopulation, stats.minPopulation);
	track(males, stats.maxMales, stats.minMales);
	track(females, stats.maxFemales, stats.minFemales);
}

std::vector<uint8_t> openblack::game_stats::Serialize(const Stats& stats, const ecs::components::PlayerMagic& magic)
{
	// The save order after the header and the player; the statics where the original puts them
	std::vector<uint8_t> out;
	const auto& st = GetStatics();
	Append(out, &st.playersOut, 4);
	Append(out, &stats.finishPlace, 4);
	Append(out, &stats.finishValue, 4);
	// (approximate) The original saves the town totals cache as it is; here zeros, as it is recomputed on its first read
	const std::array<uint8_t, 0x28> townTotals {};
	Append(out, townTotals.data(), townTotals.size());
	for (const uint32_t v : {stats.births, stats.deaths, stats.maxPopulation, stats.maxMales, stats.maxFemales,
	                         stats.minPopulation, stats.minMales, stats.minFemales, stats.townTakenOverPopulation})
	{
		Append(out, &v, 4);
	}
	Append(out, &stats.desireSum, 4);
	Append(out, &stats.desireCount, 4);
	Append(out, &stats.buildingsBuilt, 4);
	Append(out, stats.buildingsBuiltByKind.data(), 12);
	Append(out, stats.disciples.data(), 32);
	Append(out, &stats.foodEaten, 4);
	Append(out, &stats.woodUsed, 4);
	for (const auto* history : {&stats.influence, &stats.population}) // 0x7DC bytes each
	{
		Append(out, history->buckets.data(), sizeof(float) * History::k_Capacity);
		Append(out, &history->capacity, 4);
		Append(out, &history->count, 4);
		Append(out, &history->period, 2);
		Append(out, &history->samples, 2);
	}
	Append(out, &stats.playersOutBefore, 4);
	Append(out, &stats.alignmentAtStart, 4);
	Append(out, &stats.killedVillagers, 4);
	Append(out, &stats.maxWorldBelief, 4);
	Append(out, &stats.minWorldBelief, 4);
	Append(out, &stats.creatureValue, 4);
	Append(out, &stats.creatureAtStart, 4);
	Append(out, &stats.buildingStopped, 4);
	Append(out, stats.u1084.data(), 12);
	Append(out, &st.linesOfCode, 4);
	Append(out, &st.maxFrameRate, 4);
	Append(out, &st.minFrameRate, 4);
	Append(out, &st.objectsCreated, 4);
	Append(out, &st.startTime, 4); // Saved as 4 bytes
	Append(out, &st.version, 4);
	for (uint32_t type = 1; type <= 35; ++type) // Spells cast
	{
		const auto count = SpellsCast(magic, type);
		Append(out, &count, 4);
	}
	const auto itchy = SpellsCast(magic, 41); // MagicType 41
	Append(out, &itchy, 4);
	Append(out, &magic.chantsUsed, 4); // Chants used
	Append(out, &stats.killed, 4);     // (inferred) the killed counter
	Append(out, &st.townCount, 4);
	return out;
}
