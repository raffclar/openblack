/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerFood.h"

#include <fmt/format.h>
#include <glm/vec2.hpp>

#include "3D/MapCoords.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Life.h"
#include "ECS/ObjectResources.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerResources.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/Villager/VillagerStateInfo.h"
#include "ECS/Villager/VillagerTrace.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"

// The villager's food (VillagerFood.h)

namespace openblack::ecs::villager
{
using namespace components;
namespace tq = town_queries;
using state_info::StateInfo;

namespace
{
/// GetAmountOfFoodToEat's share of the town's Food desire
constexpr float k_TownFoodShare = 0.3f;

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Villager* VillagerOf(entt::entity villager)
{
	return Entities().TryGet<Villager>(villager);
}

entt::entity TownEntityOf(const Villager& v)
{
	// (guard) the original only reads the stored town: this stands for the missing town unlinking
	return v.town != entt::null && ecs::IsAvailable(v.town) && Entities().AllOf<Town>(v.town) ? v.town : entt::null;
}

entt::entity AbodeEntityOf(const Villager& v)
{
	// (guard) the original only reads the stored abode: this stands for the unlinking when the abode is deleted
	return v.abode != entt::null && ecs::IsAvailable(v.abode) ? v.abode : entt::null;
}

bool Inside(const Villager& v)
{
	return (v.flags & Villager::k_FlagAtHome) != 0;
}
} // namespace

// ---- the pure layer ----------------------------------------------------------------------------------------------

float HungerBatch(float food, uint32_t turns, float reducesFoodBy, std::optional<float> tribalPower, float speed, bool moving)
{
	// Turns (unsigned 64-bit) x reducesFoodBy, stored
	float drop = static_cast<float>(static_cast<double>(turns)) * reducesFoodBy;
	// With a player, drop / tribalPower
	if (tribalPower.has_value())
	{
		drop = drop / *tribalPower;
	}
	// speed > 1.0 (double) and IsMoving -> drop = speed x drop
	if (static_cast<double>(speed) > 1.0 && moving)
	{
		drop = speed * drop;
	}
	// food - drop, below 0 -> 0
	const float left = food - drop;
	return left < 0.0f ? 0.0f : left;
}

uint32_t FoodToEat(float food, uint32_t dinner, std::optional<float> townFoodDesire)
{
	// POWER(food) x foodReqiredForDinner (an integer multiply, the u32 read as a 64-bit value's low half), stored
	const float t = Power(food) * static_cast<float>(static_cast<double>(dinner));
	if (!townFoodDesire.has_value())
	{
		return static_cast<uint32_t>(map_coords::FtoL(t));
	}
	// Below 0 -> 0, above 1 -> 1
	float c = *townFoodDesire;
	if (c < 0.0f)
	{
		c = 0.0f;
	}
	else if (c > 1.0f)
	{
		c = 1.0f;
	}
	// Truncate((1 - c x 0.3) x t)
	const float share = c * k_TownFoodShare;
	const float keep = 1.0f - share;
	return static_cast<uint32_t>(map_coords::FtoL(keep * t));
}

uint32_t FoodRequiredForMeal(uint32_t eat, int16_t held)
{
	// eat - (signed) held; <= 0 -> 0
	const int32_t need = static_cast<int32_t>(eat) - static_cast<int32_t>(held);
	return need <= 0 ? 0u : static_cast<uint32_t>(need);
}

EatResult EatHeld(float food, int16_t held, uint32_t eat, float nourish)
{
	// eat (unsigned) and held (signed) as floats; the smaller
	const float eatF = static_cast<float>(static_cast<double>(eat));
	const float heldF = static_cast<float>(held);
	const float eaten = eatF < heldF ? eatF : heldF;
	// eaten / eat x foodNurishmentMultiplier + food; below 0 or NaN -> 0; above 1 -> 1
	const float share = eaten / eatF;
	const float gain = share * nourish;
	float result = gain + food;
	if (!(result >= 0.0f))
	{
		result = 0.0f;
	}
	else if (result > 1.0f)
	{
		result = 1.0f;
	}
	return {eaten, result};
}

// ---- the hunger --------------------------------------------------------------------------------------------------

bool CheckHungry(entt::entity villager, uint32_t turn)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return false;
	}
	// No turn since the last check -> 0, without SetGameTurnLastChecked
	const uint32_t turns = GetGameTurnsSinceLastChecked(villager, turn);
	if (turns == 0)
	{
		return false;
	}
	const auto& info = InfoOf(villager);
	// speed = (float)(u16 speed) / (int) speedDefault. (approximate) the u16 is
	// WallHug::speed back to MapCoords (map_coords::ToFixed); (openblack, guard) a speed group of 0 gives 0
	const auto* wallHug = Entities().TryGet<const WallHug>(villager);
	const auto raw = wallHug != nullptr ? static_cast<uint16_t>(map_coords::ToFixed(wallHug->speed)) : uint16_t {0};
	const auto group = static_cast<int32_t>(static_cast<uint32_t>(info.speedGroup.speedDefault));
	const float speed = group != 0 ? static_cast<float>(raw) / static_cast<float>(group) : 0.0f;
	// The player (the town's owner) -> TribalPower[3]
	// (PlayerMagic::tribalPower, 1.0 unless written)
	std::optional<float> tribal;
	if (const auto town = TownEntityOf(*v); town != entt::null)
	{
		tribal = magic::players::MagicOf(Entities().Get<const Town>(town).owner).tribalPower.at(3);
	}
	// IsMoving (it moved since the last turn). (approximate) the WallHug's
	// last step was not zero
	const bool moving = wallHug != nullptr && wallHug->step != glm::vec2(0.0f);
	const float before = v->food;
	v->food = HungerBatch(v->food, turns, info.gameTurnReducesFoodInBellyBy, tribal, speed, moving);
	// hungry = food < hungryForFood (strict; IsHungry is <=)
	const bool hungry = v->food < info.hungryForFood;
	uint32_t result = 0;
	// Hungry or poisoned -> ReduceLife(max(1 - food / hungry, 1) x hungerToLifeMultiplier, no player)
	if (hungry || life::IsPoisoned(villager))
	{
		life::ReduceLife(villager, life::HungerLifeLoss(v->food, info.hungryForFood, info.hungerToLifeMultiplier));
	}
	if (hungry)
	{
		// GetFinalState's row, read once for both flags
		const auto final = GetFinalState(villager);
		const auto& row = StateInfo(final);
		auto* again = VillagerOf(villager);
		const bool ignores =
		    again != nullptr && (again->flags & Villager::k_FlagDisciple) != 0 && DiscipleIgnoresNeeds(again->discipleType);
		// InterruptWhenHungry and not a disciple that ignores needs
		if (state_info::InterruptWhenHungry(row) && !ignores)
		{
			result = ChangeStateToFindFoodToEat(villager);
		}
		// food < starvingForFood and InterruptWhenStarving and still 0 (no
		// disciple test)
		again = VillagerOf(villager);
		if (again != nullptr && again->food < info.starvingForFood && state_info::InterruptWhenStarving(row) && result == 0)
		{
			result = ChangeStateToFindFoodToEat(villager);
		}
		// life <= 0 -> VillagerDead(final (read again) 248 / 249 / 250 or flags & 2
		// ? 4 CHANT : 1 STARVING, GetPlayer, GetLife, 1); 1
		if (life::LifeOf(villager) <= 0.0f)
		{
			const auto now = GetFinalState(villager);
			const auto* last = VillagerOf(villager);
			const bool chant = now == VillagerStates::GoHomeFromWorship || now == VillagerStates::ArrivesHomeFromWorship ||
			                   now == VillagerStates::SleepInTentFromWorship ||
			                   (last != nullptr && (last->flags & Villager::k_FlagAtWorshipSite) != 0);
			// GetPlayer (the town's owner, none without a town)
			VillagerDead(villager, chant ? DeathReason::Chant : DeathReason::Starving, GetPlayerOf(villager),
			             life::LifeOf(villager), 1);
			result = 1;
		}
	}
	if (TraceOn(villager))
	{
		Trace(villager, fmt::format("food: drop {:.6f} food {:.6f} life {:.6f}{} -> {}", before - v->food, v->food,
		                            life::LifeOf(villager), hungry ? " hungry" : "", result));
	}
	// SetGameTurnLastChecked
	SetGameTurnLastChecked(villager, turn);
	return result != 0;
}

