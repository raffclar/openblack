/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerFarmer.h"

#include <glm/gtx/vec_swizzle.hpp>

#include "3D/MapCoords.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerWork.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/Systems/FieldSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/VillagerCarry.h"
#include "ECS/VillagerWorkRules.h"
#include "ECS/WallHugRules.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "VillagerBuild.h"
#include "VillagerFootpath.h"
#include "VillagerHome.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace villager_farmer = openblack::ecs::villager_farmer;
namespace work = openblack::ecs::villager_work;

namespace
{
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

glm::vec2 GroundOf(entt::entity entity)
{
	return glm::xz(Entities().Get<const Transform>(entity).position);
}

void SetTopState(LivingAction& action, VillagerStates state)
{
	Locator::livingActionSystem::value().VillagerSetState(action, LivingAction::Index::Top, state, false);
}

void MoveTo(LivingAction& action, glm::vec2 goal, VillagerStates final)
{
	villager_home::SetupMoveTo(action, goal, final);
}

/// Walks to the field along its footpaths where they lead its way
void MoveOnPathsTo(LivingAction& action, entt::entity field, glm::vec2 goal, VillagerStates final)
{
	villager_footpath::SetupMoveToOnFootpath(action, field, goal, final);
}

/// The field the villager farms, if it still stands
entt::entity FieldOf(entt::entity villager)
{
	const auto* target = Entities().TryGet<const WorkTarget>(villager);
	return target != nullptr && Entities().Valid(target->thing) && Entities().AllOf<Field>(target->thing) ? target->thing
	                                                                                                      : entt::null;
}

/// A fresh random spot in the field to work at next
glm::vec2 RandomSpot(entt::entity field)
{
	auto& random = Locator::gameRandom::value();
	const float drawX = random.GameFloatRand(2.0f * work::k_FieldSpotReach);
	const float drawZ = random.GameFloatRand(2.0f * work::k_FieldSpotReach);
	return work::FieldWorkSpot(GroundOf(field), drawX, drawZ);
}

void PickNewSpot(entt::entity villager, entt::entity field)
{
	Entities().AssignOrReplace<FieldWorkSpot>(villager, FieldWorkSpot {.position = RandomSpot(field)});
}

glm::vec2 SpotOf(entt::entity villager)
{
	const auto* spot = Entities().TryGet<const FieldWorkSpot>(villager);
	return spot != nullptr ? spot->position : GroundOf(villager);
}

/// Whether the villager stands at its spot: nearer than it walks in a step
bool AtSpot(entt::entity villager)
{
	const auto* wallHug = Entities().TryGet<const WallHug>(villager);
	const auto speed = static_cast<float>(wall_hug::WholeSpeed(wallHug != nullptr ? wallHug->speed : 0.0f));
	return work::AreWeThere(map_coords::FromMetres(GroundOf(villager)), map_coords::FromMetres(SpotOf(villager)), speed, 0.0f);
}

bool IsFarmingState(VillagerStates state)
{
	return state == VillagerStates::FarmerArrivesAtFarm || state == VillagerStates::FarmerPlantsCrop ||
	       state == VillagerStates::FarmerDigsUpCrop;
}
} // namespace

std::optional<villager_farmer::FieldChoice> villager_farmer::FindBestField(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto town = TownOf(VillagerOf(villager));
	if (town == entt::null || !Locator::fieldSystem::has_value())
	{
		return std::nullopt;
	}
	const auto choice = Locator::fieldSystem::value().BestFieldFor(town, Entities().Get<const Transform>(villager).position);
	if (!choice.has_value())
	{
		return std::nullopt;
	}
	return FieldChoice {.field = choice->field, .score = choice->score};
}

uint32_t villager_farmer::VillagerBecomesFarmer(LivingAction& action, std::optional<entt::entity> field)
{
	if (!field.has_value())
	{
		const auto best = FindBestField(action);
		if (!best.has_value())
		{
			return 0;
		}
		field = best->field;
	}
	return SetFarmerGotoField(action, *field);
}

uint32_t villager_farmer::SetFarmerGotoField(LivingAction& action, entt::entity field)
{
	const auto activity = Locator::fieldSystem::value().GetActivity(field);
	if (activity == field_crop::Activity::Growing)
	{
		return 0;
	}
	const auto villager = EntityOf(action);
	SetTopState(action, VillagerStates::DecideWhatToDo);
	Entities().AssignOrReplace<WorkTarget>(villager, WorkTarget {.thing = field});
	MoveOnPathsTo(action, field, GroundOf(field), VillagerStates::FarmerArrivesAtFarm);
	// Its first spot is picked once it has set off
	PickNewSpot(villager, field);
	return 1;
}

