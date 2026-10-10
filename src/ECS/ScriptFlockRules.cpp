/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptFlockRules.h"

#include <cmath>

#include <algorithm>
#include <numbers>

#include "Common/GUtilsDistance.h"

using namespace openblack;
using namespace openblack::ecs;
using map_coords::MapCoords;

namespace
{
/// The game works the point out at double precision from a float angle and distance, and truncates it
int32_t Offset(int32_t fixed, double along)
{
	const double metres = along + static_cast<double>(fixed) * 10.0 / 65536.0;
	const double back = metres * 65536.0 / 10.0;
	if (!(back > -2147483648.0 && back < 2147483648.0))
	{
		return static_cast<int32_t>(0x80000000u);
	}
	return static_cast<int32_t>(back);
}

constexpr int k_Draws = 2;
constexpr int k_SpiralCells = 25;
} // namespace

std::size_t script_flock_rules::InsertAt(std::span<const uint8_t> orders, uint8_t order)
{
	const auto at = std::ranges::find_if(orders, [order](uint8_t other) { return other >= order; });
	return static_cast<std::size_t>(at - orders.begin());
}

uint8_t script_flock_rules::LeaderOrder(std::optional<uint8_t> lastOrder)
{
	return lastOrder.has_value() ? static_cast<uint8_t>(*lastOrder + 1) : k_FirstLeaderOrder;
}

MapCoords script_flock_rules::CalcRandomPos(const MapCoords& centre, float least, float most, const FloatRandom& random,
                                            const Accepts& accepts)
{
	const float range = most - least;
	for (int draw = 0; draw < k_Draws; ++draw)
	{
		const float angle = random(2.0f * std::numbers::pi_v<float>);
		const double distance = static_cast<double>(random(range)) + static_cast<double>(least);
		MapCoords point {.x = Offset(centre.x, std::cos(static_cast<double>(angle)) * distance),
		                 .z = Offset(centre.z, std::sin(static_cast<double>(angle)) * distance),
		                 .altitude = 0.0f};
		map_coords::Spiral spiral;
		for (int left = k_SpiralCells; left != 0;)
		{
			if (accepts(point))
			{
				return point;
			}
			--left;
			map_coords::AddCells(point, spiral.Next());
		}
	}
	// The living takes its flock's place itself when nothing round it will do
	return {.x = centre.x, .z = centre.z, .altitude = centre.altitude};
}

bool script_flock_rules::WithinDomain(const MapCoords& point, const MapCoords& place, uint16_t radius)
{
	return static_cast<float>(radius) >= gutils::GetDistanceInMetres(place, point);
}

script_flock_rules::FlockStep script_flock_rules::MoveInFlock(const MapCoords& position, const FlockView& flock,
                                                              const FloatRandom& random, const Accepts& accepts)
{
	if (WithinDomain(position, flock.place, flock.domainRadius))
	{
		return {.result = 1};
	}
	if (flock.isLeader)
	{
		return {.result = 0x23,
		        .goal = CalcRandomPos(flock.place, 0.0f, static_cast<float>(flock.domainRadius), random, accepts)};
	}
	if (!(static_cast<float>(flock.flockDistance) < gutils::GetDistanceInMetres(flock.leader, position)))
	{
		return {.result = 0};
	}
	const auto goal = CalcRandomPos(flock.leader, 0.0f, static_cast<float>(flock.flockDistance), random, accepts);
	const bool leaderWithin = WithinDomain(flock.leader, flock.place, flock.domainRadius);
	if (WithinDomain(goal, flock.place, flock.domainRadius) || !leaderWithin)
	{
		return {.result = 0x23, .goal = goal};
	}
	return {.result = 0};
}
