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

using namespace openblack;

namespace
{
/// The wetness counts in hundredths
constexpr float k_WetShare = 0.01f;
/// The land's alignment adds half of itself
constexpr float k_AlignmentShare = 0.5f;
/// The spreading roll: this, and a draw below 1000
constexpr double k_SpreadRollBase = 2000.0;
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
	// Worked out in double precision, rounded to a float after the sum and again after the product
	const auto amount = static_cast<double>(kind.amount);
	const auto wet = static_cast<float>(static_cast<double>(wetness) * static_cast<double>(k_WetShare) *
	                                        static_cast<double>(kind.rainAccelerator) * amount +
	                                    amount);
	return static_cast<float>((static_cast<double>(landAlignment) * static_cast<double>(k_AlignmentShare) + 1.0) *
	                          static_cast<double>(wet));
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
	// Worked out in double precision, the grown trees' share in a float
	const double bar = static_cast<double>(roll) + k_SpreadRollBase;
	const double share =
	    std::min(static_cast<double>(static_cast<float>(grownTrees)) * static_cast<double>(k_SpreadPerGrownTree), 1.0);
	++counter;
	const double chance =
	    static_cast<double>(counter) * (static_cast<double>(turnsSinceLastGain) * static_cast<double>(k_SpreadPerTurn) * share);
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
