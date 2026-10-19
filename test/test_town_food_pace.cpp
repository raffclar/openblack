/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <random>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Components/TownDesire.h"
#include "ECS/TownDesire.h"
#include "ECS/VillagerFoodRules.h"
#include "ECS/VillagerNeeds.h"
#include "InfoConstants.h"

using namespace openblack;
namespace rules = openblack::ecs::villager_food_rules;
namespace needs = openblack::ecs::villager_needs;
namespace td = openblack::ecs::town_desire;

namespace
{

/// The made-up numbers of a villager, near the info table's: food burnt a turn, the hungry mark, the hunger cost
constexpr float k_FoodPerTurn = 9e-5f;
constexpr float k_Hungry = 0.5f;
constexpr float k_HungerToLife = 0.001f;
/// A villager's needs are checked once more than this many turns go by
constexpr uint32_t k_ChecksEvery = 8;
/// A town wants this many times the dinners of its people
constexpr float k_FoodWanted = 5.0f;
/// The food desire past which a free adult takes a food job: the desire's own trigger and the least trigger
constexpr float k_FoodJobDesire = 0.01f + 0.001f;

struct ModelVillager
{
	float food {1.0f};
	uint32_t dinner {0};
	float nourishment {1.0f};
	uint32_t lastCheck {0};
};

/// A town's people that eat from its store only, by the game's rules for a check, a meal and the town's food desire.
/// It leaves out the walk to the store and back, the food jobs that fill the store, and states that hold a hungry
/// villager back: it is the store's fastest draw for the people it has.
class TownFoodModel
{
public:
	TownFoodModel(uint32_t store, std::vector<ModelVillager> people)
	    : _store(store)
	    , _people(std::move(people))
	{
		for (const auto& person : _people)
		{
			_dinners += static_cast<float>(person.dinner);
		}
	}

	[[nodiscard]] float FoodDesire() const
	{
		td::DesireInputs in {};
		in.storageFood = _store;
		in.stats.foodForDinner = _dinners;
		GTownInfo town {};
		town.foodWantedMultiplier = k_FoodWanted;
		const ecs::components::TownDesire wants {};
		const std::array<GTownDesireInfo, td::k_Count> info {};
		return td::DesireForFood({.desire = wants, .in = in, .town = town, .info = info});
	}

	void Turn(uint32_t turn)
	{
		const float shortage = FoodDesire();
		for (auto& person : _people)
		{
			if (turn - person.lastCheck <= k_ChecksEvery)
			{
				continue;
			}
			const auto digestion = rules::Digest({.turnsSinceChecked = turn - person.lastCheck,
			                                      .food = person.food,
			                                      .foodPerTurn = k_FoodPerTurn,
			                                      .hungryForFood = k_Hungry,
			                                      .hungerToLife = k_HungerToLife,
			                                      .speed = 1.0f,
			                                      .walkingSpeed = 1.0f,
			                                      .tribalPower = 1.0f});
			person.lastCheck = turn;
			person.food = digestion->food;
			if (!digestion->hungry)
			{
				continue;
			}
			const auto meal = rules::AmountToEat(needs::DesireForFood(person.food), person.dinner, shortage);
			const auto required = rules::RequiredForMeal(meal, 0);
			if (required == 0 || required > _store)
			{
				continue;
			}
			_store -= required;
			const auto eaten = rules::EatHeld(meal, static_cast<int16_t>(required), person.food, person.nourishment);
			person.food = eaten.food;
			_used += eaten.eaten;
		}
	}

	[[nodiscard]] uint32_t Store() const { return _store; }
	[[nodiscard]] uint32_t Used() const { return _used; }

private:
	uint32_t _store;
	uint32_t _used {0};
	float _dinners {0.0f};
	std::vector<ModelVillager> _people;
};

/// The first land's home town as it starts: 22 men and 22 women of a tribe whose men eat 95 for dinner, nourished 1.44
/// times, and women 90, nourished 1.2 times. Each starts as full as the game makes a new villager, with its checks at a
/// phase of their own in the first eight turns. The draws come from a fixed seed.
std::vector<ModelVillager> FirstLandsHomeTown()
{
	std::mt19937 draws(20261019u);
	const auto upTo = [&draws](float most) { return static_cast<float>(draws() >> 16u) * most / 65535.0f; };
	std::vector<ModelVillager> people;
	for (int i = 0; i < 44; ++i)
	{
		const bool man = i % 2 == 0;
		float food = upTo(0.6f) + k_Hungry;
		food = food < 1.0f ? upTo(0.6f) + k_Hungry : 1.0f;
		people.push_back({.food = food,
		                  .dinner = man ? 95u : 90u,
		                  .nourishment = man ? 1.44f : 1.2f,
		                  .lastCheck = k_ChecksEvery - draws() % k_ChecksEvery});
	}
	return people;
}

} // namespace

TEST(TownFoodPace, AHungryManEatsMostOfHisDinnerAndIsFull)
{
	TownFoodModel town(20000, {{.food = 0.5001f, .dinner = 95, .nourishment = 1.44f, .lastCheck = 0}});
	// The first check takes him under the hungry mark: he wants seven eighths of his dinner. His town has more than
	// enough, so it does not ask him to eat less.
	town.Turn(9);
	EXPECT_EQ(town.Used(), 83u);
	EXPECT_EQ(town.Store(), 20000u - 83u);
}

TEST(TownFoodPace, AFullManEatsAgainWhenHalfOfHisBellyIsBurnt)
{
	TownFoodModel town(20000, {{.food = 1.0f, .dinner = 95, .nourishment = 1.44f, .lastCheck = 0}});
	uint32_t firstMeal = 0;
	uint32_t secondMeal = 0;
	for (uint32_t turn = 1; turn <= 12000 && secondMeal == 0; ++turn)
	{
		const auto before = town.Used();
		town.Turn(turn);
		if (town.Used() != before)
		{
			(firstMeal == 0 ? firstMeal : secondMeal) = turn;
		}
	}
	// Half a belly at 9e-5 a turn is 5556 turns, checked every 9: the first check past it
	EXPECT_EQ(firstMeal, 5562u);
	EXPECT_EQ(secondMeal - firstMeal, 5562u);
}

TEST(TownFoodPace, TheFirstLandsHomeTownDrawsItsStoreAtTheGamesPace)
{
	TownFoodModel town(20000, FirstLandsHomeTown());
	std::vector<uint32_t> stores;
	std::optional<uint32_t> foodJobTurn;
	for (uint32_t turn = k_ChecksEvery + 1; turn <= 14000; ++turn)
	{
		town.Turn(turn);
		if (!foodJobTurn.has_value() && town.FoodDesire() > k_FoodJobDesire)
		{
			foodJobTurn = turn;
		}
		if (turn % 2000 == 0)
		{
			stores.push_back(town.Store());
		}
	}
	EXPECT_EQ(town.Store() + town.Used(), 20000u);
	// Forty-four people eat about 80 each in every 5562 turns, about 0.63 food a turn, those that start least full first
	EXPECT_EQ(stores, (std::vector<uint32_t> {19127, 18497, 16648, 15510, 14771, 12737, 12307}));
	// The town wants five times its dinners, 20350, so 20000 is too little from the start: its food desire is past the
	// trigger of the food jobs at the first turn
	ASSERT_TRUE(foodJobTurn.has_value());
	EXPECT_EQ(*foodJobTurn, k_ChecksEvery + 1);
}
