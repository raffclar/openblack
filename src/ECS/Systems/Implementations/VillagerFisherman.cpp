/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerFisherman.h"

#include <array>
#include <optional>

#include <glm/gtx/vec_swizzle.hpp>

#include "3D/MapCoords.h"
#include "Animals/FishFarmRules.h"
#include "Common/GUtilsDistance.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerWork.h"
#include "ECS/Registry.h"
#include "ECS/Systems/FishFarmSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "ECS/VillagerCarry.h"
#include "ECS/VillagerWorkRules.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "VillagerBuild.h"
#include "VillagerHome.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace villager_fisherman = openblack::ecs::villager_fisherman;
namespace work = openblack::ecs::villager_work;

namespace
{
/// How near a field is to a farmer counts for less the further it is, out to this many metres
constexpr float k_FieldReach = 300.0f;
/// How near the store is counts for less the further it is, out to this many metres, as a villager weighs taking its
/// food there
constexpr float k_DropOffReach = 500.0f;

ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

entt::entity EntityOf(LivingAction& action)
{
	return Entities().ToEntity(action);
}

Villager& VillagerOf(entt::entity villager)
{
	return Entities().Get<Villager>(villager);
}

const GVillagerInfo& InfoOf(const Villager& villager)
{
	const auto& infos = Locator::infoConstants::value().villager;
	return infos.at(static_cast<size_t>(GVillagerInfo::Find(villager.tribe, villager.number)));
}

entt::entity TownOf(const Villager& villager)
{
	const auto& registry = Entities();
	return villager.town != entt::null && registry.Valid(villager.town) && registry.AllOf<Town>(villager.town) ? villager.town
	                                                                                                           : entt::null;
}

glm::vec3 PositionOf(entt::entity entity)
{
	return Entities().Get<const Transform>(entity).position;
}

void SetTopState(LivingAction& action, VillagerStates state)
{
	Locator::livingActionSystem::value().VillagerSetState(action, LivingAction::Index::Top, state, false);
}

void MoveTo(LivingAction& action, glm::vec2 goal, VillagerStates final)
{
	if (Locator::livingActionSystem::value().VillagerSetCurrentAndDestinationState(action, VillagerStates::MoveToPos, final))
	{
		villager_home::SetupMobileMoveTo(action, goal);
	}
}

void MoveOnPathsTo(LivingAction& action, glm::vec2 goal, VillagerStates final)
{
	// TODO(villager-jobs-1): along the town's footpaths where there are any; villagers don't walk footpaths yet
	MoveTo(action, goal, final);
}

entt::entity FarmOf(entt::entity villager)
{
	const auto* target = Entities().TryGet<const WorkTarget>(villager);
	return target != nullptr && Entities().Valid(target->thing) && Entities().AllOf<FishFarm>(target->thing) ? target->thing
	                                                                                                         : entt::null;
}

/// Where a fisherman walks to: the farm's own place, the middle of its map cell
glm::vec2 ArrivePositionOf(entt::entity farm)
{
	return map_coords::ToMetres(map_coords::MapCoords {Entities().Get<const FishFarm>(farm).place.x,
	                                                   Entities().Get<const FishFarm>(farm).place.y, 0.0f});
}

/// The best of its town's fields for the villager, by how much each wants farming and how near it is
work::FoodJobCandidate BestField(entt::entity villager, entt::entity town)
{
	work::FoodJobCandidate best {.job = work::FoodJob::Field};
	const auto* record = Entities().TryGet<const Town>(town);
	if (record == nullptr)
	{
		return best;
	}
	const auto& types = Locator::infoConstants::value().fieldType;
	const auto here = glm::xz(PositionOf(villager));
	Entities().Each<const Field, const Transform>([&](const Field& field, const Transform& at) {
		if (field.town != static_cast<int>(record->id))
		{
			return;
		}
		const auto& type = types.at(static_cast<size_t>(field.type));
		// TODO(villager-jobs-1): fields keep no farmers in openblack yet, so none is ever crowded
		const float desire = work::DesireToBeFarmed({.timesSown = field.crop.timesSown,
		                                             .timesToSow = type.timesToSow,
		                                             .age = field.crop.age,
		                                             .ageGrowth = type.ageGrowth,
		                                             .ageRecolt = type.ageRecolt,
		                                             .farmers = 0,
		                                             .maxFarmers = type.maxFarmerInFarm});
		const float score =
		    gutils::GetDistanceModifier(gutils::GetDistanceInMetres(glm::xz(at.position), here), k_FieldReach) * desire;
		if (score > best.score)
		{
			best.score = score;
			best.found = true;
		}
	});
	return best;
}
} // namespace

