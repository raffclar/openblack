/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

#include <LNDFile.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/LandIslandInterface.h"
#include "Creature/CreatureDrink.h"
#include "Creature/CreatureExplorationMap.h"
#include "Creature/CreatureRoute.h"
#include "CreatureMindSystemDetail.h"
#include "ECS/Components/DrinkWaypoint.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureExplorationLand.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace land = openblack::ecs::creature_exploration_land;

namespace
{
/// Whether the cell of the land under a place has water in it. A place off the land has no water.
bool InWater(const LandIslandInterface& terrain, glm::vec2 place)
{
	const auto x = static_cast<int32_t>(std::floor(place.x / creature_route::k_CellSize));
	const auto z = static_cast<int32_t>(std::floor(place.y / creature_route::k_CellSize));
	if (x < 0 || z < 0 || x >= creature_route::k_CellsPerSide || z >= creature_route::k_CellsPerSide)
	{
		return false;
	}
	const auto* cell = terrain.FindCell(glm::u16vec2(static_cast<uint16_t>(x), static_cast<uint16_t>(z)));
	return cell != nullptr && cell->properties.hasWater != 0;
}
} // namespace

std::optional<creature_drink::Place> ecs::systems::mind_detail::DrinkPlaceOf(entt::entity creature)
{
	if (!Locator::creatureLocomotionSystem::has_value() || !Locator::terrainSystem::has_value())
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (transform == nullptr)
	{
		return std::nullopt;
	}
	const glm::vec2 from {transform->position.x, transform->position.z};
	const auto& walkable = Locator::creatureLocomotionSystem::value().GetWalkableLand();
	const auto& terrain = Locator::terrainSystem::value();
	// The point itself when the creature can stand there, else the nearest point where it can stand, if near enough
	const auto standAt = [&walkable, &terrain](glm::vec2 wanted) {
		const glm::vec3 at {wanted.x, terrain.GetHeightAt(wanted), wanted.y};
		const bool valid = walkable.IsValid(wanted, creature_route::k_DestinationClearance);
		const auto found = valid ? std::nullopt : walkable.NearestStandable(wanted);
		return creature_drink::StandingPoint(at, valid, found);
	};

	// The water points, in the order that the land made them
	std::vector<std::pair<uint32_t, glm::vec2>> marked;
	registry.Each<const DrinkWaypoint, const Transform>([&marked](const DrinkWaypoint& point, const Transform& at) {
		marked.emplace_back(point.number, glm::vec2(at.position.x, at.position.z));
	});
	std::ranges::sort(marked, {}, &std::pair<uint32_t, glm::vec2>::first);
	std::vector<glm::vec2> points;
	points.reserve(marked.size());
	std::ranges::transform(marked, std::back_inserter(points), &std::pair<uint32_t, glm::vec2>::second);
	if (const auto water = creature_drink::NearestWaterPoint(from, points))
	{
		const auto stand = standAt(*water);
		if (!stand.has_value())
		{
			return std::nullopt;
		}
		return creature_drink::Place {.walkTo = *stand};
	}

	// With no water point near, the nearest coast of its map and the sea nearest to that coast. Where the map has no
	// such place, the creature uses the low corner of the map, as in the game.
	constexpr glm::vec2 k_Corner {0.0f};
	const auto coast = land::FindPlace(creature, creature_exploration::Kind::Coast, from, false).value_or(k_Corner);
	creature_drink::Place place {
	    .sea = land::FindPlace(creature, creature_exploration::Kind::Sea, coast, false).value_or(k_Corner)};
	// A creature that stands in the water already only turns to the sea
	if (!InWater(terrain, from))
	{
		const auto stand = standAt(coast);
		if (!stand.has_value())
		{
			return std::nullopt;
		}
		place.walkTo = *stand;
	}
	return place;
}
