/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FollowHeadingThing.h"

#include <cmath>

#include "3D/L3DMesh.h"
#include "CHLApi.h"
#include "Creature/CreatureMode.h"
#include "Creature/CreaturePlanner.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::ecs
{

namespace
{

using components::Creature;
using components::CreatureMindState;
using components::Mesh;
using components::Transform;

/// The height of a thing as a camera reads it: a creature's body, or the height of the model of anything else
float HeightOf(const Registry& registry, entt::entity thing, const Transform& transform)
{
	if (const auto* creature = registry.TryGet<const Creature>(thing); creature != nullptr)
	{
		return creature_mode::CreatureHeight(ShownSize(*creature));
	}
	if (const auto* mesh = registry.TryGet<const Mesh>(thing); mesh != nullptr && Locator::resources::has_value())
	{
		const auto& meshes = Locator::resources::value().GetMeshes();
		if (meshes.Contains(mesh->id))
		{
			return meshes.Handle(mesh->id)->GetBoundingBox().Size().y * std::abs(transform.scale.y);
		}
	}
	return 0.0f;
}

} // namespace

FollowHeadingCameraModel::ThingLookup FollowHeadingThing(entt::entity thing)
{
	return [thing]() -> std::optional<follow_heading::Thing> {
		if (!Locator::entitiesRegistry::has_value())
		{
			return std::nullopt;
		}
		const auto& registry = Locator::entitiesRegistry::value();
		if (!registry.Valid(thing))
		{
			return std::nullopt;
		}
		const auto* transform = registry.TryGet<const Transform>(thing);
		if (transform == nullptr)
		{
			return std::nullopt;
		}
		const float height = HeightOf(registry, thing, *transform);
		bool turnsTooFast = false;
		if (const auto* mind = registry.TryGet<const CreatureMindState>(thing); mind != nullptr)
		{
			turnsTooFast = follow_heading::TurnsTooFast(creature_planner::ActionInView(mind->planner));
		}
		return follow_heading::Thing {
		    .point = transform->position + glm::vec3(0.0f, height * 0.5f, 0.0f),
		    .facing = chlapi::CameraFacingOf(thing, *transform),
		    .turnsTooFast = turnsTooFast,
		};
	};
}

} // namespace openblack::ecs
