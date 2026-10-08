/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreaturePhysics.h"

#include <utility>

#include "ECS/Components/Creature.h"
#include "ECS/Components/Transform.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Physics/PhysicsBody.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

creature_physics::BodyShape creature_physics::ShapeOf(float height)
{
	// (approximate) the original's body is the creature's bounding sphere with a point for each bone of its skeleton;
	// here a ball as tall as the creature, its six farthest points and the faces between them, facing out
	const float r = 0.5f * height;
	BodyShape shape;
	shape.points = {{r, 0.0f, 0.0f}, {-r, 0.0f, 0.0f}, {0.0f, r, 0.0f}, {0.0f, -r, 0.0f}, {0.0f, 0.0f, r}, {0.0f, 0.0f, -r}};
	shape.faces = {{0, 2, 4}, {1, 4, 2}, {0, 4, 3}, {0, 5, 2}, {1, 3, 4}, {1, 2, 5}, {0, 3, 5}, {1, 5, 3}};
	shape.centre = glm::vec3(0.0f, r, 0.0f);
	shape.radius = r;
	return shape;
}

bool creature_physics::SetUpBody(entt::entity creature, physics::PhysicsBody& body)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (!registry.Valid(creature) || !registry.AllOf<Creature, Transform>(creature))
	{
		return false;
	}
	const auto& transform = registry.Get<const Transform>(creature);
	const auto shape = ShapeOf(object::GetHeight(creature));
	body.Initialise(1.0f, shape.radius);
	body.SetUpConstants(shape.mass, physics::PhysicsObjects::Constants(physics::PhysicsObjects::ConstantsType(creature)),
	                    shape.dynamic);
	body.BuildShape(shape.points, shape.faces, shape.centre, shape.radius, 1.0f, transform.rotation, transform.position);
	return true;
}

void creature_physics::RegisterPhysicsHandlers()
{
	physics::PhysicsObjects::ClassHandlers handlers;
	handlers.setUpBody = [](entt::entity creature, physics::PhysicsBody& body) { return SetUpBody(creature, body); };
	// (approximate) the original weighs the creature's own mass, which is not read yet
	handlers.weight = [](entt::entity) { return k_Mass; };
	physics::PhysicsObjects::SetClassHandlers(physics::PhysicsClass::Creature, std::move(handlers));
}

creature_physics::Release creature_physics::ReleaseOf(bool atTarget, entt::entity creature)
{
	return atTarget && creature != entt::null ? Release::AtTarget : Release::LetGo;
}
