/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VortexCrossingLines.h"

#include <cmath>

#include <algorithm>
#include <string>

#include <glm/gtx/euler_angles.hpp>
#include <glm/mat4x4.hpp>

#include "ECS/Components/Animal.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/VillagerAge.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::vortex::crossing;

namespace
{
/// A town's number, -1 for none
int32_t TownNumber(const ecs::Registry& registry, entt::entity town)
{
	const auto* data = registry.Valid(town) ? registry.TryGet<const Town>(town) : nullptr;
	return data != nullptr ? static_cast<int32_t>(data->id) : -1;
}

/// Turned only about its up axis by an angle a, as the things made that way are: the angle back
float YAngleOf(const glm::mat3& rotation)
{
	// Taken from 0 so that no turn reads as -0
	return 0.0f - std::atan2(rotation[2][0], rotation[0][0]);
}

/// Turned about x, then y, then z: the three angles back
glm::vec3 AnglesOf(const glm::mat3& rotation)
{
	glm::vec3 negated {0.0f};
	glm::extractEulerAngleXYZ(glm::mat4(rotation), negated.x, negated.y, negated.z);
	return glm::vec3(0.0f) - negated;
}

bool PartOfStoragePit(const ecs::Registry& registry, entt::entity pot)
{
	bool part = false;
	registry.Each<const StoragePit>([pot, &part](const StoragePit& pit) {
		part = part || pit.foodPile == pot || std::ranges::find(pit.woodPiles, pot) != pit.woodPiles.end();
	});
	return part;
}
} // namespace

map_coords::MapCoords ecs::vortex_crossing::MapPlace(glm::vec3 position, float ground)
{
	auto place = map_coords::FromMetres({position.x, position.z});
	place.altitude = position.y - ground;
	return place;
}

std::optional<Line> ecs::vortex_crossing::LineFor(const Registry& registry, entt::entity thing,
                                                  const map_coords::MapCoords& vortex, const World& world)
{
	const auto* transform = registry.Valid(thing) ? registry.TryGet<const Transform>(thing) : nullptr;
	if (transform == nullptr)
	{
		return std::nullopt;
	}
	const auto& position = transform->position;
	const auto offset = MapPlace(position, world.ground({position.x, position.z})) - vortex;
	const float scale = transform->scale.x;
	if (const auto* villager = registry.TryGet<const Villager>(thing))
	{
		const auto town = TownNumber(registry, villager->town);
		const auto age = villager_age::AgeOf(world.turn, villager->birthTurn);
		// One of no tribe is written by its row of the villagers' table
		if (villager->tribe == Tribe::NONE)
		{
			return SpecialVillagerLine {
			    .town = town, .offset = offset, .row = world.villagerRow(villager->tribe, villager->number), .age = age};
		}
		std::string type(k_TribeStrs.at(static_cast<size_t>(villager->tribe)));
		type += '_';
		type += k_VillagerNumberStrs.at(static_cast<size_t>(villager->number));
		return VillagerLine {.town = town, .offset = offset, .villagerType = type, .age = age};
	}
	if (const auto* animal = registry.TryGet<const Animal>(thing))
	{
		const auto* flock = registry.Valid(animal->flock) ? registry.TryGet<const Flock>(animal->flock) : nullptr;
		const auto* grazer = registry.TryGet<const Grazer>(thing);
		// TODO(vortex-crossing): the ages of the animals that aren't grazers aren't kept
		const uint32_t age = grazer != nullptr ? villager_age::AgeOf(world.turn, grazer->birthTurn) : 0;
		return AnimalLine {.offset = offset,
		                   .type = static_cast<int32_t>(animal->type),
		                   .flock = flock != nullptr ? flock->scriptId.value_or(-1) : -1,
		                   .town = flock != nullptr ? TownNumber(registry, flock->town) : -1,
		                   .age = age};
	}
	if (const auto* pot = registry.TryGet<const Pot>(thing))
	{
		// A pile of a storage pit stays with it
		if (PartOfStoragePit(registry, thing))
		{
			return std::nullopt;
		}
		return PotLine {.offset = offset,
		                .row = static_cast<int32_t>(pot->type),
		                .resource = world.potResource(pot->type),
		                .amount = pot->amount};
	}
	if (const auto* tree = registry.TryGet<const Tree>(thing))
	{
		return TreeLine {.offset = offset,
		                 .row = static_cast<int32_t>(tree->type),
		                 .nonScenic = tree->nonScenic,
		                 .yAngle = YAngleOf(transform->rotation),
		                 .scale = scale,
		                 .maxScale = tree->maxSize};
	}
	if (const auto* still = registry.TryGet<const MobileStatic>(thing))
	{
		const auto angles = AnglesOf(transform->rotation);
		return MobileStaticLine {.offset = offset,
		                         .row = static_cast<int32_t>(still->type),
		                         .xAngle = angles.x,
		                         .yAngle = angles.y,
		                         .zAngle = angles.z,
		                         .scale = scale};
	}
	if (const auto* mobile = registry.TryGet<const MobileObject>(thing))
	{
		return MobileObjectLine {.offset = offset,
		                         .row = static_cast<int32_t>(mobile->type),
		                         .yAngle = YAngleOf(transform->rotation),
		                         .scale = scale};
	}
	if (const auto* dead = registry.TryGet<const DeadTree>(thing))
	{
		// Nobody's dead tree is the neutral player's
		const auto player = world.player(thing).value_or(PlayerNames::NEUTRAL);
		const auto angles = AnglesOf(transform->rotation);
		return DeadTreeLine {.offset = offset,
		                     .player = std::string(k_PlayerNamesStrs.at(static_cast<size_t>(player))),
		                     .row = static_cast<int32_t>(dead->type),
		                     .life = world.life(thing),
		                     .xAngle = angles.x,
		                     .yAngle = angles.y,
		                     .zAngle = angles.z};
	}
	return std::nullopt;
}