uint32_t GetAmountOfFoodToEat(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0;
	}
	// With a town, its Food desire without the boosts
	std::optional<float> desire;
	if (const auto town = TownEntityOf(*v); town != entt::null)
	{
		desire = town_desire::GetField(town, TownDesireInfo::ForFood, town_desire::Field::Desire);
	}
	return FoodToEat(v->food, InfoOf(villager).foodReqiredForDinner, desire);
}

uint32_t GetAmountOfFoodRequiredForMeal(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	return v != nullptr ? FoodRequiredForMeal(GetAmountOfFoodToEat(villager), v->resourceHeld.at(0)) : 0;
}

uint32_t CheckSatisfyOwnFoodDesire(entt::entity villager)
{
	// IsHungry (food <= hungryForFood) -> ChangeStateToFindFoodToEat
	return IsHungry(villager) ? ChangeStateToFindFoodToEat(villager) : 0;
}

uint32_t ChangeStateToFindFoodToEat(entt::entity villager)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0;
	}
	// need = GetAmountOfFoodRequiredForMeal; eat = inside ? 118 : 117
	const uint32_t need = GetAmountOfFoodRequiredForMeal(villager);
	const auto eat = Inside(*v) ? VillagerStates::EatFoodAtHome : VillagerStates::EatFood;
	const int16_t held = v->resourceHeld.at(0);
	// need 0 -> SetTopState(eat); 1
	if (need == 0)
	{
		villager::TraceFormatted(villager, "food: need 0 held {} -> {}", held, static_cast<uint32_t>(eat));
		SetTopState(villager, eat);
		return 1;
	}
	// A functional abode whose GetResource(FOOD) + (signed) held >= need (unsigned) -> 36, or
	// 118 inside
	if (const auto abode = AbodeEntityOf(*v); abode != entt::null && abode_queries::IsFunctional(abode))
	{
		const uint32_t have =
		    object_resources::GetResource(abode, ResourceType::Food) + static_cast<uint32_t>(static_cast<int32_t>(held));
		if (!(have < need))
		{
			const auto next = Inside(*v) ? VillagerStates::EatFoodAtHome : VillagerStates::GoHome;
			villager::TraceFormatted(villager, "food: need {} home {} held {} -> {}", need, have - static_cast<uint32_t>(held),
			                         held, static_cast<uint32_t>(next));
			SetTopState(villager, next);
			return 1;
		}
	}
	// GetStoragePit (the town's, else its abode) functional: enough -> 33; else on to what it carries
	const auto pit = GetStoragePit(villager);
	if (pit != entt::null && abode_queries::IsFunctional(pit))
	{
		const uint32_t inPit = object_resources::GetResource(pit, ResourceType::Food);
		if (!(inPit < need))
		{
			villager::TraceFormatted(villager, "food: need {} pit {} has {} -> 33", need, static_cast<uint32_t>(pit), inPit);
			SetTopState(villager, VillagerStates::GotoStoragePitForFood);
			return 1;
		}
	}
	else
	{
		// pos = GetResourceDropoffPos(FOOD); not IsCloseToEqual(pos, me, 0) -> walk, FINAL 34; 1
		const auto pos = GetResourceDropoffPos(villager, ResourceType::Food);
		if (!(tq::GetDistanceInMetres(pos, tq::PosOf(villager)) <= 0.0f))
		{
			villager::TraceFormatted(villager, "food: need {} no pit -> 34 at the drop-off point", need);
			SetupMoveToWithHug(villager, tq::ToMetres(pos), VillagerStates::ArrivesAtStoragePitForFood);
			return 1;
		}
	}
	// It carries some -> SetTopState(eat); 1; else 0
	if (held != 0)
	{
		villager::TraceFormatted(villager, "food: need {} -> eat-held {} ({})", need, held, static_cast<uint32_t>(eat));
		SetTopState(villager, eat);
		return 1;
	}
	villager::TraceFormatted(villager, "food: need {} -> none", need);
	return 0;
}

