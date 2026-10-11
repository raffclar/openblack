/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerDeathRules.h"

namespace openblack
{

uint32_t villager_death::GuidanceGapSpread(uint32_t leastGap, float draw)
{
	const float cube = draw * draw * draw;
	return static_cast<uint32_t>(static_cast<float>(leastGap * 5u) * (1.0f - cube));
}

bool villager_death::GuidanceMayPlay(const GuidanceKind& kind, uint32_t helpLevel, bool firstLandForOnePlayer, uint32_t gap,
                                     uint32_t turnsSinceLastHeard)
{
	if (firstLandForOnePlayer && !kind.onFirstLand)
	{
		return false;
	}
	return kind.helpLevel <= helpLevel && gap < turnsSinceLastHeard;
}

void villager_death::CountDeath(DeathCounts& counts, uint32_t turn, size_t killer, DeathReason reason)
{
	counts.lastDeathTurn = turn;
	++counts.total;
	if (killer < counts.byPlayer.size())
	{
		++counts.byPlayer.at(killer);
	}
	if (const auto index = static_cast<size_t>(reason); index < counts.byReason.size())
	{
		++counts.byReason.at(index);
	}
}

int32_t villager_death::PointingLimit(float drawUpToTwenty, uint32_t turnsPerSecond)
{
	return static_cast<int32_t>((drawUpToTwenty + 2.0f) * static_cast<float>(turnsPerSecond));
}

int32_t villager_death::MourningLimit(float drawUpToThree, uint32_t turnsPerSecond)
{
	return static_cast<int32_t>((drawUpToThree + 4.0f) * static_cast<float>(turnsPerSecond));
}

villager_death::Approach villager_death::ApproachToBody(float distance, float keepAway)
{
	if (!(distance > keepAway * k_CloseEnoughShare))
	{
		return {.looks = true, .walk = 0.0f};
	}
	const float walk = distance - keepAway;
	if (!(walk > k_LeastWalk))
	{
		return {.looks = true, .walk = 0.0f};
	}
	return {.looks = false, .walk = walk};
}

uint8_t villager_death::SoulAlphaAt(uint32_t timeMs, uint32_t clipMs)
{
	const auto fadeFrom = clipMs > k_SoulFadeMs ? clipMs - k_SoulFadeMs : 0u;
	if (timeMs <= fadeFrom)
	{
		return k_SoulAlpha;
	}
	const float faded = 1.0f - static_cast<float>(timeMs - fadeFrom) * (1.0f / static_cast<float>(k_SoulFadeMs));
	const auto alpha = static_cast<int32_t>(faded * static_cast<float>(k_SoulAlpha));
	return static_cast<uint8_t>(alpha < 0 ? 0 : alpha);
}

} // namespace openblack