uint32_t villager_fisherman::CheckSatisfyFoodDesire(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto& person = VillagerOf(villager);
	const auto town = TownOf(person);
	if (town == entt::null)
	{
		return 0;
	}
	const auto here = PositionOf(villager);
	std::optional<entt::entity> farm;
	work::FoodJobCandidate fishing {.job = work::FoodJob::FishFarm};
	if (Locator::fishFarmSystem::has_value())
	{
		if (const auto choice = Locator::fishFarmSystem::value().BestFarmFor(town, here))
		{
			farm = choice->farm;
			fishing.score = choice->score;
			fishing.found = true;
		}
	}
	// TODO(villager-jobs-1): the town's flocks for shepherds are weighed third; openblack has no shepherds yet
	const std::array<work::FoodJobCandidate, 3> jobs {fishing, BestField(villager, town),
	                                                  work::FoodJobCandidate {.job = work::FoodJob::Flock}};
	const auto& info = InfoOf(person);
	const auto room = static_cast<int16_t>(info.maxFoodCarried - static_cast<uint32_t>(person.foodHeld));
	float storeNearness = 0.0f;
	const auto pit = construction::StoragePitOf(town);
	if (pit.has_value() && villager_home::IsFunctional(*pit))
	{
		storeNearness = gutils::GetDistanceModifier(
		    gutils::GetDistanceInMetres(villager_home::ArrivePosition(*pit), glm::xz(here)), k_DropOffReach);
	}
	// TODO(villager-jobs-1): without a working storage pit the town's temporary pot is where it would carry food
	const auto choice = work::ChooseFoodJob(jobs, room, static_cast<int32_t>(info.maxFoodCarried), storeNearness);
	if (choice.dropOff)
	{
		return villager_build::GotoStoragePitForDropOff(action);
	}
	if (!choice.job.has_value())
	{
		return 0;
	}
	switch (*choice.job)
	{
	case work::FoodJob::FishFarm:
		return VillagerBecomesFisherman(action, *farm);
	case work::FoodJob::Field:
		// TODO(villager-jobs-1): farming isn't in openblack yet; a villager the town would send to a field does nothing
		// for its want of food
		return 0;
	case work::FoodJob::Flock:
		return 0;
	}
	return 0;
}

uint32_t villager_fisherman::FishermanLookForWater(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto town = TownOf(VillagerOf(villager));
	if (town == entt::null || !Locator::fishFarmSystem::has_value())
	{
		return 0;
	}
	const auto choice = Locator::fishFarmSystem::value().BestFarmFor(town, PositionOf(villager));
	if (!choice.has_value())
	{
		return 0;
	}
	VillagerBecomesFisherman(action, choice->farm);
	return 1;
}

uint32_t villager_fisherman::VillagerBecomesFisherman(LivingAction& action, entt::entity farm)
{
	const auto villager = EntityOf(action);
	SetTopState(action, VillagerStates::DecideWhatToDo);
	Entities().AssignOrReplace<WorkTarget>(villager, WorkTarget {.thing = farm});
	MoveOnPathsTo(action, ArrivePositionOf(farm), VillagerStates::FishermanArrivesAtFishing);
	return 1;
}

