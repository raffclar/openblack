/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandFollowers.h"

#include "3D/LandIslandInterface.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerPose.h"
#include "ECS/Registry.h"

namespace openblack::ecs::land_followers
{

std::vector<Follower> Capture(const Registry& registry, const LandIslandInterface& land, glm::vec2 lowest, glm::vec2 highest)
{
	std::vector<Follower> followers;
	// Only the living are looked at: villagers, animals and creatures, each kind on its own
	const auto consider = [&](entt::entity entity, const components::Transform& transform) {
		const glm::vec2 at {transform.position.x, transform.position.z};
		if (at.x < lowest.x || at.y < lowest.y || at.x > highest.x || at.y > highest.y)
		{
			return;
		}
		if (!FollowsTheLand(true, registry.AllOf<components::InPhysics>(entity), registry.AllOf<components::InHand>(entity)))
		{
			return;
		}
		followers.push_back({.entity = entity, .landBefore = land.GetHeightAt(at)});
	};
	registry.Each<const components::Villager, const components::Transform>(
	    [&consider](entt::entity entity, const components::Villager& /*unused*/, const components::Transform& transform) {
		    consider(entity, transform);
	    });
	registry.Each<const components::Animal, const components::Transform>(
	    [&consider](entt::entity entity, const components::Animal& /*unused*/, const components::Transform& transform) {
		    consider(entity, transform);
	    });
	registry.Each<const components::Creature, const components::Transform>(
	    [&consider](entt::entity entity, const components::Creature& /*unused*/, const components::Transform& transform) {
		    consider(entity, transform);
	    });
	return followers;
}

void Follow(Registry& registry, const LandIslandInterface& land, std::span<const Follower> followers)
{
	if (followers.empty())
	{
		return;
	}
	for (const auto& follower : followers)
	{
		auto* transform = registry.TryGet<components::Transform>(follower.entity);
		if (transform == nullptr)
		{
			continue;
		}
		const float landAfter = land.GetHeightAt(glm::vec2(transform->position.x, transform->position.z));
		const float before = transform->position.y;
		transform->position.y = FollowedHeight(before, follower.landBefore, landAfter);
		const float change = transform->position.y - before;
		if (change == 0.0f)
		{
			continue;
		}
		// The places its drawing glides between move with it
		if (auto* pose = registry.TryGet<components::VillagerPose>(follower.entity); pose != nullptr)
		{
			if (pose->turnStart.has_value())
			{
				pose->turnStart->y += change;
			}
			if (pose->drawnAt.has_value())
			{
				pose->drawnAt->y += change;
			}
		}
		if (auto* animal = registry.TryGet<components::Animal>(follower.entity); animal != nullptr)
		{
			animal->previousPosition.y += change;
			animal->position.y += change;
		}
		if (auto* locomotion = registry.TryGet<components::CreatureLocomotion>(follower.entity); locomotion != nullptr)
		{
			locomotion->fromPosition.y += change;
			locomotion->toPosition.y += change;
		}
	}
	registry.SetDirty();
}

} // namespace openblack::ecs::land_followers
