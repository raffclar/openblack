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
#include "ECS/Components/Firefly.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"

namespace openblack::ecs
{

map_coords::MapCoords thing_map_place::Of(const Registry& registry, entt::entity thing)
{
	using components::Animal;
	using components::Firefly;
	using components::Flock;
	using components::Transform;
	using components::WallHug;
	if (const auto* flock = registry.TryGet<const Flock>(thing))
	{
		return {.x = flock->place.x, .z = flock->place.z};
	}
	if (const auto* animal = registry.TryGet<const Animal>(thing))
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
	return {.x = map_coords::ToFixed(metres.x), .z = map_coords::ToFixed(metres.y)};
}

} // namespace openblack::ecs