uint32_t villager_fisherman::FishermanArrivesAtFishing(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto farm = FarmOf(villager);
	if (farm == entt::null)
	{
		SetTopState(action, VillagerStates::DecideWhatToDo);
		return 1;
	}
	const auto& place = Entities().Get<const FishFarm>(farm).place;
	const auto here = map_coords::FromMetres(glm::xz(PositionOf(villager)));
	switch (work::ArriveAtFishing(here, {place.x, place.y, 0.0f}))
	{
	case work::FishingArrival::WalkAgain:
		MoveOnPathsTo(action, ArrivePositionOf(farm), VillagerStates::FishermanArrivesAtFishing);
		break;
	case work::FishingArrival::MoveToSpot:
		MoveTo(action, glm::xz(Locator::fishFarmSystem::value().FishingSpot(farm)), VillagerStates::FishermanArrivesAtFishing);
		break;
	case work::FishingArrival::StartFishing:
		// It stands with its rod until its clip ends, then fishes
		Locator::livingActionSystem::value().VillagerPlayAnimThenSetState(action, VillagerStates::Fishing);
		break;
	}
	return 1;
}

uint32_t villager_fisherman::Fishing(LivingAction& action)
{
	const auto villager = EntityOf(action);
	if (!Locator::livingActionSystem::value().VillagerIsReadyForNewAnimation(action, 1))
	{
		return 1;
	}
	action.turnsSinceStateChange = 0;
	const auto farm = FarmOf(villager);
	if (farm == entt::null)
	{
		SetTopState(action, VillagerStates::DecideWhatToDo);
		return 1;
	}
	auto& person = VillagerOf(villager);
	const auto& info = InfoOf(person);
	const auto player = world_objects::PlayerOf(villager);
	const float tribalPower = player.has_value() && Locator::magicSystem::has_value()
	                              ? Locator::magicSystem::value().GetTribalPower(*player, Tribe::NORSE)
	                              : 1.0f;
	const auto caught = Locator::fishFarmSystem::value().Fish(
	    farm, info.maxFoodCarried, static_cast<uint32_t>(static_cast<int32_t>(person.foodHeld)), tribalPower);
	if (!caught.has_value())
	{
		return 1;
	}
	if (*caught != 0)
	{
		// The fish come out of the sea; the town counts what its people carry
		const auto picked = villager_carry::PickUp(person, ResourceType::Food, static_cast<int16_t>(*caught), 0);
		if (auto* tally = Entities().TryGet<TownResourceTally>(TownOf(person)))
		{
			tally->foodCarried += static_cast<float>(picked);
		}
	}
	const auto held = static_cast<uint32_t>(static_cast<int32_t>(person.foodHeld) - *caught);
	if (fish_farm::TakesCatchToStore(info.maxFoodCarried, held, static_cast<uint32_t>(*caught)))
	{
		return villager_build::GotoStoragePitForDropOff(action);
	}
	return 1;
}

bool villager_fisherman::EnterFishing(LivingAction& action, VillagerStates previous, VillagerStates /*next*/)
{
	const bool fromFishing = previous == VillagerStates::FishermanArrivesAtFishing || previous == VillagerStates::Fishing;
	const auto villager = EntityOf(action);
	const auto farm = FarmOf(villager);
	if (!fromFishing && TownOf(VillagerOf(villager)) != entt::null && farm != entt::null)
	{
		Locator::fishFarmSystem::value().AddFisherman(farm, villager);
	}
	return true;
}

bool villager_fisherman::ExitFishing(LivingAction& action, VillagerStates next)
{
	if (next == VillagerStates::FishermanArrivesAtFishing || next == VillagerStates::Fishing)
	{
		return false;
	}
	const auto villager = EntityOf(action);
	const auto farm = FarmOf(villager);
	if (farm != entt::null && Locator::fishFarmSystem::has_value())
	{
		Locator::fishFarmSystem::value().RemoveFisherman(farm, villager);
	}
	Entities().Remove<WorkTarget>(villager);
	// Leaving is never held up
	return false;
}
