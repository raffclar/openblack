/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ThingMapPlace.h"

#include <glm/gtx/vec_swizzle.hpp>

#include "ECS/Components/Animal.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/Firefly.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/MapPlace.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "Locator.h"

namespace openblack::ecs
{

map_coords::MapCoords thing_map_place::Of(const Registry& registry, entt::entity thing)
{
	using components::Animal;
	using components::Firefly;
	using components::Flock;
	using components::MapPlace;
	using components::Transform;
	using components::WallHug;
	if (const auto* flock = registry.TryGet<const Flock>(thing))
	{
		return {.x = flock->place.x, .z = flock->place.z};
	}
	// An animal in a hand is where the hand holds it, as all held things are, because its turns stop
	if (const auto* animal = registry.TryGet<const Animal>(thing);
	    animal != nullptr && !registry.AnyOf<components::InHand, components::HeldByCreature>(thing))
	{
		return {.x = animal->move.position.x, .z = animal->move.position.y};
	}
	if (const auto* firefly = registry.TryGet<const Firefly>(thing))
	{
		return {.x = firefly->at.x, .z = firefly->at.z};
	}
	const auto* at = registry.TryGet<const Transform>(thing);
	if (at == nullptr)
	{
		return {};
	}
	// A walker is where its walk holds it, unless something else moved it since
	const auto metres = glm::xz(at->position);
	if (const auto* walk = registry.TryGet<const WallHug>(thing); walk != nullptr && walk->placedAt == metres)
	{
		return {.x = walk->position.x, .z = walk->position.y};
	}
	// A thing that the game put at an exact map position keeps it while it stays there
	if (const auto* kept = registry.TryGet<const MapPlace>(thing); kept != nullptr && kept->placedAt == metres)
	{
		return {.x = kept->place.x, .z = kept->place.z};
	}
	return {.x = map_coords::ToFixed(metres.x), .z = map_coords::ToFixed(metres.y)};
}

void thing_map_place::Keep(Registry& registry, entt::entity thing, const map_coords::MapCoords& place)
{
	const auto* at = registry.TryGet<const components::Transform>(thing);
	if (at == nullptr)
	{
		return;
	}
	const auto metres = glm::xz(at->position);
	const bool firstPlace = !registry.AllOf<components::MapPlace>(thing);
	registry.AssignOrReplace<components::MapPlace>(
	    thing, components::MapPlace {.place = {.x = place.x, .z = place.z}, .placedAt = metres});
	if (auto* walk = registry.TryGet<components::WallHug>(thing))
	{
		walk->position = {place.x, place.z};
		walk->placedAt = metres;
	}
	if (auto* animal = registry.TryGet<components::Animal>(thing))
	{
		animal->move.position = {place.x, place.z};
	}
	// A creature that gets its first exact map position where it was made, with its home there, gets this map position as
	// its home
	auto* leash = firstPlace ? registry.TryGet<components::CreatureLeash>(thing) : nullptr;
	if (leash != nullptr && leash->home.has_value() && leash->homeOnMap.has_value() && glm::xz(*leash->home) == metres &&
	    leash->homeOnMap->x == map_coords::ToFixed(metres.x) && leash->homeOnMap->z == map_coords::ToFixed(metres.y))
	{
		leash->homeOnMap = place;
	}
}

void thing_map_place::Put(Registry& registry, entt::entity thing, const map_coords::MapCoords& place, float height)
{
	auto* at = registry.TryGet<components::Transform>(thing);
	if (at == nullptr)
	{
		return;
	}
	at->position = {map_coords::ToMetres(place.x), height, map_coords::ToMetres(place.z)};
	Keep(registry, thing, place);
	// It goes into the map cells of the new place at once, as the game moves it
	if (Locator::entitiesMap::has_value())
	{
		Locator::entitiesMap::value().Moved(thing);
	}
}

void thing_map_place::Reinsert(entt::entity thing)
{
	if (Locator::entitiesMap::has_value())
	{
		Locator::entitiesMap::value().Reinsert(thing);
	}
}

} // namespace openblack::ecs
