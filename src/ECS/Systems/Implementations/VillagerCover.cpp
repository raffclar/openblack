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
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "Animals/PredatorRules.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerHiding.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/VillagerHidingRules.h"
#include "ECS/VillagerRoutine.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/ReactionRules.h"
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

/// The kind of the reaction a villager holds, none when it holds none
Reaction HeldKind(entt::entity villager)
{
	const auto* state = Entities().TryGet<const LivingReaction>(villager);
	return state != nullptr && state->reaction != 0 ? state->type : Reaction::None;
}

/// The reaction table's row of a kind
const ReactionInfo& RowOf(Reaction kind)
{
	return Locator::infoConstants::value().reaction.at(static_cast<size_t>(kind));
}

/// How a thing moves, in metres a turn: an animal by its step
glm::vec3 MovementOf(entt::entity thing)
{
	if (Entities().AllOf<Animal>(thing) && Locator::animalSystem::has_value())
	{
		return Locator::animalSystem::value().MovementOf(thing);
	}
	return glm::vec3(0.0f);
}

/// A frightened villager far enough from what frightens it, and not in its way, turns to watch it; otherwise it runs a
/// step away from it, across its way when it moves, and watches it once there
uint32_t FleeFromObjectIfComingTowardsMe(LivingAction& action, entt::entity thing, const ReactionInfo& row)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	const auto here = registry.Get<const Transform>(villager).position;
	const auto there = registry.Get<const Transform>(thing).position;
	const auto movement = MovementOf(thing);
	const bool coming = magic::ComingTowards(here, there, movement);
	auto& living = Locator::livingActionSystem::value();
	if (animals::predators::WatchesRatherThanRuns(gutils::GetDistanceInMetres(here, there), row.minDistanceToRunAwayFromObject,
	                                              coming))
	{
		living.VillagerSetState(action, LivingAction::Index::Top, VillagerStates::LookingAtObjectReaction, false);
		ecs::systems::villager_reactions::LookAt(villager, there);
		return 1;
	}
	auto to = magic::FleePointFromStill(here, there);
	if (glm::length(movement) > 0.0f)
	{
		auto& random = Locator::gameRandom::value();
		const float x = random.GameFloatRand(magic::k_FleeJitter);
		const float z = random.GameFloatRand(magic::k_FleeJitter);
		to = magic::FleePointFromMoving(here, there, movement, x, z);
	}
	if (map_coords::InBounds(map_coords::FromMetres(glm::xz(to))))
	{
		ecs::villager_home::SetupMoveTo(action, glm::xz(to), VillagerStates::LookingAtObjectReaction);
		ecs::systems::villager_reactions::SetStateSpeed(villager, living.VillagerGetFinalState(action));
	}
	return 1;
}

/// Whether a hunter is asleep or in its lair
bool Resting(entt::entity hunter)
{
	constexpr uint8_t k_Asleep = 35;
	constexpr uint8_t k_InLair = 51;
	const auto state =
	    Locator::animalSystem::has_value() ? Locator::animalSystem::value().TopGameStateOf(hunter) : std::optional<uint8_t>();
	return state.has_value() && (*state == k_Asleep || *state == k_InLair);
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

void villager_cover::ResetHidingTurns(entt::entity villager)
{
	if (auto* hidingIn = Entities().TryGet<VillagerHiding>(villager))
	{
		hidingIn->turns = 0;
	}
}

uint32_t villager_cover::FleeingFromPredatorReaction(LivingAction& action)
{
	namespace predators = animals::predators;
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	const auto hunter = ecs::systems::villager_reactions::ReactingTo(villager);
	const bool there = hunter.has_value() && registry.AllOf<Transform>(*hunter);
	const bool animal = there && registry.AllOf<Animal>(*hunter);
	if (HeldKind(villager) == Reaction::None)
	{
		return ecs::systems::villager_reactions::StopReacting(action);
	}
	predators::VillagerFleeTurn turn {.hunterThere = there, .animal = animal};
	if (animal)
	{
		const auto& animals = Locator::animalSystem::value();
		const auto here = registry.Get<const Transform>(villager).position;
		const auto at = registry.Get<const Transform>(*hunter).position;
		const auto step = animals.MovementOf(*hunter);
		turn.distance = gutils::GetDistanceInMetres(here, at);
		turn.furthestRunDistance = RowOf(HeldKind(villager)).maxDistanceToRunAwayFromObject;
		turn.fast = predators::SpeedMetresPerSecond(animals.SpeedOf(*hunter)) > predators::k_FastMetresPerSecond;
		turn.comingAt = magic::ComingTowards(here, at, step);
	}
	const auto step = predators::VillagerFleeStep(turn, IntRandom());
	if (const auto logger = spdlog::get("game"); logger != nullptr)
	{
		SPDLOG_LOGGER_DEBUG(logger, "Predator: villager {} frightened, {:.1f} m away, fast {}, coming {}, step {}",
		                    static_cast<uint32_t>(villager), turn.distance, turn.fast, turn.comingAt, static_cast<int>(step));
	}
	switch (step)
	{
	case predators::VillagerFlees::Stop:
		return ecs::systems::villager_reactions::StopReacting(action);
	case predators::VillagerFlees::Nothing:
		return 0;
	case predators::VillagerFlees::TryToHide:
		// It stays frightened of the hunter while it hides
		if (SetupGoAndHideInNearbyBuilding(action, *hunter, Reaction::FleeFromPredator))
		{
			return 1;
		}
		break;
	case predators::VillagerFlees::RunOrWatch:
		break;
	}
	return FleeFromObjectIfComingTowardsMe(action, *hunter, RowOf(HeldKind(villager)));
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
	// An animal still awake and about within the reach of the reaction it holds sends it back inside, or, with its
	// building gone, makes it run or watch
	if (const auto thing = ecs::systems::villager_reactions::ReactingTo(villager);
	    thing.has_value() && registry.AllOf<Transform>(*thing) && HeldKind(villager) != Reaction::None)
	{
		const auto& row = RowOf(HeldKind(villager));
		const float distance = gutils::GetDistanceInMetres(GroundOf(villager), GroundOf(*thing));
		if (animals::predators::StillFrightensLookout(true, distance, row.maxReactionDistance, registry.AllOf<Animal>(*thing),
		                                              Resting(*thing)))
		{
			if (CanHideIn(hidingIn->building))
			{
				hidingIn->turns = 0;
				GoBackInside(action, hidingIn->building);
				return 1;
			}
			return FleeFromObjectIfComingTowardsMe(action, *thing, row);
		}
	}
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
