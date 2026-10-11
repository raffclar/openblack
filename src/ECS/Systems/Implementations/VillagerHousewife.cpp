/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerHousewife.h"

#include <optional>
#include <vector>

#include <glm/gtx/vec_swizzle.hpp>

#include "Common/GameRandom.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/VillagerCarry.h"
#include "ECS/VillagerFoodRules.h"
#include "ECS/VillagerNeeds.h"
#include "ECS/VillagerRoutine.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "ObjectMeasures.h"
#include "VillagerBuild.h"
#include "VillagerFood.h"
#include "VillagerHome.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace housewife = openblack::ecs::villager_housewife;
namespace rules = openblack::ecs::villager_food_rules;
namespace villager_home = openblack::ecs::villager_home;
namespace villager_food = openblack::ecs::villager_food;
namespace villager_build = openblack::ecs::villager_build;
namespace routine = openblack::ecs::villager_routine;
namespace construction = openblack::ecs::construction;

namespace
{
auto& Entities()
{
	return Locator::entitiesRegistry::value();
}

entt::entity EntityOf(LivingAction& action)
{
	return Entities().ToEntity(action);
}

void SetTopState(LivingAction& action, VillagerStates state)
{
	Locator::livingActionSystem::value().VillagerSetState(action, LivingAction::Index::Top, state, false);
}

const GVillagerInfo& InfoOf(const Villager& villager)
{
	const auto& infos = Locator::infoConstants::value().villager;
	return infos.at(static_cast<size_t>(GVillagerInfo::Find(villager.tribe, villager.number)));
}

bool IsAtHome(entt::entity villager)
{
	return Entities().AllOf<AtHome>(villager);
}

entt::entity AbodeOf(entt::entity villager)
{
	auto& registry = Entities();
	const auto abode = registry.Get<const Villager>(villager).abode;
	return abode != entt::null && registry.Valid(abode) && registry.AllOf<Abode>(abode) ? abode : entt::null;
}

entt::entity TownOf(entt::entity villager)
{
	auto& registry = Entities();
	const auto town = registry.Get<const Villager>(villager).town;
	return town != entt::null && registry.Valid(town) && registry.AllOf<Town>(town) ? town : entt::null;
}

std::optional<entt::entity> StoragePitOf(entt::entity villager)
{
	return construction::StoragePitOf(TownOf(villager));
}

bool IsFunctional(std::optional<entt::entity> pit)
{
	return pit.has_value() && villager_home::IsFunctional(*pit);
}

uint32_t FoodIn(entt::entity store)
{
	return Locator::resourceStoreSystem::value().GetResource(store, ResourceType::Food);
}

/// The food the household wants for its dinner
uint32_t FoodNeededForDinner(entt::entity abode)
{
	auto& registry = Entities();
	std::vector<uint32_t> dinners;
	for (const auto person : registry.Get<const Abode>(abode).inhabitants)
	{
		if (const auto* resident = registry.TryGet<const Villager>(person))
		{
			dinners.push_back(InfoOf(*resident).foodReqiredForDinner);
		}
	}
	return rules::FoodForDinner(dinners);
}

bool IsEnoughFoodForDinner(entt::entity abode)
{
	return rules::IsEnoughForDinner(FoodNeededForDinner(abode), FoodIn(abode));
}

/// The food she needs for her own meal beyond what she carries
uint32_t RequiredForMeal(entt::entity villager)
{
	const auto& person = Entities().Get<const Villager>(villager);
	const auto town = TownOf(villager);
	std::optional<float> townDesire;
	if (town != entt::null && Locator::townDesireSystem::has_value())
	{
		townDesire = Locator::townDesireSystem::value().GetDesire(town, TownDesireInfo::ForFood);
	}
	const auto toEat =
	    rules::AmountToEat(ecs::villager_needs::DesireForFood(person.food), InfoOf(person).foodReqiredForDinner, townDesire);
	return rules::RequiredForMeal(toEat, person.foodHeld);
}

/// The town's pit has enough for her own meal
bool IsEnoughFoodInStoragePitForDinner(entt::entity villager)
{
	const auto pit = StoragePitOf(villager);
	return pit.has_value() && RequiredForMeal(villager) <= FoodIn(*pit);
}

/// Makes dinner when the home has enough for it, else fetches it first
uint32_t StartDinner(LivingAction& action)
{
	const auto abode = AbodeOf(EntityOf(action));
	SetTopState(action, abode != entt::null && IsEnoughFoodForDinner(abode) ? VillagerStates::HousewifeServesDinner
	                                                                        : VillagerStates::HousewifeGotoStoragePit);
	return 1;
}

/// The first of her household that is hungry: at home she makes dinner, else she goes home
uint32_t CheckNeededToMakeDinner(LivingAction& action)
{
	auto& registry = Entities();
	const auto abode = AbodeOf(EntityOf(action));
	if (abode == entt::null)
	{
		return 0;
	}
	for (const auto person : registry.Get<const Abode>(abode).inhabitants)
	{
		if (registry.AllOf<Villager>(person) && villager_food::IsHungry(person))
		{
			return IsAtHome(person) ? StartDinner(action) : villager_home::GoHome(action);
		}
	}
	return 0;
}

/// Short of dinner she fetches it; at home with enough she does the housework for a few turns
uint32_t CheckNeededForHouseWork(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto abode = AbodeOf(villager);
	if (abode == entt::null || !IsEnoughFoodForDinner(abode))
	{
		SetTopState(action, VillagerStates::HousewifeGotoStoragePit);
		return 1;
	}
	if (IsAtHome(villager))
	{
		action.turnsUntilStateChange = 5;
		SetTopState(action, VillagerStates::HousewifeDoesHousework);
		return 1;
	}
	return 0;
}
} // namespace

