/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerFlock.h"

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "Common/GUtilsDistance.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Transform.h"
#include "ECS/Flocks.h"
#include "ECS/LivingPos.h"
#include "ECS/Registry.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/VillagerSpeed.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"

namespace openblack::ecs::villager
{
using components::Flock;
using components::LivingAction;
using components::Transform;

namespace
{
/// The position checks of CalcRandomPos: always valid for a villager
bool Always(glm::vec2 /*position*/)
{
	return true;
}

/// CalcRandomPos for a villager, with its info's collide type
glm::vec2 RandomPos(entt::entity villager, glm::vec2 centre, float rMax, glm::vec2 own)
{
	const auto* info = VillagerInfoOf(villager);
	const auto collideType = info != nullptr ? static_cast<uint32_t>(info->collideType) : 0u;
	return living::CalcRandomPos(centre, 0.0f, rMax, collideType, own, Always, Always);
}
} // namespace

uint32_t MoveInFlock(LivingAction& action)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	const auto flockEntity = flocks::FlockOf(villager);
	const auto* flock = flockEntity != entt::null ? registry.TryGet<const Flock>(flockEntity) : nullptr;
	const auto* transform = registry.TryGet<const Transform>(villager);
	if (flock == nullptr || transform == nullptr)
	{
		return 1;
	}
	const glm::vec2 me(transform->position.x, transform->position.z);
	// Within the flock's domain -> 1
	if (flocks::PosWithinDomain(villager, me, 1.0f))
	{
		return 1;
	}
	if (flocks::IsLeader(villager))
	{
		// The leader wanders to a random point of the domain
		const glm::vec2 centre(flock->domainCentre.x, flock->domainCentre.z);
		const auto p = RandomPos(villager, centre, static_cast<float>(flock->domainRadius), me);
		SetupMoveToPos(villager, p, VillagerStates::MoveInFlock);
		return 0x23;
	}
	// A member close enough to the leader (the flock's tail) stays: 0
	const auto leaderPos3 = flocks::GetFlockPos(flockEntity);
	const glm::vec2 leaderPos(leaderPos3.x, leaderPos3.z);
	const float distance = gutils::GetDistanceInMetres(leaderPos, me);
	const auto flockDistance = static_cast<float>(flock->flockDistance);
	if (!(flockDistance < distance))
	{
		return 0;
	}
	// Else a random point within flockDistance of the leader
	const auto p = RandomPos(villager, leaderPos, flockDistance, me);
	// p outside the domain while the leader is inside -> 0
	const bool leaderInside = flocks::PosWithinDomain(villager, leaderPos, 1.0f);
	if (!flocks::PosWithinDomain(villager, p, 1.0f) && leaderInside)
	{
		return 0;
	}
	SetupMoveToPos(villager, p, VillagerStates::MoveInFlock);
	return 0x23;
}

} // namespace openblack::ecs::villager
