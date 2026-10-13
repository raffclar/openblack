/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerCover.h"

#include <vector>

#include <glm/gtx/vec_swizzle.hpp>

#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerHiding.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/VillagerHidingRules.h"
#include "ECS/VillagerRoutine.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "VillagerBuild.h"
#include "VillagerFootpath.h"
#include "VillagerHome.h"
#include "VillagerReactions.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace villager_cover = openblack::ecs::villager_cover;
namespace hiding = openblack::ecs::villager_hiding;

namespace
{
ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

glm::vec2 GroundOf(entt::entity entity)
{
	return glm::xz(Entities().Get<const Transform>(entity).position);
}

auto IntRandom()
{
	return [](uint32_t limit) { return Locator::gameRandom::value().GameRand(limit); };
}

/// Whether a building is still there and can hide a villager
bool CanHideIn(entt::entity building)
{
	auto& registry = Entities();
	if (building == entt::null || !registry.Valid(building) || !registry.AllOf<Abode, Transform>(building))
	{
		return false;
	}
	const auto* info = ecs::world_objects::AbodeInfoOf(building);
	return info != nullptr && hiding::CanBeHiddenIn(info->abodeType, ecs::villager_home::IsFunctional(building));
}

/// Whether what frightened a villager is still about: a shield only while its miracle stands
bool ThreatAvailable(entt::entity threat)
{
	auto& registry = Entities();
	if (threat == entt::null || !registry.Valid(threat) || !registry.AllOf<Transform>(threat))
	{
		return false;
	}
	const auto* shield = registry.TryGet<const MagicShield>(threat);
	return shield == nullptr || shield->spell != entt::null;
}

/// The villager stops being frightened and goes back to what it was doing
uint32_t StopHiding(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	Entities().Remove<VillagerHiding>(villager);
	ecs::systems::villager_reactions::StopReacting(action);
	return 1;
}

void GoBackInside(LivingAction& action, entt::entity building)
{
	ecs::villager_footpath::SetupMoveToOnFootpath(action, building, ecs::villager_home::ArrivePosition(building),
	                                              VillagerStates::GoAndHideInNearbyBuilding);
}
} // namespace

bool villager_cover::SetupGoAndHideInNearbyBuilding(LivingAction& action, entt::entity threat, Reaction threatKind)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	const auto& person = registry.Get<const Villager>(villager);
	if (person.town == entt::null || !registry.Valid(person.town))
	{
		return false;
	}
	const auto& buildings = registry.Get<const Town>(person.town).abodes;
	const auto here = GroundOf(villager);
	std::vector<hiding::Shelter> shelters;
	shelters.reserve(buildings.size());
	for (const auto building : buildings)
	{
		const bool hides = CanHideIn(building);
		shelters.push_back(
		    {.distance = hides ? gutils::GetDistanceInMetres(here, ecs::villager_home::ArrivePosition(building)) : 0.0f,
		     .canBeHiddenIn = hides});
	}
	const auto nearest = hiding::NearestShelter(shelters);
	if (!nearest.has_value())
	{
		return false;
	}
	const auto building = buildings.at(*nearest);
	registry.AssignOrReplace<VillagerHiding>(
	    villager, VillagerHiding {.building = building, .turns = 0, .threat = threat, .threatKind = threatKind});
	GoBackInside(action, building);
	return true;
}

void villager_cover::SetPanicPoint(entt::entity villager, glm::vec2 point)
{
	Entities().AssignOrReplace<VillagerPanic>(villager, VillagerPanic {.point = point});
}

uint32_t villager_cover::GoAndHideInNearbyBuilding(LivingAction& action)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	auto* hidingIn = registry.TryGet<VillagerHiding>(villager);
	if (hidingIn == nullptr)
	{
		return StopHiding(action);
	}
	// What frightened it is forgotten once it is gone
	if (hidingIn->threat != entt::null && !ThreatAvailable(hidingIn->threat))
	{
		hidingIn->threat = entt::null;
	}
	const bool stands = CanHideIn(hidingIn->building);
	const bool atDoor =
	    stands && ecs::villager_home::AreWeThere(villager, ecs::villager_home::ArrivePosition(hidingIn->building));
	bool threatNear = false;
	if (stands && atDoor && hidingIn->threat != entt::null)
	{
		const auto& row = Locator::infoConstants::value().reaction.at(static_cast<size_t>(hidingIn->threatKind));
		threatNear =
		    gutils::GetDistanceInMetres(GroundOf(villager), GroundOf(hidingIn->threat)) <= row.minDistanceToRunAwayFromObject;
	}
	const auto step = hiding::HideStep(
	    {.shelterStands = stands, .atDoor = atDoor, .threatNear = threatNear, .turnsInside = hidingIn->turns}, IntRandom());
	hidingIn->turns = step.turnsInside;
	switch (step.what)
	{
	case hiding::Hiding::GiveUp:
		return StopHiding(action);
	case hiding::Hiding::WalkToDoor:
		GoBackInside(action, hidingIn->building);
		return 1;
	case hiding::Hiding::ComeOut:
	{
		// It steps out of the door and looks about
		const auto building = hidingIn->building;
		const auto out = ecs::villager_routine::PosOutsideDoor(
		    GroundOf(building), ecs::villager_home::ArrivePosition(building),
		    [](float limit) { return Locator::gameRandom::value().GameFloatRand(limit); });
		ecs::villager_home::SetupMoveTo(action, out, VillagerStates::LookToSeeIfItIsSafe);
		return 1;
	}
	case hiding::Hiding::StayWhileThreatNear:
	case hiding::Hiding::Stay:
		return 1;
	}
	return 1;
}

uint32_t villager_cover::LookToSeeIfItIsSafe(LivingAction& action)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	auto* hidingIn = registry.TryGet<VillagerHiding>(villager);
	if (hidingIn == nullptr)
	{
		return StopHiding(action);
	}
	// TODO(villagers): a predator still awake and about within the reaction's reach sends it back inside, or makes it
	// run if its building is gone; openblack has no villager reactions to predators yet. Nothing else frightening it is
	// looked out for here.
	const auto step = hiding::LookAbout(hidingIn->turns, IntRandom());
	hidingIn->turns = step.turns;
	if (step.safe)
	{
		return StopHiding(action);
	}
	return 1;
}

uint32_t villager_cover::PanicReaction(LivingAction& action)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	const auto* panic = registry.TryGet<const VillagerPanic>(villager);
	const auto point = panic != nullptr ? panic->point : GroundOf(villager);
	// It turns quickly to look back at what it ran from, then stands a moment and decides afresh
	if (ecs::villager_build::FaceTowards(villager, point, hiding::k_PanicLookStep))
	{
		registry.Remove<VillagerPanic>(villager);
		Locator::livingActionSystem::value().VillagerPlayAnimThenSetState(action, VillagerStates::DecideWhatToDo);
	}
	return 1;
}
