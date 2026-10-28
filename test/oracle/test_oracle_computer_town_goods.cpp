/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <bit>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/ComputerMindRules.h"
#include "OracleData.h"

using namespace openblack;
using namespace openblack::computer_mind_rules;

namespace
{
constexpr uint32_t k_NoTown = 0xFFFFFFFFU;
} // namespace

TEST(OracleComputerTownGoods, FoodShortfallAsTheGameWorksItOut)
{
	const auto table = oracle::Table::Load("computer_food_shortfall.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 400U);
	const auto wanted = table->Column("wanted");
	const auto held = table->Column("held");
	const auto amount = table->Column("amount");
	ASSERT_TRUE(wanted && held && amount);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto ours = FoodShortfall(table->Float(row, *wanted), table->Bits(row, *held));
		if (static_cast<uint32_t>(ours) != table->Bits(row, *amount) && different++ < 10)
		{
			ADD_FAILURE() << "wanted " << table->Float(row, *wanted) << " held " << table->Bits(row, *held) << ": ours " << ours
			              << ", the game's " << static_cast<int32_t>(table->Bits(row, *amount));
		}
	}
	EXPECT_EQ(different, 0U);
}

TEST(OracleComputerTownGoods, FishFarmWishAsTheGameWorksItOut)
{
	const auto table = oracle::Table::Load("computer_fish_farm_wish.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 400U);
	const auto distance = table->Column("distance");
	const auto pull = table->Column("pull");
	const auto value = table->Column("value");
	ASSERT_TRUE(distance && pull && value);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const float wish = FishFarmWish(table->Float(row, *distance), table->Float(row, *pull));
		// The game gives nothing for a farm that it does not want
		const uint32_t ours = wish > 0.0f ? std::bit_cast<uint32_t>(wish) : 0U;
		if (ours != table->Bits(row, *value) && different++ < 10)
		{
			ADD_FAILURE() << "distance " << table->Float(row, *distance) << " pull " << table->Float(row, *pull) << ": ours "
			              << oracle::Hex(wish);
		}
	}
	EXPECT_EQ(different, 0U);
}

TEST(OracleComputerTownGoods, GivingTownAsTheGameChoosesIt)
{
	const auto table = oracle::Table::Load("computer_giving_town.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 500U);
	const auto need = table->Column("need");
	const auto pull = table->Column("pull");
	const std::array stocks {table->Column("stock0"), table->Column("stock1"), table->Column("stock2")};
	const std::array raws {table->Column("raw0"), table->Column("raw1"), table->Column("raw2")};
	const auto value = table->Column("value");
	const auto index = table->Column("index");
	const auto ask = table->Column("ask");
	ASSERT_TRUE(need && pull && value && index && ask);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		std::vector<GivingTown> towns;
		for (size_t i = 0; i < stocks.size(); ++i)
		{
			ASSERT_TRUE(stocks.at(i) && raws.at(i));
			towns.push_back({.stock = table->Bits(row, *stocks.at(i)),
			                 .rawDesire = table->Float(row, *raws.at(i)),
			                 .pull = table->Float(row, *pull)});
		}
		const float rowNeed = table->Float(row, *need);
		const auto choice = ChooseGivingTown(towns, rowNeed);
		const uint32_t ourIndex = choice.has_value() ? static_cast<uint32_t>(choice->index) : k_NoTown;
		const uint32_t ourValue = choice.has_value() ? std::bit_cast<uint32_t>(choice->value) : 0U;
		const uint32_t ourAsk = choice.has_value() ? std::bit_cast<uint32_t>(OtherTownAsk(rowNeed)) : 0U;
		const bool same =
		    ourIndex == table->Bits(row, *index) && ourValue == table->Bits(row, *value) && ourAsk == table->Bits(row, *ask);
		if (!same && different++ < 10)
		{
			ADD_FAILURE() << "row " << row << " need " << rowNeed << ": ours town " << ourIndex << " value "
			              << oracle::Hex(choice.has_value() ? choice->value : 0.0f);
		}
	}
	EXPECT_EQ(different, 0U);
}
