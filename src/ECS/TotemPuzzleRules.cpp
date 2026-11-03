/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TotemPuzzleRules.h"

#include <algorithm>

#include "Common/GUtilsAngle.h"
#include "Common/X87.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// The changes of the kinds with the first table. The row is the moved totem, and the column is the changed totem.
constexpr totem_puzzle::Couplings k_FirstCouplings {{
    {0, 4, 0, 0, 0, 2},
    {0, 0, 0, 0, 0, 0},
    {1, 1, 0, 2, 0, 0},
    {0, 0, 1, 0, 0, 0},
    {0, 0, 0, 0, 0, 1},
    {0, 3, 0, 1, 0, 0},
}};
/// The changes of the kinds with the second table
constexpr totem_puzzle::Couplings k_SecondCouplings {{
    {1, 0, 0, 0, 0, 3},
    {1, 1, 1, 1, 0, 0},
    {1, 1, 0, 0, 0, 0},
    {1, 1, 1, 0, 1, 0},
    {1, 1, 1, 0, 0, 0},
    {2, 0, 0, 0, 0, 1},
}};
/// The curve: a step of a sixth of three full turns in the units of the totem loop, which reads the game's table of sines
/// at one sixth of the step
constexpr int32_t k_CurveStep = 0x400;
constexpr int32_t k_CurveTableDivisor = 6;
/// A quarter turn in radians, as a float
constexpr double k_QuarterTurn = 1.5707963705062866;
} // namespace

const totem_puzzle::Couplings& totem_puzzle::CouplingsOf(script::PuzzleGameType type) noexcept
{
	if (type == script::PuzzleGameType::Totem2 || type == script::PuzzleGameType::Totem4)
	{
		return k_SecondCouplings;
	}
	return k_FirstCouplings;
}

std::optional<totem_puzzle::Change> totem_puzzle::Step(script::PuzzleGameType type, const Heights& kept, const Heights& totems)
{
	const auto moved = std::ranges::mismatch(kept, totems).in1;
	if (moved == kept.end())
	{
		return std::nullopt;
	}
	const auto index = static_cast<size_t>(std::distance(kept.begin(), moved));
	// The difference goes up by five, so that the remainder is never less than zero
	const int32_t difference = totems.at(index) - kept.at(index) + k_Positions;
	Change change {.moved = index, .kept = kept};
	change.kept.at(index) = (kept.at(index) + difference) % k_Positions;
	const auto& couplings = CouplingsOf(type).at(index);
	for (size_t other = 0; other < k_Totems; ++other)
	{
		if (other != index)
		{
			change.kept.at(other) = (couplings.at(other) * difference + change.kept.at(other)) % k_Positions;
		}
	}
	return change;
}

bool totem_puzzle::Solved(const Heights& kept) noexcept
{
	return std::ranges::all_of(kept, [](int32_t height) { return height == k_Highest; });
}

map_coords::MapCoords totem_puzzle::CurveOffset(size_t totem, float scale)
{
	const auto& table = gutils::SinTable();
	const auto index = static_cast<size_t>(static_cast<int32_t>(totem) * k_CurveStep / k_CurveTableDivisor);
	constexpr size_t k_Quarter = gutils::k_GameAngleCircle / 4;
	// Each sine times the size rounds to a float, and the double of it is exact
	const float sine = static_cast<float>(table.at(index)) * scale;
	const float cosine = static_cast<float>(table.at(index + k_Quarter)) * scale;
	return {.x = map_coords::FtoL(sine + sine), .z = map_coords::FtoL(cosine + cosine), .altitude = 0.0f};
}

float totem_puzzle::CurveFacing(const map_coords::MapCoords& place, const map_coords::MapCoords& totem)
{
	const float across = map_coords::ToMetres(totem.x) - map_coords::ToMetres(place.x);
	const float along = map_coords::ToMetres(totem.z) - map_coords::ToMetres(place.z);
	return x87::Sub(x87::Atan2(along, across), k_QuarterTurn);
}

float totem_puzzle::SlideShare(int32_t height, int32_t previous, int32_t stepsLeft, int32_t highest)
{
	// Each step rounds to a float, in the game's order
	const auto toward = static_cast<float>((k_SlideSteps - stepsLeft) * height);
	const float from = static_cast<float>(previous) * static_cast<float>(stepsLeft);
	const float sum = toward + from;
	return (sum * 0.2f) / static_cast<float>(highest);
}
