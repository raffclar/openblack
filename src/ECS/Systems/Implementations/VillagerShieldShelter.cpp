/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerShieldShelter.h"

#include <cmath>

#include <numbers>

#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "Common/GameRandom.h"
#include "Common/PortableRotations.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/LivingFacing.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/TownDesire.h"
#include "ECS/VillagerClips.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/ShieldRules.h"
#include "VillagerAnimate.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace villager_shield = openblack::ecs::villager_shield;
namespace villager_animate = openblack::ecs::villager_animate;
namespace villager_clips = openblack::ecs::villager_clips;
namespace shield = openblack::magic::shield;

namespace
{
constexpr float k_TwoPi = 2.0f * std::numbers::pi_v<float>;
constexpr size_t k_ForProtection = 3;

/// The way a villager faces, as an angle across the land, from how it is turned: walking turns it by
/// eulerAngleY(-angle - a quarter turn), so that angle is undone here
float FacingOf(const Transform& transform)
{
	const auto x = transform.rotation * glm::vec3(1.0f, 0.0f, 0.0f);
	return -portable_math::Atan2(-x.z, x.x) - glm::radians(90.0f);
}

/// Turns a villager towards a point by at most its turn a game turn; whether it faces it
bool LookAtPos(entt::entity villager, const Transform& transform, glm::vec2 point)
{
	const auto towards = point - glm::xz(transform.position);
	const float target = portable_math::Atan2(towards.y, towards.x);
	const float step = shield::k_LookTurnPerTurn * k_TwoPi;
	const float facing = FacingOf(transform);
	const float diff = std::remainder(target - facing, k_TwoPi);
	auto& registry = Locator::entitiesRegistry::value();
	if (std::abs(diff) < step)
	{
		openblack::ecs::living_facing::FaceAngle(registry, villager, target);
		return true;
	}
	openblack::ecs::living_facing::FaceAngle(registry, villager, facing + (diff > 0.0f ? step : -step));
	return false;
}

void SetupWaitForCounter(LivingAction& action, uint16_t counter, VillagerStates state)
{
	// It waits, then goes into the state; the wait is counted only once it is waiting
	if (Locator::livingActionSystem::value().VillagerSetCurrentAndDestinationState(action, VillagerStates::WaitForCounter,
	                                                                               state))
	{
		action.turnsUntilStateChange = counter;
	}
}
} // namespace

float villager_shield::ProtectionSignificance(entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* desire = town != entt::null && registry.Valid(town) ? registry.TryGet<const TownDesire>(town) : nullptr;
	if (desire == nullptr || !Locator::infoConstants::has_value())
	{
		return 0.0f;
	}
	return town_desire::GetDesireSignificanceToVillager(*desire, Locator::infoConstants::value().townDesire.at(k_ForProtection),
	                                                    k_ForProtection);
}

uint32_t villager_shield::AmazedByMagicShield(LivingAction& action)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& random = Locator::gameRandom::value();
	const auto villager = registry.ToEntity(action);
	auto* reacting = registry.TryGet<VillagerShieldReaction>(villager);
	const auto* person = registry.TryGet<const Villager>(villager);
	const auto town = person != nullptr ? person->town : entt::null;
	const auto* standing = reacting != nullptr && registry.Valid(reacting->shield)
	                           ? registry.TryGet<const MagicShield>(reacting->shield)
	                           : nullptr;
	if (town != entt::null && registry.Valid(town) && standing != nullptr && standing->spell != entt::null &&
	    ProtectionSignificance(town) > 0.0f)
	{
		auto& transform = registry.Get<Transform>(villager);
		const auto centre = glm::xz(registry.Get<const Transform>(reacting->shield).position);
		// It looks out through the shield, now and then somewhere else once it faces where it looked
		if (LookAtPos(villager, transform, reacting->lookAt) && random.GameRand(4) == 0)
		{
			reacting->lookAt =
			    shield::LookOut(centre, glm::xz(transform.position), random.GameFloatRand(shield::k_ShelterTurnRange));
		}
		registry.SetDirty();
		// The state chooses its clip each turn. It takes up the clip after 20 to 30 seconds of the clip that plays, or at
		// once after the clip that points up, which talking and pointing follows.
		const float seconds = random.GameFloatRand(shield::k_AnimationSecondsRange) + shield::k_AnimationSecondsLeast;
		const auto current = villager_animate::CurrentClip(villager);
		const auto length = villager_clips::ClipMilliseconds(current);
		auto clip = villager_animate::StateClip(villager);
		const bool pointing = static_cast<uint32_t>(current) == shield::k_PointingAnimation;
		if (pointing)
		{
			clip = static_cast<int32_t>(shield::k_TalkingAndPointingAnimation);
		}
		if (length.has_value() || pointing)
		{
			const auto times = shield::AmazedClipTimes(seconds, static_cast<int32_t>(length.value_or(0.0f)), pointing);
			if (villager_animate::IsReadyForNewAnimation(action, static_cast<uint32_t>(times)))
			{
				villager_animate::SetAnim(villager, clip, true);
				action.turnsSinceStateChange = 0;
			}
		}
		return 1;
	}
	// The shield has gone, or the town no longer wants protection: it waits a while, then decides what to do
	const auto wait = static_cast<uint16_t>(static_cast<int32_t>(
	    static_cast<float>(random.GameRand(shield::k_GiveUpWaitRange)) + static_cast<float>(shield::k_GiveUpWaitLeast)));
	SetupWaitForCounter(action, wait, VillagerStates::DecideWhatToDo);
	return 1;
}

uint32_t villager_shield::WaitForCounter(LivingAction& action)
{
	auto& living = Locator::livingActionSystem::value();
	action.turnsUntilStateChange = static_cast<uint16_t>(action.turnsUntilStateChange - 1);
	if (static_cast<int16_t>(action.turnsUntilStateChange) < 1)
	{
		auto final = living.VillagerGetState(action, LivingAction::Index::Final);
		if (final == VillagerStates::InvalidState)
		{
			final = VillagerStates::DecideWhatToDo;
		}
		living.VillagerSetState(action, LivingAction::Index::Final, VillagerStates::InvalidState, true);
		living.VillagerSetState(action, LivingAction::Index::Top, final, false);
	}
	return 1;
}