uint32_t villager_farmer::FarmerArrivesAtFarm(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto field = FieldOf(villager);
	if (field == entt::null)
	{
		SetTopState(action, VillagerStates::DecideWhatToDo);
		return 1;
	}
	const auto& fields = Locator::fieldSystem::value();
	const auto activity = fields.GetActivity(field);
	const bool sowing = activity == field_crop::Activity::Sowing;
	if (sowing)
	{
		// It sows empty-handed
		VillagerOf(villager).carried = CarriedObject::None;
	}
	const bool grown = activity == field_crop::Activity::Grown;
	const bool ripe = grown && fields.IsRipe(field);
	switch (work::ArriveAtField(sowing, grown, ripe, (sowing || ripe) && AtSpot(villager)))
	{
	case work::FarmerArrival::Sow:
		PickNewSpot(villager, field);
		// The sowing clip plays to its end, then it sows
		Locator::livingActionSystem::value().VillagerPlayAnimThenSetState(action, VillagerStates::FarmerPlantsCrop);
		break;
	case work::FarmerArrival::Dig:
		PickNewSpot(villager, field);
		Locator::livingActionSystem::value().VillagerPlayAnimThenSetState(action, VillagerStates::FarmerDigsUpCrop);
		break;
	case work::FarmerArrival::WalkToSpot:
		MoveTo(action, SpotOf(villager), VillagerStates::FarmerArrivesAtFarm);
		break;
	case work::FarmerArrival::Decide:
		SetTopState(action, VillagerStates::DecideWhatToDo);
		break;
	}
	return 1;
}

uint32_t villager_farmer::FarmerPlantsCrop(LivingAction& action)
{
	const auto field = FieldOf(EntityOf(action));
	auto* fields = Locator::fieldSystem::has_value() ? &Locator::fieldSystem::value() : nullptr;
	// Once it has sown, it walks to its next spot while the field still wants sowing
	if (field != entt::null && fields != nullptr && fields->PlantCrop(field) &&
	    fields->GetActivity(field) == field_crop::Activity::Sowing)
	{
		SetTopState(action, VillagerStates::FarmerArrivesAtFarm);
		return 1;
	}
	SetTopState(action, VillagerStates::DecideWhatToDo);
	return 1;
}

uint32_t villager_farmer::FarmerDigsUpCrop(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto field = FieldOf(villager);
	if (field == entt::null)
	{
		SetTopState(action, VillagerStates::DecideWhatToDo);
		return 1;
	}
	auto& person = VillagerOf(villager);
	const auto& info = InfoOf(person);
	// The room left is kept in 16 bits
	const auto room = static_cast<int16_t>(info.maxFoodCarried - static_cast<uint32_t>(person.foodHeld));
	const auto given = Locator::fieldSystem::value().RemoveFood(field, static_cast<float>(room));
	if (given != 0)
	{
		const auto picked = villager_carry::PickUp(person, ResourceType::Food, static_cast<int16_t>(given), 0);
		if (auto* tally = Entities().TryGet<TownResourceTally>(TownOf(person)))
		{
			tally->foodCarried += static_cast<float>(picked);
		}
	}
	const auto roomAfter = static_cast<int16_t>(info.maxFoodCarried - static_cast<uint32_t>(person.foodHeld));
	const float cropFood =
	    Locator::infoConstants::value().mobileObject.at(static_cast<size_t>(MobileObjectInfo::Crop)).foodValue;
	if (work::TakesHarvestToStore(given, roomAfter, cropFood))
	{
		return villager_build::GotoStoragePitForDropOff(action);
	}
	SetTopState(action, VillagerStates::FarmerArrivesAtFarm);
	return 1;
}

bool villager_farmer::EnterFarming(LivingAction& action, VillagerStates previous, VillagerStates /*next*/)
{
	const auto villager = EntityOf(action);
	const auto field = FieldOf(villager);
	// It can't farm without a field
	if (field == entt::null)
	{
		return false;
	}
	if (!IsFarmingState(previous))
	{
		Locator::fieldSystem::value().AddFarmer(field, villager);
	}
	return true;
}

bool villager_farmer::ExitFarming(LivingAction& action, VillagerStates next)
{
	if (IsFarmingState(next))
	{
		return false;
	}
	const auto villager = EntityOf(action);
	if (const auto field = FieldOf(villager); field != entt::null && Locator::fieldSystem::has_value())
	{
		Locator::fieldSystem::value().RemoveFarmer(field, villager);
	}
	Entities().Remove<WorkTarget>(villager);
	Entities().Remove<FieldWorkSpot>(villager);
	// Leaving is never held up
	return false;
}
