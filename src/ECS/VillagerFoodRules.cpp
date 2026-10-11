/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerFoodRules.h"

#include <algorithm>
#include <numeric>

using namespace openblack;
using namespace openblack::ecs;

std::optional<villager_food_rules::Digestion> villager_food_rules::Digest(const DigestInputs& in)
{
	if (in.turnsSinceChecked == 0)
	{
		return std::nullopt;
	}
	float burnt = static_cast<float>(in.turnsSinceChecked) * in.foodPerTurn;
	const float pace = in.speed / in.walkingSpeed;
	if (in.tribalPower.has_value())
	{
		burnt /= *in.tribalPower;
	}
	if (pace > 1.0f && in.moving)
	{
		burnt = pace * burnt;
	}
	Digestion result;
	result.food = std::max(in.food - burnt, 0.0f);
	result.hungry = result.food < in.hungryForFood;
	if (result.hungry || in.poisoned)
	{
		// The emptier it is the more it would lose, but never less than the whole rate: with food never below none, it
		// always loses the rate itself
		float share = 1.0f - result.food / in.hungryForFood;
		if (share <= 1.0f)
		{
			share = 1.0f;
		}
		result.lifeLost = share * in.hungerToLife;
	}
	return result;
}

bool villager_food_rules::SeeksFood(bool stateGivesWayToHunger, bool stateGivesWayToStarving, bool workingDisciple, float food,
                                    float starvingForFood, const std::function<bool()>& tryToEat)
{
	bool went = false;
	if (stateGivesWayToHunger && !workingDisciple)
	{
		went = tryToEat();
	}
	if (food < starvingForFood && stateGivesWayToStarving && !went)
	{
		went = tryToEat();
	}
	return went;
}

uint32_t villager_food_rules::AmountToEat(float desireForFood, uint32_t foodForDinner, std::optional<float> townFoodDesire)
{
	const float wanted = desireForFood * static_cast<float>(foodForDinner);
	if (townFoodDesire.has_value())
	{
		const float shortage = std::clamp(*townFoodDesire, 0.0f, 1.0f);
		return static_cast<uint32_t>((1.0f - shortage * k_TownShortageShare) * wanted);
	}
	return static_cast<uint32_t>(wanted);
}

uint32_t villager_food_rules::RequiredForMeal(uint32_t amountToEat, int16_t carried)
{
	const int32_t required = static_cast<int32_t>(amountToEat) - carried;
	return required > 0 ? static_cast<uint32_t>(required) : 0u;
}

villager_food_rules::MealPlan villager_food_rules::PlanMeal(const MealInputs& in)
{
	const auto eatHere = in.atHome ? MealPlan::EatAtHome : MealPlan::EatHeld;
	if (in.required == 0)
	{
		return eatHere;
	}
	// What it carries is already taken off what it needs, so only its home's food is counted (the game counted what it
	// carries again: deviations register)
	if (in.homeFood.has_value() && in.required <= *in.homeFood)
	{
		return in.atHome ? MealPlan::EatAtHome : MealPlan::GoHome;
	}
	if (in.pitFood.has_value())
	{
		if (in.required <= *in.pitFood)
		{
			return MealPlan::GoToStoragePit;
		}
	}
	else if (in.dropOffElsewhere)
	{
		return MealPlan::GoToDropOff;
	}
	return in.carried == 0 ? MealPlan::None : eatHere;
}

villager_food_rules::Meal villager_food_rules::EatHeld(uint32_t amountToEat, int16_t carried, float food, float nourishment)
{
	// A meal of nothing leaves it as it is (the game emptied its belly: deviations register)
	if (amountToEat == 0)
	{
		return {.eaten = 0, .food = food};
	}
	const auto meal = static_cast<float>(amountToEat);
	const auto held = static_cast<float>(carried);
	const float eaten = meal < held ? meal : held;
	float full = food + eaten / meal * nourishment;
	if (!(full >= 0.0f))
	{
		full = 0.0f;
	}
	else if (full > 1.0f)
	{
		full = 1.0f;
	}
	return {.eaten = static_cast<uint16_t>(static_cast<int32_t>(eaten)), .food = full};
}

uint32_t villager_food_rules::FoodForDinner(std::span<const uint32_t> dinners)
{
	return std::accumulate(dinners.begin(), dinners.end(), 0u);
}

uint16_t villager_food_rules::GossipTurns(uint32_t gossipTime, float food)
{
	return static_cast<uint16_t>(static_cast<int32_t>(static_cast<float>(gossipTime) + food * k_GossipTurnsPerFood));
}

bool villager_food_rules::IsInterestedInFood(const FoodInterestInputs& in)
{
	const float room = static_cast<float>(static_cast<int16_t>(in.maxCarried - in.carried)) / static_cast<float>(in.maxCarried);
	const float speedUp = in.speedUp ? k_SpeedUpFoodInterest : 1.0f;
	if (speedUp * room * in.desireForFood * in.stateInterest * in.distanceModifier + in.interactDesire >
	    k_FoodInterestThreshold)
	{
		return true;
	}
	return in.townFoodDesire.has_value() &&
	       in.interactDesire + *in.townFoodDesire * room * in.stateInterest * in.distanceModifier > in.townTrigger;
}

bool villager_food_rules::CarriesFoodToStore(float townShortage, int16_t carried, uint32_t maxCarried, float storeNearness)
{
	const float load = static_cast<float>(carried) / static_cast<float>(maxCarried);
	return townShortage * load * storeNearness > k_CarryFoodToStoreThreshold;
}
