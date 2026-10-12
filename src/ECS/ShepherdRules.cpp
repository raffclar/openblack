/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ShepherdRules.h"

#include <glm/vec3.hpp>

#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"

using namespace openblack;
using namespace openblack::ecs;
using map_coords::MapCoords;

namespace
{
/// The game searches the coast for at most this many cells
constexpr int32_t k_CoastCells = 999999;
} // namespace

std::optional<shepherd_rules::FlockChoice> shepherd_rules::ChooseBestFlock(std::span<const FlockCandidate> flocks)
{
	std::optional<FlockChoice> best;
	float bestScore = 0.0f;
	for (std::size_t index = 0; index < flocks.size(); ++index)
	{
		if (flocks[index].herded)
		{
			continue;
		}
		const float score = gutils::GetDistanceModifier(flocks[index].distance, k_FlockReach);
		if (score > bestScore)
		{
			best = FlockChoice {.index = index, .score = score};
			bestScore = score;
		}
	}
	return best;
}

shepherd_rules::FlockDecision shepherd_rules::DecideWithFlock(std::size_t members, float townWantsFood, float trigger)
{
	if (members < 2)
	{
		return FlockDecision::Release;
	}
	if (townWantsFood >= trigger && (members > 2 || townWantsFood > k_HungryTown))
	{
		return FlockDecision::Slaughter;
	}
	return FlockDecision::MoveToWater;
}

shepherd_rules::StoreLeg shepherd_rules::StoreLegFor(float distanceToDoor)
{
	if (distanceToDoor > k_FarFromStore)
	{
		return StoreLeg::Quarter;
	}
	if (distanceToDoor > k_NearStore)
	{
		return StoreLeg::Step;
	}
	return StoreLeg::Door;
}

MapCoords shepherd_rules::StoreLegGoal(const MapCoords& from, const MapCoords& door)
{
	const float distance = gutils::GetDistanceInMetres(from, door);
	auto goal = from;
	switch (StoreLegFor(distance))
	{
	case StoreLeg::Door:
		return door;
	case StoreLeg::Quarter:
		// A quarter of the way along the line to the door
		goal += gutils::GetPosFromAngle(gutils::Get3DAngleFromXZ(from, door), (1.0f * distance) / 4.0f);
		return goal;
	case StoreLeg::Step:
		goal += gutils::GetPosFromAngle(gutils::Get3DAngleFromXZ(from, door), k_StepNearStore);
		return goal;
	}
	return goal;
}

std::optional<std::size_t> shepherd_rules::NearestAnimal(std::span<const MemberView> members, const MapCoords& from,
                                                         float reach)
{
	std::optional<std::size_t> nearest;
	float best = reach;
	for (std::size_t index = 0; index < members.size(); ++index)
	{
		const auto& member = members[index];
		if (member.self || member.villager)
		{
			continue;
		}
		const float distance = gutils::GetDistanceInMetres(from, member.position);
		if (distance < best)
		{
			nearest = index;
			best = distance;
		}
	}
	return nearest;
}

int16_t shepherd_rules::SlaughterFood(float foodValue, FoodType foodType)
{
	const auto meatOrVegetable = static_cast<uint32_t>(FoodType::MeatVegetable);
	if ((meatOrVegetable & static_cast<uint32_t>(foodType)) == 0)
	{
		return 0;
	}
	return static_cast<int16_t>(map_coords::FtoL(foodValue));
}

std::optional<MapCoords> shepherd_rules::NearestRiverPoint(const MapCoords& from, const River& river, float reach)
{
	// Measured between the ground under the place and under each point
	const glm::vec3 here {map_coords::ToMetres(from.x), river.groundHeight(from) + from.altitude, map_coords::ToMetres(from.z)};
	std::optional<MapCoords> nearest;
	for (const auto& point : river.points)
	{
		const glm::vec3 there {map_coords::ToMetres(point.x), river.groundHeight(point) + point.altitude,
		                       map_coords::ToMetres(point.z)};
		const float distance = gutils::GetDistance(there, here);
		if (distance < reach)
		{
			MapCoords found {map_coords::ToFixed(there.x), map_coords::ToFixed(there.z), 0.0f};
			found.altitude = there.y - river.groundHeight(found);
			nearest = found;
			reach = distance;
		}
	}
	return nearest;
}

std::optional<MapCoords> shepherd_rules::NearestCoast(const MapCoords& from, const IsCoast& isCoast, float reach)
{
	auto cell = from;
	map_coords::Spiral spiral;
	for (int32_t left = k_CoastCells; left > 0; --left)
	{
		if (gutils::GetDistanceInMetres(from, cell) > reach)
		{
			return std::nullopt;
		}
		if (map_coords::InBounds(cell) && isCoast(cell))
		{
			return cell;
		}
		map_coords::AddCells(cell, spiral.Next());
	}
	return std::nullopt;
}

std::optional<MapCoords> shepherd_rules::NearestDrinkingWater(const MapCoords& from, const River& river, const IsCoast& isCoast,
                                                              float reach)
{
	const auto riverPoint = NearestRiverPoint(from, river, reach);
	if (!riverPoint.has_value())
	{
		return NearestCoast(from, isCoast, reach);
	}
	// A coast nearer than the river is taken instead
	const auto coast = NearestCoast(from, isCoast, gutils::GetDistanceInMetres(from, *riverPoint));
	return coast.has_value() ? coast : riverPoint;
}
