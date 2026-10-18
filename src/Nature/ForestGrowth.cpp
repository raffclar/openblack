/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ForestGrowth.h"

#include <algorithm>

#include "Common/X87.h"

using namespace openblack;

namespace
{
/// The wetness counts in hundredths
constexpr float k_WetShare = 0.01f;
/// The land's alignment adds half of itself
constexpr float k_AlignmentShare = 0.5f;
/// The spreading roll: this, and a draw below 1000
constexpr float k_SpreadRollBase = 2000.0f;
/// Each grown tree adds a twentieth to the spreading chance
constexpr float k_SpreadPerGrownTree = 0.05f;
/// The turns since a forest last gained a tree count in three hundredths
constexpr float k_SpreadPerTurn = 1.0f / 300.0f;
/// The empty forest's count, and the count below which it goes
constexpr uint16_t k_EmptyForestTurns = 2000;
constexpr uint16_t k_EmptyForestGone = 2;
} // namespace

int forest_growth::Wetness(int8_t rain, int8_t snowCover)
{
	return std::max<int>(rain, snowCover);
}

float forest_growth::Growth(const Kind& kind, int wetness, float landAlignment)
{
	const auto amount = kind.amount;
	const auto wet = ((static_cast<float>(wetness) * k_WetShare) * kind.rainAccelerator) * amount + amount;
	return (landAlignment * k_AlignmentShare + 1.0f) * wet;
}

float forest_growth::Grown(float size, float amount, float largest)
{
	return std::min(size + amount, largest);
}

uint16_t forest_growth::FirstWait(uint32_t draw)
{
	return static_cast<uint16_t>(draw);
}

bool forest_growth::Spreads(float roll, uint16_t& counter, uint32_t turnsSinceLastGain, size_t grownTrees)
{
	const float bar = roll + k_SpreadRollBase;
	const float grownShare = x87::Mul(static_cast<double>(grownTrees), static_cast<double>(k_SpreadPerGrownTree));
	const float share = grownShare < 1.0f ? grownShare : 1.0f;
	++counter;
	// The turns can be more than a float holds exactly: their product rounds one time
	const float perCount = x87::Mul(static_cast<double>(turnsSinceLastGain), static_cast<double>(k_SpreadPerTurn)) * share;
	const float chance = static_cast<float>(counter) * perCount;
	if (!(bar < chance))
	{
		return false;
	}
	counter = 0;
	return true;
}

uint32_t forest_growth::ParentDraws(size_t grownTrees)
{
	return static_cast<uint32_t>(grownTrees / 2 + 1);
}

bool forest_growth::EmptyForestGoes(uint16_t& countdown)
{
	if (countdown == 0)
	{
		countdown = k_EmptyForestTurns;
		return false;
	}
	--countdown;
	return countdown < k_EmptyForestGone;
}
