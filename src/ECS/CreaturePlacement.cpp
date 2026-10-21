/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreaturePlacement.h"

#include "3D/LandIslandInterface.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/ObjectMeasures.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;

creature_land_tilt::Tilt creature_placement::TiltOf(const Registry& registry, entt::entity creature,
                                                    const components::Transform& transform)
{
	if (!Locator::terrainSystem::has_value())
	{
		return {};
	}
	const auto& land = Locator::terrainSystem::value();
	const float radius = systems::object_measures::TwoDRadius(registry, creature);
	// openblack has no creature that holds another creature, so the body always follows the land
	constexpr bool k_HeldByCreature = false;
	return creature_land_tilt::Of(transform.position, radius, k_HeldByCreature,
	                              [&land](int32_t x, int32_t z) { return land.GetAltitude(x, z); });
}

glm::mat4 creature_placement::PlacementOf(const Registry& registry, entt::entity creature,
                                          const components::Transform& transform)
{
	return creature_land_tilt::Placement(transform.position, transform.rotation, transform.scale,
	                                     TiltOf(registry, creature, transform));
}