uint32_t housewife::AtHome(LivingAction& action)
{
	if (IsAtHome(EntityOf(action)) && (CheckNeededToMakeDinner(action) == 1 || CheckNeededForHouseWork(action) == 1))
	{
		return 1;
	}
	return 0;
}

uint32_t housewife::GotoStoragePit(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto pit = StoragePitOf(villager);
	const auto abode = AbodeOf(villager);
	if (IsFunctional(pit) && abode != entt::null && !IsEnoughFoodForDinner(abode))
	{
		// TODO(villagers): along the town's footpaths where there are any
		villager_home::SetupMoveTo(action, villager_home::ArrivePosition(*pit), VillagerStates::HousewifeArrivesAtStoragePit);
		return 1;
	}
	villager_home::GoHome(action);
	return 1;
}

uint32_t housewife::ArrivesAtStoragePit(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto pit = StoragePitOf(villager);
	if (!IsFunctional(pit))
	{
		SetTopState(action, VillagerStates::DecideWhatToDo);
		return 1;
	}
	const auto gossip = [&action] {
		SetTopState(action, VillagerStates::HousewifeGossipsAroundStoragePit);
		action.turnsUntilStateChange = 1;
		return GossipsAroundStoragePit(action);
	};
	if (!IsEnoughFoodInStoragePitForDinner(villager))
	{
		return gossip();
	}
	// She fetches the whole household's dinner, whatever the home already holds
	auto amount = RequiredForMeal(villager);
	if (const auto abode = AbodeOf(villager); abode != entt::null)
	{
		amount = static_cast<uint16_t>(FoodNeededForDinner(abode));
	}
	if (amount > FoodIn(*pit))
	{
		return gossip();
	}
	if (amount > 0)
	{
		return villager_build::ArrivesAtStoragePitForResource(
		    action, ResourceType::Food, amount, VillagerStates::HousewifePickupFromStoragePit, VillagerStates::DecideWhatToDo);
	}
	if (Entities().Get<const Villager>(villager).foodHeld != 0)
	{
		villager_home::SetupMoveTo(action, villager_home::ArrivePosition(*pit), VillagerStates::HousewifePickupFromStoragePit);
		return 1;
	}
	SetTopState(action, VillagerStates::DecideWhatToDo);
	return 1;
}

