/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerArtefactDance.h"

#include <algorithm>
#include <array>

#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
#include "Common/GameRandom.h"
#include "ECS/ArtefactRules.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Dances.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "VillagerDance.h"
#include "VillagerHome.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace rules = openblack::ecs::artefact_rules;
using ClearAreaFilter = openblack::ecs::systems::TownSystemInterface::ClearAreaFilter;

namespace
{
/// The states that share leaving a dance: moving between them keeps a villager in its dance
constexpr std::array k_DanceStates {
    VillagerStates::WorshippingCreature, VillagerStates::DanceForEditingPurposes, VillagerStates::DanceButNotWorship,
    VillagerStates::ArtifactDance,       VillagerStates::WaitForArtifactDance,
};

ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

const GVillagerInfo& InfoOf(entt::entity villager)
{
	const auto& person = Entities().Get<const Villager>(villager);
	return Locator::infoConstants::value().villager.at(static_cast<size_t>(GVillagerInfo::Find(person.tribe, person.number)));
}

/// The villager walks to a point and goes on waiting there
void WalkThenWait(LivingAction& action, glm::vec2 goal)
{
	auto& living = Locator::livingActionSystem::value();
	if (living.VillagerSetCurrentAndDestinationState(action, VillagerStates::MoveToPos, VillagerStates::WaitForArtifactDance))
	{
		ecs::villager_home::SetupMobileMoveTo(action, goal);
	}
}
} // namespace

uint32_t ecs::villager_artefact_dance::WaitForArtefactDance(LivingAction& action)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	const auto dance = dances::DanceOf(registry, villager);
	auto& living = Locator::livingActionSystem::value();
	if (dance == entt::null)
	{
		living.VillagerSetState(action, LivingAction::Index::Top, VillagerStates::DecideWhatToDo, false);
		return 1;
	}
	const auto& data = registry.Get<Dance>(dance);
	const auto at = glm::xz(registry.Get<const Transform>(villager).position);
	const auto middle = map_coords::ToMetres(data.place);
	// It keeps a little more than its own width clear about itself
	const float room = world_objects::SizeOf(villager).radius * rules::k_WaitingRoom;
	const float distance = glm::distance(middle, at);
	const auto& towns = Locator::townSystem::value();
	// Whether it has room where it stands is only asked on its first turn of waiting close by
	const bool askRoom = distance <= rules::k_WaitingReach && action.turnsUntilStateChange == 0;
	const bool roomHere = askRoom && towns.CheckForClearArea(at, room, ClearAreaFilter::AnyObject, villager);
	switch (rules::WaitingTurn(distance, action.turnsUntilStateChange, roomHere, InfoOf(villager).timeToWaitForArtifactDance))
	{
	case rules::Waiting::WalkCloser:
	{
		// To a clear spot up to 12 m from the artefact, on its own side of it, a little to one side or the other
		auto& random = Locator::gameRandom::value();
		const float spread = random.GameFloatRand(rules::k_WaitingSpread);
		const float angle = gutils::Get3DAngleFromXZ(middle, at) + spread - rules::k_WaitingHalfSpread;
		const float away = random.GameFloatRand(rules::k_WaitingReach);
		const auto target = map_coords::ToMetres(data.place + gutils::GetPosFromAngle(angle, away));
		if (const auto spot = towns.FindClearArea(target, rules::k_WaitingReach, rules::k_WaitingRoomStep, room,
		                                          ClearAreaFilter::AnyObject, villager);
		    spot.has_value())
		{
			WalkThenWait(action, *spot);
		}
		break;
	}
	case rules::Waiting::MakeRoom:
		if (const auto spot = towns.FindClearArea(at, rules::k_WaitingReach, rules::k_WaitingRoomStep, room,
		                                          ClearAreaFilter::AnyObject, villager);
		    spot.has_value())
		{
			WalkThenWait(action, *spot);
		}
		break;
	case rules::Waiting::Wait:
		++action.turnsUntilStateChange;
		break;
	case rules::Waiting::StartDance:
	{
		++action.turnsUntilStateChange;
		auto& started = registry.Get<Dance>(dance);
		started.state = Dance::State::Dancing;
		started.startTurn = Locator::time::value().GetTurn();
		break;
	}
	}
	return 1;
}

bool ecs::villager_artefact_dance::EnterWaitForArtefactDance(LivingAction& action, VillagerStates /*previous*/,
                                                             VillagerStates /*next*/)
{
	action.turnsUntilStateChange = 0;
	return true;
}

uint32_t ecs::villager_artefact_dance::ArtefactDance(LivingAction& action)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	const auto dance = dances::DanceOf(registry, villager);
	if (dance != entt::null)
	{
		// One standing right at the dance's middle walks straight to its place
		const auto& place = registry.Get<const Dance>(dance).place;
		const auto at = map_coords::FromMetres(glm::xz(registry.Get<const Transform>(villager).position));
		const auto slot = villager_dance::SlotOf(villager);
		if (slot.has_value() && map_coords::FromMetres(*slot) != at && at.x == place.x && at.z == place.z)
		{
			auto& living = Locator::livingActionSystem::value();
			if (living.VillagerSetCurrentAndDestinationState(action, VillagerStates::MoveToPos, VillagerStates::ArtifactDance))
			{
				villager_home::SetupMobileMoveTo(action, *slot);
			}
			return 1;
		}
	}
	return villager_dance::PerformDance(action, VillagerStates::ArtifactDance);
}

bool ecs::villager_artefact_dance::ExitDance(LivingAction& action, VillagerStates next)
{
	// Into another dance's state, or a script's dance, it stays in its dance
	if (next != VillagerStates::InDance && std::ranges::find(k_DanceStates, next) == k_DanceStates.end())
	{
		auto& registry = Entities();
		dances::RemoveDancer(registry, registry.ToEntity(action));
	}
	return false;
}
