/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ComputerMindScaffoldRules.h"

#include <algorithm>

#include "Common/X87.h"

namespace openblack::computer_mind_rules
{

namespace
{
/// A scaffold for a house or a civic building is worth three quarters of the want of the town, and a quarter more
constexpr float k_WantShare = 0.75f;
constexpr float k_WantBase = 0.25f;
/// The game keeps the same two numbers as doubles for a civic building
constexpr double k_CivicWantShare = 0.75;
constexpr double k_CivicWantBase = 0.25;
} // namespace

bool WantsToCombine(float wantsAbodes, float wantsCivic)
{
	return wantsCivic > wantsAbodes;
}

float ScaffoldUseValue(uint32_t value, const ScaffoldTownFacts& town, float nearness)
{
	switch (value)
	{
	case 1:
	case 2:
	{
		const float wish = ((town.wantsAbodes * k_WantShare) + k_WantBase) * nearness;
		return town.adultPlaces > k_MostAdultPlacesForHouses ? 0.0f : wish;
	}
	case 3:
	case 4:
	case 5:
		return x87::Mul(x87::Add(x87::Mul(town.wantsCivic, k_CivicWantShare), k_CivicWantBase), nearness);
	case 7:
	{
		const float twice = nearness + nearness;
		return twice < 1.0f ? twice : 1.0f;
	}
	default:
		return nearness;
	}
}

float ScaffoldTownDesire(uint32_t value, const ScaffoldTownFacts& town)
{
	if (value <= 2)
	{
		return town.adultPlaces > k_MostAdultPlacesForHouses ? 0.0f : town.wantsAbodes;
	}
	if (value < 5)
	{
		return town.wantsCivic;
	}
	return 1.0f;
}

float ScaffoldPlaceDistance(float randomSpread, float radius)
{
	return randomSpread + (radius * k_ScaffoldPlaceNearest);
}

bool TooSteepForScaffold(const std::array<uint8_t, 4>& corners)
{
	const auto [lowest, highest] = std::ranges::minmax(corners);
	return static_cast<int32_t>(highest) - static_cast<int32_t>(lowest) > k_ScaffoldMostRise;
}

int32_t KeepCellOnMap(int32_t fixed, int32_t cells)
{
	const auto bits = static_cast<uint32_t>(fixed);
	const auto cell = static_cast<int16_t>(bits >> 16u);
	int32_t kept = cell;
	if (cell < 0)
	{
		kept = 0;
	}
	else if (cell > cells - 1)
	{
		kept = cells - 1;
	}
	return static_cast<int32_t>((static_cast<uint32_t>(kept) << 16u) | (bits & 0xFFFFu));
}

float WorkshopFillValue(int32_t room, float mostToProduce)
{
	return static_cast<float>(room) / mostToProduce;
}

} // namespace openblack::computer_mind_rules