uint32_t housewife::PickupFromStoragePit(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto abode = AbodeOf(villager);
	if (Entities().Get<const Villager>(villager).foodHeld != 0 && abode != entt::null)
	{
		villager_home::SetupMoveTo(action, villager_home::ArrivePosition(abode), VillagerStates::HousewifeReturnHomeWithFood);
		return 1;
	}
	SetTopState(action, VillagerStates::DecideWhatToDo);
	return 1;
}

uint32_t housewife::ReturnHomeWithFood(LivingAction& action)
{
	auto& registry = Entities();
	const auto villager = EntityOf(action);
	// As in the game, the food goes into the home as soon as she sets off for its door, not only once she is in
	if (villager_home::ArrivesHome(action) == 1)
	{
		auto& person = registry.Get<Villager>(villager);
		const auto dropped = ecs::villager_carry::PutDown(person, ResourceType::Food, 0);
		if (auto* tally = TownOf(villager) != entt::null ? registry.TryGet<TownResourceTally>(TownOf(villager)) : nullptr)
		{
			tally->foodCarried -= static_cast<float>(dropped);
		}
		if (const auto abode = AbodeOf(villager); abode != entt::null)
		{
			Locator::resourceStoreSystem::value().AddResource(abode, ResourceType::Food, dropped, std::nullopt);
		}
	}
	return 1;
}

uint32_t housewife::MakeDinner(LivingAction& action)
{
	return StartDinner(action);
}

uint32_t housewife::ServesDinner(LivingAction& action)
{
	action.turnsUntilStateChange =
	    static_cast<uint16_t>(InfoOf(Entities().Get<const Villager>(EntityOf(action))).housewifeWaits);
	SetTopState(action, VillagerStates::HousewifeClearsAwayDinner);
	return 1;
}

uint32_t housewife::ClearsAwayDinner(LivingAction& action)
{
	--action.turnsUntilStateChange;
	if (action.turnsUntilStateChange == 0)
	{
		villager_food::EatFoodHeld(EntityOf(action));
		SetTopState(action, VillagerStates::GoHome);
	}
	return 1;
}

uint32_t housewife::DoesHousework(LivingAction& action)
{
	if (villager_home::CheckNeededForSpecial(action) != 1)
	{
		--action.turnsUntilStateChange;
		if (action.turnsUntilStateChange == 0)
		{
			villager_home::GoHome(action);
		}
	}
	return 1;
}

uint32_t housewife::GossipsAroundStoragePit(LivingAction& action)
{
	const auto turns = static_cast<int16_t>(action.turnsUntilStateChange) - 1;
	action.turnsUntilStateChange = static_cast<uint16_t>(turns);
	if (turns > 0)
	{
		return 1;
	}
	const auto villager = EntityOf(action);
	const auto pit = StoragePitOf(villager);
	if (!IsFunctional(pit))
	{
		// The game goes on to look at the pit it hasn't got; here she just goes home
		villager_home::GoHome(action);
		return 1;
	}
	if (FoodIn(*pit) > RequiredForMeal(villager))
	{
		SetTopState(action, VillagerStates::HousewifeGotoStoragePit);
		return GotoStoragePit(action);
	}
	if (villager_home::CheckNeededForSpecial(action) == 1)
	{
		return 1;
	}
	// She wanders to a spot just beyond the pit's edge to gossip a while
	auto& random = Locator::gameRandom::value();
	const float angle = random.GameFloatRand(routine::k_TwoPi);
	const float reach = random.GameFloatRand(rules::k_GossipReach);
	const auto centre = glm::xz(Entities().Get<const Transform>(*pit).position);
	const auto spot = routine::Polar(centre, angle, ecs::systems::object_measures::TwoDRadius(Entities(), *pit) + reach);
	if (!villager_home::Collides(spot))
	{
		villager_home::SetupMoveTo(action, spot, VillagerStates::HousewifeGossipsAroundStoragePit);
	}
	const auto& person = Entities().Get<const Villager>(villager);
	action.turnsUntilStateChange = rules::GossipTurns(InfoOf(person).housewifeGossipTime, person.food);
	return 1;
}
