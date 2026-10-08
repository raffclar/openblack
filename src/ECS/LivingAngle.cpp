/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LivingAngle.h"

#include <cmath>

#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/VillagerSpeed.h"
#include "Locator.h"

namespace openblack::ecs::living
{

void SetYAngle(entt::entity entity, float radians)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<components::Villager>(entity))
	{
		villager::SetYAngle(entity, radians);
	}
	else if (registry.AllOf<components::Animal>(entity))
	{
		animal_ai::SetYAngle(entity, radians);
	}
}

bool SetFocus(entt::entity entity, const glm::vec3& target)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const components::Transform>(entity);
	if (transform == nullptr || !registry.AnyOf<components::Villager, components::Animal>(entity))
	{
		return false;
	}
	// its position in map coordinates, in metres; the angle does not read y. (inferred) openblack's Transform position
	// stands for it, through map coordinates
	const glm::vec3 from(map_coords::Quantise(transform->position.x), transform->position.y,
	                     map_coords::Quantise(transform->position.z));
	// the angle from the position to the target, rounded to a float, then SetYAngle
	SetYAngle(entity, static_cast<float>(affine::GetYAngleBetween(from, target)));
	return true;
}

std::optional<float> SpeedProperty(entt::entity entity)
{
	return SpeedProperty(entity, physics::PhysicsObjects::IsFlying(entity), physics::PhysicsObjects::Find(entity));
}

std::optional<float> SpeedProperty(entt::entity entity, bool inPhysics, const physics::PhysicsObject* po)
{
	// any object in the physics with its PhysicsObject
	if (inPhysics && po != nullptr)
	{
		const auto& v = po->body.velocity;
		return std::sqrt((v.z * v.z + v.y * v.y) + v.x * v.x);
	}
	auto& registry = Locator::entitiesRegistry::value();
	const bool villager = registry.AllOf<components::Villager>(entity);
	const bool animal = registry.AllOf<components::Animal>(entity);
	if (!villager && !animal)
	{
		return std::nullopt;
	}
	const auto speedInMetres = [&]() { return villager ? VillagerSpeedInMetres(entity) : animal_ai::GetSpeedInMetres(entity); };
	if (inPhysics)
	{
		return speedInMetres(); // no dead test
	}
	// dead -> 0
	const bool dead = villager ? villager::IsDead(entity) : animal_ai::IsDead(entity);
	return dead ? 0.0f : speedInMetres();
}

std::optional<float> AgeProperty(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<components::Villager>(entity))
	{
		return static_cast<float>(villager::GetAge(entity));
	}
	if (registry.AllOf<components::Animal>(entity))
	{
		return static_cast<float>(animal_ai::GetAge(entity));
	}
	return std::nullopt;
}

} // namespace openblack::ecs::living
