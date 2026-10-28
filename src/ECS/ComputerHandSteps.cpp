/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ComputerHandSteps.h"

#include <cassert>
#include <cmath>
#include <cstdint>

#include "3D/MapCoords.h"

using namespace openblack;
using namespace openblack::computer_hand;
using namespace openblack::computer_hand::steps;

void openblack::computer_hand::steps::AddStep(StepTable& table, [[maybe_unused]] StepOwner owner, StepKind kind, StepRow row)
{
	// Only the owner of a kind of step adds it, one time
	assert(OwnerOf(kind) == owner);
	assert(!table.at(static_cast<size_t>(kind)).made);
	table.at(static_cast<size_t>(kind)) = row;
}

Argument openblack::computer_hand::steps::ArgumentAt(const Step& step, size_t index)
{
	return index < step.arguments.size() ? step.arguments[index] : Argument {};
}

glm::vec3 openblack::computer_hand::steps::PlaceOf(const World& world, const map_coords::MapCoords& coords)
{
	return {map_coords::ToMetres(coords.x), world.moves.LandHeight(coords) + coords.altitude, map_coords::ToMetres(coords.z)};
}

map_coords::MapCoords openblack::computer_hand::steps::UnderHand(const HandAction& action)
{
	return {.x = map_coords::FtoL(action.hand.x * 65536.0f * 0.1f),
	        .z = map_coords::FtoL(action.hand.z * 65536.0f * 0.1f),
	        .altitude = 0.0f};
}

float openblack::computer_hand::steps::StepTowardTarget(HandAction& action)
{
	float dx = action.target.x - action.hand.x;
	float dz = action.target.z - action.hand.z;
	const float distance = std::sqrt(dz * dz + dx * dx);
	if (dx != 0.0f || dz != 0.0f)
	{
		const float inverse = 1.0f / distance;
		dx = inverse * dx;
		dz = dz * inverse;
	}
	// Near the target the hand moves at its speed. Further away it moves a fixed step each turn.
	float step = action.speed / static_cast<float>(k_TurnsPerSecond);
	if (distance < step)
	{
		step = distance;
	}
	if (distance > k_FarDistance)
	{
		step = k_FarStep;
	}
	else if (distance > k_MiddleDistance)
	{
		step = k_MiddleStep;
	}
	action.hand.x = dx * step + action.hand.x;
	action.hand.y = step * 0.0f + action.hand.y;
	action.hand.z = dz * step + action.hand.z;
	return distance;
}

void openblack::computer_hand::steps::FloatOverLand(HandAction& action, const World& world)
{
	action.hand.y = world.moves.LandHeight(UnderHand(action)) + k_HandHeight;
}

void openblack::computer_hand::steps::KeepHandOver(HandAction& action, const World& world, entt::entity thing)
{
	const auto coords = world.moves.CoordsOf(thing);
	if (!coords.has_value())
	{
		return;
	}
	action.target = PlaceOf(world, *coords);
	action.target.y = world.moves.HeightOf(thing) + coords->altitude;
	StepTowardTarget(action);
	const float over = world.moves.HeightOf(thing) * k_OverThingHeights;
	action.hand.y = world.moves.LandHeight(UnderHand(action)) + over;
}

float openblack::computer_hand::steps::WholeSeconds(uint32_t turns)
{
	return static_cast<float>(turns / k_TurnsPerSecond);
}
