/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreaturePenSystem.h"

#include <utility>
#include <vector>

#include <glm/vec3.hpp>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "Common/GUtilsDistance.h"
#include "Creature/CreatureMode.h"
#include "Creature/CreatureMorph.h"
#include "Creature/TemplePen.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

namespace
{
/// The game's world: temples' meshes from the resource cache, the land's height and the creatures' meshes
CreaturePenSystem::World GameWorld()
{
	return {
	    .penPlace = [](entt::entity temple) -> std::optional<map_coords::MapCoords> {
		    const auto& registry = Locator::entitiesRegistry::value();
		    const auto* transform = registry.TryGet<const Transform>(temple);
		    const auto* mesh = registry.TryGet<const Mesh>(temple);
		    const auto* building = registry.TryGet<const Temple>(temple);
		    if (transform == nullptr || mesh == nullptr || building == nullptr || !Locator::resources::has_value())
		    {
			    return std::nullopt;
		    }
		    const auto& meshes = Locator::resources::value().GetMeshes();
		    if (!meshes.Contains(mesh->id))
		    {
			    return std::nullopt;
		    }
		    // The pen the carry home takes a creature to
		    return creature_mode::TemplePenOnMap(transform->position, building->yAngle, transform->scale.x,
		                                         meshes.Handle(mesh->id)->GetExtraMetrics());
	    },
	    .groundAt =
	        [](glm::vec2 point) {
		        return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
	        },
	    .drawnScale = [](CreatureType species,
	                     float size) { return ecs::archetypes::CreatureArchetype::DrawnScale(species, size); },
	};
}

/// A player's temple, whose heart stands; none when the player has none
std::optional<entt::entity> TempleOf(const ecs::Registry& registry, PlayerNames owner)
{
	std::optional<entt::entity> found;
	registry.Each<const Temple>([&found, owner](entt::entity entity, const Temple& temple) {
		if (!found.has_value() && temple.owner == owner)
		{
			found = entity;
		}
	});
	return found;
}
} // namespace

CreaturePenSystem::CreaturePenSystem()
    : _world(GameWorld())
{
}

CreaturePenSystem::CreaturePenSystem(World world)
    : _world(std::move(world))
{
}

void CreaturePenSystem::ProcessTurn()
{
	ProcessTurn(Locator::entitiesRegistry::value());
}

void CreaturePenSystem::CreatureTurn(entt::entity creature)
{
	CreatureTurn(Locator::entitiesRegistry::value(), creature);
}

void CreaturePenSystem::ProcessTurn(ecs::Registry& registry) const
{
	std::vector<entt::entity> creatures;
	registry.Each<Creature, Transform>(
	    [&creatures](entt::entity entity, Creature& /*creature*/, Transform& /*transform*/) { creatures.push_back(entity); });
	for (const auto entity : creatures)
	{
		CreatureTurn(registry, entity);
	}
}

void CreaturePenSystem::CreatureTurn(ecs::Registry& registry, entt::entity entity) const
{
	if (!registry.AllOf<Creature, Transform>(entity))
	{
		return;
	}
	auto& creature = registry.Get<Creature>(entity);
	auto& transform = registry.Get<Transform>(entity);
	{
		std::optional<float> penSize;
		if (const auto temple = TempleOf(registry, creature.owner))
		{
			// While its player's temple stands, its home is the temple's pen, whatever it was before
			auto* leash = registry.TryGet<CreatureLeash>(entity);
			if (const auto pen = _world.penPlace(*temple))
			{
				if (leash == nullptr)
				{
					leash = &registry.Assign<CreatureLeash>(entity);
				}
				const glm::vec2 place {map_coords::ToMetres(pen->x), map_coords::ToMetres(pen->z)};
				leash->home = glm::vec3(place.x, _world.groundAt(place), place.y);
				leash->homeOnMap = *pen;
			}
			// The home follows a temple still going up, but the pen only shrinks the creature once the temple is built
			const auto* templeTransform = registry.TryGet<const Transform>(*temple);
			const bool built = !registry.AllOf<BuildProgress>(*temple);
			if (built && leash != nullptr && leash->home.has_value() && templeTransform != nullptr)
			{
				const auto distance = gutils::GetDistanceInMetres(transform.position, *leash->home);
				if (distance <= temple_pen::k_OuterRadius)
				{
					const auto between = temple_pen::BetweenWalls(temple_pen::MapPlace(templeTransform->position),
					                                              registry.Get<const Temple>(*temple).yAngle,
					                                              glm::vec2(transform.position.x, transform.position.z));
					if (between)
					{
						penSize = temple_pen::ShownSize(creature.size, distance, true);
					}
				}
			}
		}
		creature.penSize = penSize;
		const auto shown = ShownSize(creature);
		transform.scale = glm::vec3(_world.drawnScale(creature.species, shown));
	}
}
