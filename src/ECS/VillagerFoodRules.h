/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <bit>
#include <functional>
#include <optional>
#include <span>

#include "Enums.h"

/// How a villager's belly empties and fills: the food it burns between its checks and the life hunger costs it, when it
/// sets off to eat and where, how much a meal is and how much eating it fills it, the food a household wants for its
/// dinner, and the magic food that speeds it up for a while. Pure, tested on made-up villagers.
namespace openblack::ecs::villager_food_rules
{

/// Eating magic food speeds a villager up for this many ticks, one taken off every so many game turns
constexpr uint8_t k_FoodSpeedUpTicks = 255;
constexpr uint32_t k_FoodSpeedUpTickTurns = 10;
/// A town short of food asks its people to eat up to this share less, as much less as it wants food. The game's value is
/// a hair under three tenths.
constexpr float k_TownShortageShare = std::bit_cast<float>(0x3E999999u);
/// A housewife gossips about the storage pit for at least a set while, longer the fuller she is, by this many turns
/// for a full belly
constexpr float k_GossipTurnsPerFood = 10.0f;
/// She wanders to gossip within a little of the pit's edge: a random distance up to this
constexpr float k_GossipReach = 5.0f;

/// The magic food speed-up left after a game turn: a tick less on every tenth turn
[[nodiscard]] constexpr uint8_t TickFoodSpeedUp(uint8_t ticks, uint32_t turn)
{
	return ticks != 0 && turn % k_FoodSpeedUpTickTurns == 0 ? static_cast<uint8_t>(ticks - 1) : ticks;
}

/// What a check on a villager's belly comes from
struct DigestInputs
{
	/// Game turns since its needs were last looked at
	uint32_t turnsSinceChecked {0};
	float food {1.0f};
	/// Food burnt a turn at its walking pace, and at what food it counts as hungry
	float foodPerTurn {0.0f};
	float hungryForFood {0.5f};
	/// The life a hungry or poisoned villager loses at a check
	float hungerToLife {0.0f};
	/// The speed it goes at now and its usual walking speed: going faster than its walk burns more, while it moves
	float speed {0.0f};
	float walkingSpeed {1.0f};
	bool moving {false};
	/// The power of its player's Japanese tribe, which makes its food last longer; none without a player
	std::optional<float> tribalPower;
	bool poisoned {false};
};

/// What the check did to it
struct Digestion
{
	float food {0.0f};
	/// The life it loses
	float lifeLost {0.0f};
	/// It is hungry now: under the hungry mark
	bool hungry {false};
};

/// A check on a villager's belly: the food burnt since the last check, and the life a hungry or poisoned villager
/// loses. Nothing when no turn has passed.
[[nodiscard]] std::optional<Digestion> Digest(const DigestInputs& in);

/// Whether a hungry villager sets off to eat after its check: when its state gives way to hunger, unless it is a working
/// disciple; failing that, starving, when its state gives way to starvation. `tryToEat` tries and says whether it went.
[[nodiscard]] bool SeeksFood(bool stateGivesWayToHunger, bool stateGivesWayToStarving, bool workingDisciple, float food,
                             float starvingForFood, const std::function<bool()>& tryToEat);

/// How much food a meal is: as much of its dinner as it wants food, a little less as its town wants food
[[nodiscard]] uint32_t AmountToEat(float desireForFood, uint32_t foodForDinner, std::optional<float> townFoodDesire);
/// How much more food it needs to eat its meal than it carries
[[nodiscard]] uint32_t RequiredForMeal(uint32_t amountToEat, int16_t carried);

/// Where a hungry villager gets its meal
enum class MealPlan : uint8_t
{
	/// Nowhere: it stays hungry
	None,
	/// From what it carries, where it is
	EatHeld,
	/// At home, from what it carries and its home's store
	EatAtHome,
	/// It goes home to eat there
	GoHome,
	/// It goes to fetch food from its town's storage pit
	GoToStoragePit,
	/// With no working pit it walks to where its town leaves food
	GoToDropOff,
};

struct MealInputs
{
	/// The food it needs beyond what it carries, and what it carries
	uint32_t required {0};
	int16_t carried {0};
	bool atHome {false};
	/// The food in its home, when it has one that works
	std::optional<uint32_t> homeFood;
	/// The food in its town's storage pit, when that works
	std::optional<uint32_t> pitFood;
	/// With no working pit: the place its town leaves food is somewhere else than where it stands
	bool dropOffElsewhere {false};
};

/// Where it gets its meal: what it carries when that is enough; else home when home has enough with what it carries
/// (what it carries counted twice, as in the game); else the storage pit when that has enough; else, with no working pit,
/// where its town leaves food; else what it carries, if anything
[[nodiscard]] MealPlan PlanMeal(const MealInputs& in);

/// What eating from its hands did
struct Meal
{
	/// The food it ate, gone from its hands
	uint16_t eaten {0};
	/// How full it is after
	float food {0.0f};
};

/// It eats as much of its meal as it carries, which fills it by the share of the meal eaten, times how nourishing its
/// tribe finds food. A meal of nothing empties it, as it does in the game.
[[nodiscard]] Meal EatHeld(uint32_t amountToEat, int16_t carried, float food, float nourishment);

/// The food a household wants for its dinner: each of its people's dinner, children and all
[[nodiscard]] uint32_t FoodForDinner(std::span<const uint32_t> dinners);
/// Whether its home holds enough for the household's dinner, the want counted as the game counts it, in 16 bits
[[nodiscard]] constexpr bool IsEnoughForDinner(uint32_t wanted, uint32_t homeFood)
{
	return static_cast<uint16_t>(wanted) <= homeFood;
}
/// How long a housewife gossips about the storage pit: a while, longer the fuller she is
[[nodiscard]] uint16_t GossipTurns(uint32_t gossipTime, float food);

/// Food lying about catches a villager's eye when its interest passes this; sped-up food is twice as tempting
constexpr float k_FoodInterestThreshold = 0.25f;
constexpr float k_SpeedUpFoodInterest = 2.0f;
/// A villager not hungry carries food it picked up to the store when its town's shortage, how laden it is and how near
/// the store is come to more than this
constexpr float k_CarryFoodToStoreThreshold = 0.1f;

/// What decides whether food lying about catches a villager's eye
struct FoodInterestInputs
{
	float desireForFood {0.0f};
	/// What it carries and the most it can carry
	int16_t carried {0};
	uint32_t maxCarried {1};
	/// How much its state lets it care for food it finds
	float stateInterest {0.0f};
	/// How near the food is, from almost 1 beside it to almost 0 at the reaction's reach
	float distanceModifier {0.0f};
	bool speedUp {false};
	/// Its town's food desire, and the desire at which a town's food desire moves its people; none without a town
	std::optional<float> townFoodDesire;
	float townTrigger {0.0f};
	/// What a land's script adds to every villager's interest; none unless it sets it
	float interactDesire {0.0f};
};
/// Whether it is interested in the food: its own want of food, its room to carry more, its state and the food's nearness
/// together, or else its town's want of food in place of its own
[[nodiscard]] bool IsInterestedInFood(const FoodInterestInputs& in);

/// Whether a villager that picked up food and isn't hungry takes it to the store: by its town's shortage, the share of a
/// full load it carries and how near the store is
[[nodiscard]] bool CarriesFoodToStore(float townShortage, int16_t carried, uint32_t maxCarried, float storeNearness);

/// What a villager that runs out of life while hungry dies of: chanting, when it was chanting at a worship site, else
/// starvation
[[nodiscard]] constexpr DeathReason HungerDeath(bool chanting)
{
	return chanting ? DeathReason::Chant : DeathReason::Starving;
}

} // namespace openblack::ecs::villager_food_rules