float EatFoodHeld(entt::entity villager)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0.0f;
	}
	const auto& info = InfoOf(villager);
	const uint32_t eat = GetAmountOfFoodToEat(villager);
	const auto before = v->food;
	const auto r = EatHeld(v->food, v->resourceHeld.at(0), eat, info.foodNurishmentMultiplier);
	// DropFood(truncate(eaten)) (DropFood(0) drops all of it)
	const auto eatenInt = static_cast<uint16_t>(map_coords::FtoL(r.eaten));
	DropFood(villager, eatenInt);
	// The belly
	if (auto* again = VillagerOf(villager))
	{
		again->food = r.food;
	}
	// With a town, the town's UseFood(truncate(eaten))
	if (const auto town = TownEntityOf(*v); town != entt::null)
	{
		town_villagers::UseFood(town, static_cast<uint32_t>(map_coords::FtoL(r.eaten)));
	}
	villager::TraceFormatted(villager, "eat: held {} eat {} food {:.6f} -> {:.6f}", eatenInt, eat, before, r.food);
	return r.food;
}

void GetFoodFromHome(entt::entity villager, uint32_t amount)
{
	const auto* v = VillagerOf(villager);
	const auto abode = v != nullptr ? AbodeEntityOf(*v) : entt::null;
	if (abode == entt::null)
	{
		return;
	}
	// m = min(n, GetResource(FOOD))
	const uint32_t have = object_resources::GetResource(abode, ResourceType::Food);
	const uint32_t m = amount < have ? amount : have;
	// GetResourceFrom(abode, FOOD, m): RemoveResource + PickupResource
	const auto took = GetResourceFrom(villager, abode, ResourceType::Food, static_cast<int16_t>(m));
	// PickupFood(what it took) again (literal: the villager gains twice what the abode lost)
	PickupFood(villager, static_cast<int16_t>(took));
	if (TraceOn(villager))
	{
		const auto* after = VillagerOf(villager);
		Trace(villager, fmt::format("home-food: took {} held {}", took, after != nullptr ? after->resourceHeld.at(0) : 0));
	}
}

