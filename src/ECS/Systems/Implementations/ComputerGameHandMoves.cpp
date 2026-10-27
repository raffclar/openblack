/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ComputerGameHandMoves.h"

#include <optional>

#include "3D/LandIslandInterface.h"
#include "ComputerMindCommon.h"
#include "ECS/Registry.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::computer_world;

GameHandMoves::GameHandMoves(PlayerNames player, ComputerPlayer& state)
    : _player(player)
    , _state(state)
{
}

std::optional<map_coords::MapCoords> GameHandMoves::CoordsOf(entt::entity thing) const
{
	return CoordsOfThing(thing);
}

float GameHandMoves::HeightOf(entt::entity thing) const
{
	return Entities().Valid(thing) ? ecs::world_objects::SizeOf(thing).height : 0.0f;
}

float GameHandMoves::LandHeight(const map_coords::MapCoords& coords) const
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetAltitude(coords.x, coords.z) : 0.0f;
}

float GameHandMoves::InfluenceAt(const map_coords::MapCoords& coords) const
{
	// The game has no alliances yet, so the influence of the allies is not in the sum
	return Locator::influenceSystem::has_value() ? Locator::influenceSystem::value().PlayerInfluence(_player, coords) : 0.0f;
}
