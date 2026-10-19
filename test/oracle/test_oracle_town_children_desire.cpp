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
#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "ECS/Components/TownDesire.h"
#include "ECS/TownDesire.h"
#include "InfoConstants.h"
#include "OracleData.h"

using namespace openblack;
namespace td = openblack::ecs::town_desire;

namespace
{

constexpr size_t k_Food = 0;
constexpr size_t k_Protection = 3;
constexpr size_t k_Mercy = 4;
constexpr size_t k_Children = 8;

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

} // namespace

TEST(OracleTownChildrenDesire, DataIsComplete)
{
	const auto desires = oracle::Table::Load("town_children_desire.txt");
	ASSERT_TRUE(desires.has_value());
	EXPECT_GT(desires->Rows(), 3000u);
	const auto significance = oracle::Table::Load("town_desire_significance.txt");
	ASSERT_TRUE(significance.has_value());
	EXPECT_GT(significance->Rows(), 2000u);
}

TEST(OracleTownChildrenDesire, DesireAsTheGameWorksItOut)
{
	const auto table = oracle::Table::Load("town_children_desire.txt");
	ASSERT_TRUE(table.has_value());
	const auto foodRaw = ColumnOf(*table, "food_raw");
	const auto foodBoost = ColumnOf(*table, "food_boost");
	const auto foodBoostA = ColumnOf(*table, "food_boost_a");
	const auto children = ColumnOf(*table, "children");
	const auto childPlaces = ColumnOf(*table, "child_places");
	const auto adults = ColumnOf(*table, "adults");
	const auto adultPlaces = ColumnOf(*table, "adult_places");
	const auto protection = ColumnOf(*table, "protection");
	const auto protectionBoost = ColumnOf(*table, "protection_boost");
	const auto mercy = ColumnOf(*table, "mercy");
	const auto hasPlayer = ColumnOf(*table, "has_player");
	const auto alignment = ColumnOf(*table, "alignment");
	const auto power = ColumnOf(*table, "power");
	const auto creche = ColumnOf(*table, "creche");
	const auto desire = ColumnOf(*table, "desire");

	const GTownInfo town {};
	const std::array<GTownDesireInfo, td::k_Count> info {};
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		ecs::components::TownDesire wants {};
		wants.raw.at(k_Food) = table->Float(row, foodRaw);
		wants.boost.at(k_Food) = table->Float(row, foodBoost);
		wants.boostA.at(k_Food) = table->Float(row, foodBoostA);
		wants.desire.at(k_Protection) = table->Float(row, protection);
		wants.boost.at(k_Protection) = table->Float(row, protectionBoost);
		wants.desire.at(k_Mercy) = table->Float(row, mercy);
		td::DesireInputs in {};
		in.stats.children = table->Bits(row, children);
		in.stats.childPlaces = table->Bits(row, childPlaces);
		in.stats.adults = table->Bits(row, adults);
		in.stats.adultPlaces = table->Bits(row, adultPlaces);
		in.hasPlayer = table->Bits(row, hasPlayer) != 0;
		in.alignment = table->Float(row, alignment);
		in.tribalPower4 = table->Float(row, power);
		in.crecheFunctional = table->Bits(row, creche) == 1;
		const td::DesireContext context {.desire = wants, .in = in, .town = town, .info = info};
		const float ours = td::DesireForChildren(context);
		if (!Same(ours, table->Bits(row, desire)))
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives " << oracle::Hex(table->Float(row, desire))
				              << ", ours gives " << oracle::Hex(ours);
			}
		}
	}
	EXPECT_EQ(different, 0u);
}

TEST(OracleTownChildrenDesire, SignificanceAsTheGameWorksItOut)
{
	const auto table = oracle::Table::Load("town_desire_significance.txt");
	ASSERT_TRUE(table.has_value());
	const auto desire = ColumnOf(*table, "desire");
	const auto boost = ColumnOf(*table, "boost");
	const auto boostA = ColumnOf(*table, "boost_a");
	const auto trigger = ColumnOf(*table, "trigger");
	const auto significance = ColumnOf(*table, "significance");
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		ecs::components::TownDesire wants {};
		wants.desire.at(k_Children) = table->Float(row, desire);
		wants.boost.at(k_Children) = table->Float(row, boost);
		wants.boostA.at(k_Children) = table->Float(row, boostA);
		GTownDesireInfo info {};
		info.desireTriggersVillagerAction = table->Float(row, trigger);
		const float ours = td::GetDesireSignificanceToVillager(wants, info, k_Children);
		if (!Same(ours, table->Bits(row, significance)))
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives " << oracle::Hex(table->Float(row, significance))
				              << ", ours gives " << oracle::Hex(ours);
			}
		}
	}
	EXPECT_EQ(different, 0u);
}

TEST(OracleTownChildrenDesire, TheFirstLandsBoostHoldsBackAHalfFedTown)
{
	// The first land's script takes 0.2 off its home town's desire for children. A town that feels 0.45 or less
	// then never wants children enough for a couple at home to make one.
	ecs::components::TownDesire wants {};
	wants.desire.at(k_Children) = 0.45f;
	wants.boost.at(k_Children) = -0.2f;
	GTownDesireInfo info {};
	info.desireTriggersVillagerAction = 0.25f;
	EXPECT_EQ(td::GetDesireSignificanceToVillager(wants, info, k_Children), 0.0f);
	wants.desire.at(k_Children) = 0.71f;
	EXPECT_GT(td::GetDesireSignificanceToVillager(wants, info, k_Children), 0.0f);
}