// ---- the state functions -----------------------------------------------------------------------------------------

uint32_t EatFood(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// EatFoodHeld; PlayAnimThenSetState(poisoned ? 212 : 163, 1)
	EatFoodHeld(villager);
	PlayAnimThenSetState(villager, life::IsPoisoned(villager) ? VillagerStates::ShowPoisoned : VillagerStates::DecideWhatToDo);
	return 1;
}

uint32_t EatFoodAtHome(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 1;
	}
	// The held food read first; n = GetAmountOfFoodToEat - held; > 0 -> GetFoodFromHome(n)
	const int16_t held = v->resourceHeld.at(0);
	const int32_t n = static_cast<int32_t>(GetAmountOfFoodToEat(villager)) - static_cast<int32_t>(held);
	if (n > 0)
	{
		GetFoodFromHome(villager, static_cast<uint32_t>(n));
	}
	// EatFoodHeld; SetTopState(poisoned ? 212 : 38)
	EatFoodHeld(villager);
	SetTopState(villager, life::IsPoisoned(villager) ? VillagerStates::ShowPoisoned : VillagerStates::AtHome);
	return 1;
}

uint32_t ShowPoisoned(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto* v = VillagerOf(villager);
	// Inside -> SetupMoveToWithHug(FindPosOutsideAbode(0), 212)
	if (v != nullptr && Inside(*v))
	{
		const auto pos = FindPosOutsideAbode(villager, entt::null);
		SetupMoveToWithHug(villager, tq::ToMetres(pos), VillagerStates::ShowPoisoned);
	}
	// PlayAnimThenSetState(163, 1)
	PlayAnimThenSetState(villager, VillagerStates::DecideWhatToDo);
	return 1;
}

uint32_t GotoStoragePitForFood(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// GetStoragePit functional -> SetupMoveToOnFootpath(pit, pit.GetArrivePos, 34)
	if (const auto pit = GetStoragePit(villager); pit != entt::null && abode_queries::IsFunctional(pit))
	{
		villager::TraceFormatted(villager, "food 33: to the pit {}", static_cast<uint32_t>(pit));
		SetupMoveToOnFootpath(villager, pit, abode_queries::GetArrivePos(pit), VillagerStates::ArrivesAtStoragePitForFood);
		return 1;
	}
	// Else SetupMoveToWithHug(GetResourceDropoffPos(FOOD), 34)
	const auto pos = GetResourceDropoffPos(villager, ResourceType::Food);
	SetupMoveToWithHug(villager, tq::ToMetres(pos), VillagerStates::ArrivesAtStoragePitForFood);
	return 1;
}

uint32_t ArrivesAtStoragePitForFood(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// ArrivesAtStoragePitForResource(FOOD, GetAmountOfFoodRequiredForMeal(), 163, 163)
	return ArrivesAtStoragePitForResource(villager, ResourceType::Food, GetAmountOfFoodRequiredForMeal(villager),
	                                      VillagerStates::DecideWhatToDo, VillagerStates::DecideWhatToDo);
}

uint32_t ArrivesAtHomeWithFood(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto* v = VillagerOf(villager);
	// With an abode, abode.AddResource(FOOD, (signed) DropFood(0), 0, 0, 0, 0)
	if (v != nullptr)
	{
		if (const auto abode = AbodeEntityOf(*v); abode != entt::null)
		{
			const auto dropped = DropFood(villager, 0);
			object_resources::AddResource(abode, ResourceType::Food,
			                              static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(dropped))));
		}
	}
	// ArrivesHome (its result)
	return ArrivesHome(villager);
}
} // namespace openblack::ecs::villager
