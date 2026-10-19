/*******************************************************************************
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
#include <bit>
#include <ios>
#include <optional>

#include <gtest/gtest.h>

#include "ECS/Components/TownDesire.h"
#include "ECS/TownDesire.h"
#include "ECS/VillagerFoodRules.h"
#include "ECS/VillagerNeeds.h"
#include "InfoConstants.h"
#include "OracleData.h"

using namespace openblack;
namespace rules = openblack::ecs::villager_food_rules;
namespace needs = openblack::ecs::villager_needs;
namespace td = openblack::ecs::town_desire;

namespace
{

/// The data's mark for a check that took no life
constexpr uint32_t k_NoLifeLost = 0xFFFFFFFFu;

/// The same float, or both zero of either sign
bool Same(float ours, uint32_t game)
{
	return std::bit_cast<uint32_t>(ours) == game || (ours == 0.0f && std::bit_cast<float>(game) == 0.0f);
}

/// The column, or a failure that names it
size_t ColumnOf(const oracle::Table& table, const char* name)
{
	const auto column = table.Column(name);
	EXPECT_TRUE(column.has_value()) << name;
	return column.value_or(0);
}

/// The town's food desire, when the row has a town
std::optional<float> TownDesireOf(const oracle::Table& table, size_t row, size_t hasTown, size_t desire)
{
	return table.Bits(row, hasTown) != 0 ? std::optional(table.Float(row, desire)) : std::nullopt;
}

/// Counts a row that differs, and reports the first ones
void Report(size_t& different, size_t row, const char* what, uint32_t game, uint32_t ours)
{
	++different;
	if (different <= 20)
	{
		ADD_FAILURE() << "row " << row << " " << what << ": the game gives " << std::hex << game << ", ours gives " << ours;
	}
}

} // namespace

TEST(OracleVillagerFood, DataIsComplete)
{
	for (const auto* name : {"villager_food_desire.txt", "villager_amount_to_eat.txt", "villager_eat_held.txt",
	                         "villager_digest.txt", "town_food_desire.txt"})
	{
		const auto table = oracle::Table::Load(name);
		ASSERT_TRUE(table.has_value()) << name;
		EXPECT_GT(table->Rows(), 3000u) << name;
	}
}

TEST(OracleVillagerFood, DesireForFoodAsTheGameWorksItOut)
{
	const auto table = oracle::Table::Load("villager_food_desire.txt");
	ASSERT_TRUE(table.has_value());
	const auto food = ColumnOf(*table, "food");
	const auto desire = ColumnOf(*table, "desire");
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const float ours = needs::DesireForFood(table->Float(row, food));
		if (!Same(ours, table->Bits(row, desire)))
		{
			Report(different, row, "desire", table->Bits(row, desire), std::bit_cast<uint32_t>(ours));
		}
	}
	EXPECT_EQ(different, 0u);
}

TEST(OracleVillagerFood, MealAsTheGameWorksItOut)
{
	const auto table = oracle::Table::Load("villager_amount_to_eat.txt");
	ASSERT_TRUE(table.has_value());
	const auto food = ColumnOf(*table, "food");
	const auto dinner = ColumnOf(*table, "dinner");
	const auto hasTown = ColumnOf(*table, "has_town");
	const auto townDesire = ColumnOf(*table, "town_desire");
	const auto amount = ColumnOf(*table, "amount");
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto ours = rules::AmountToEat(needs::DesireForFood(table->Float(row, food)), table->Bits(row, dinner),
		                                     TownDesireOf(*table, row, hasTown, townDesire));
		if (ours != table->Bits(row, amount))
		{
			Report(different, row, "amount", table->Bits(row, amount), ours);
		}
	}
	EXPECT_EQ(different, 0u);
}

TEST(OracleVillagerFood, EatingAsTheGameWorksItOut)
{
	const auto table = oracle::Table::Load("villager_eat_held.txt");
	ASSERT_TRUE(table.has_value());
	const auto food = ColumnOf(*table, "food");
	const auto dinner = ColumnOf(*table, "dinner");
	const auto hasTown = ColumnOf(*table, "has_town");
	const auto townDesire = ColumnOf(*table, "town_desire");
	const auto carried = ColumnOf(*table, "carried");
	const auto nourishment = ColumnOf(*table, "nourishment");
	const auto meal = ColumnOf(*table, "meal");
	const auto eaten = ColumnOf(*table, "eaten");
	const auto after = ColumnOf(*table, "food_after");
	size_t different = 0;
	size_t compared = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto amount = rules::AmountToEat(needs::DesireForFood(table->Float(row, food)), table->Bits(row, dinner),
		                                       TownDesireOf(*table, row, hasTown, townDesire));
		if (amount != table->Bits(row, meal))
		{
			Report(different, row, "meal", table->Bits(row, meal), amount);
			continue;
		}
		// A meal of nothing empties the game's belly; ours stays as it is (deviations register, 26)
		if (amount == 0)
		{
			continue;
		}
		++compared;
		const auto ours = rules::EatHeld(amount, static_cast<int16_t>(table->Bits(row, carried)), table->Float(row, food),
		                                 table->Float(row, nourishment));
		if (ours.eaten != table->Bits(row, eaten))
		{
			Report(different, row, "eaten", table->Bits(row, eaten), ours.eaten);
		}
		if (!Same(ours.food, table->Bits(row, after)))
		{
			Report(different, row, "food", table->Bits(row, after), std::bit_cast<uint32_t>(ours.food));
		}
	}
	EXPECT_EQ(different, 0u);
	EXPECT_GT(compared, 3000u);
}

TEST(OracleVillagerFood, CheckOnTheBellyAsTheGameWorksItOut)
{
	const auto table = oracle::Table::Load("villager_digest.txt");
	ASSERT_TRUE(table.has_value());
	const auto since = ColumnOf(*table, "since");
	const auto food = ColumnOf(*table, "food");
	const auto foodPerTurn = ColumnOf(*table, "food_per_turn");
	const auto speed = ColumnOf(*table, "speed");
	const auto baseSpeed = ColumnOf(*table, "base_speed");
	const auto hasPlayer = ColumnOf(*table, "has_player");
	const auto power = ColumnOf(*table, "power");
	const auto moving = ColumnOf(*table, "moving");
	const auto hungry = ColumnOf(*table, "hungry");
	const auto hungerToLife = ColumnOf(*table, "hunger_to_life");
	const auto poisoned = ColumnOf(*table, "poisoned");
	const auto after = ColumnOf(*table, "food_after");
	const auto lifeLost = ColumnOf(*table, "life_lost");
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto ours = rules::Digest({
		    .turnsSinceChecked = table->Bits(row, since),
		    .food = table->Float(row, food),
		    .foodPerTurn = table->Float(row, foodPerTurn),
		    .hungryForFood = table->Float(row, hungry),
		    .hungerToLife = table->Float(row, hungerToLife),
		    .speed = static_cast<float>(table->Bits(row, speed)),
		    .walkingSpeed = static_cast<float>(table->Bits(row, baseSpeed)),
		    .moving = table->Bits(row, moving) != 0,
		    .tribalPower = table->Bits(row, hasPlayer) != 0 ? std::optional(table->Float(row, power)) : std::nullopt,
		    .poisoned = table->Bits(row, poisoned) != 0,
		});
		ASSERT_TRUE(ours.has_value());
		if (!Same(ours->food, table->Bits(row, after)))
		{
			Report(different, row, "food", table->Bits(row, after), std::bit_cast<uint32_t>(ours->food));
		}
		const auto game = table->Bits(row, lifeLost);
		const bool gameTookLife = game != k_NoLifeLost;
		const bool weTookLife = ours->hungry || table->Bits(row, poisoned) != 0;
		if (gameTookLife != weTookLife || (gameTookLife && !Same(ours->lifeLost, game)))
		{
			Report(different, row, "life", game, std::bit_cast<uint32_t>(ours->lifeLost));
		}
	}
	EXPECT_EQ(different, 0u);
}

TEST(OracleVillagerFood, TownDesireAsTheGameWorksItOut)
{
	const auto table = oracle::Table::Load("town_food_desire.txt");
	ASSERT_TRUE(table.has_value());
	const auto carried = ColumnOf(*table, "carried");
	const auto store = ColumnOf(*table, "store");
	const auto storeFood = ColumnOf(*table, "store_food");
	const auto wanted = ColumnOf(*table, "wanted");
	const auto dinners = ColumnOf(*table, "dinners");
	const auto desire = ColumnOf(*table, "desire");
	const ecs::components::TownDesire wants {};
	const std::array<GTownDesireInfo, td::k_Count> info {};
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		td::DesireInputs in {};
		in.stats.foodCarried = table->Float(row, carried);
		in.stats.foodForDinner = table->Float(row, dinners);
		if (table->Bits(row, store) == 1)
		{
			in.storageFood = table->Bits(row, storeFood);
		}
		else if (table->Bits(row, store) == 2)
		{
			in.potFood = table->Bits(row, storeFood);
		}
		GTownInfo town {};
		town.foodWantedMultiplier = table->Float(row, wanted);
		const td::DesireContext context {.desire = wants, .in = in, .town = town, .info = info};
		const float ours = td::DesireForFood(context);
		if (!Same(ours, table->Bits(row, desire)))
		{
			Report(different, row, "desire", table->Bits(row, desire), std::bit_cast<uint32_t>(ours));
		}
	}
	EXPECT_EQ(different, 0u);
}
