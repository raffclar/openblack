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

#include <gtest/gtest.h>

#include "ECS/LivingBirth.h"
#include "ECS/SpecialVillagers.h"

using namespace openblack;
using namespace openblack::ecs::special_villager;

namespace
{
std::array<Row, k_NameCount> AlternatingRows()
{
	std::array<Row, k_NameCount> rows {};
	for (size_t i = 0; i < rows.size(); ++i)
	{
		rows.at(i) = {.sex = i % 2 == 0 ? SexType::Male : SexType::Female, .hasName = true};
	}
	return rows;
}
} // namespace

TEST(SpecialVillagers, TwoInTenTryForAName)
{
	EXPECT_TRUE(TriesNamed(0, false));
	EXPECT_TRUE(TriesNamed(1, false));
	EXPECT_FALSE(TriesNamed(2, false));
	EXPECT_FALSE(TriesNamed(9, false));
}

TEST(SpecialVillagers, NetworkGameGivesNoNames)
{
	for (uint32_t draw = 0; draw < k_Draw; ++draw)
	{
		EXPECT_FALSE(TriesNamed(draw, true));
	}
}

TEST(SpecialVillagers, LivingThingDrawsBelowHalfItsStartAge)
{
	// The start ages of the villager, creature and animal tables: 25, 23 and 20
	EXPECT_EQ(ecs::living_birth::AgeDrawRange(25), 12u);
	EXPECT_EQ(ecs::living_birth::AgeDrawRange(23), 11u);
	EXPECT_EQ(ecs::living_birth::AgeDrawRange(20), 10u);
	// A range of 0 draws no number
	EXPECT_EQ(ecs::living_birth::AgeDrawRange(1), 0u);
	EXPECT_EQ(ecs::living_birth::AgeDrawRange(0), 0u);
}

TEST(SpecialVillagers, PickWalksFromTheDrawToTheFirstFreeNameOfTheSex)
{
	const auto rows = AlternatingRows();
	std::array<bool, k_NameCount> used {};
	EXPECT_EQ(PickName(rows, used, SexType::Male, 4), 4);
	EXPECT_EQ(PickName(rows, used, SexType::Female, 4), 5);
	used.at(5) = true;
	EXPECT_EQ(PickName(rows, used, SexType::Female, 4), 7);
	// Round the end of the table
	EXPECT_EQ(PickName(rows, used, SexType::Male, 47), 0);
	// Any sex
	EXPECT_EQ(PickName(rows, used, SexType::None, 5), 6);
}

TEST(SpecialVillagers, NoNameWhenEveryOneOfTheSexIsGiven)
{
	auto rows = AlternatingRows();
	std::array<bool, k_NameCount> used {};
	for (size_t i = 1; i < used.size(); i += 2)
	{
		used.at(i) = true;
	}
	EXPECT_FALSE(PickName(rows, used, SexType::Female, 0).has_value());
	// Rows without a name are passed over
	rows.at(2).hasName = false;
	EXPECT_EQ(PickName(rows, used, SexType::Male, 2), 4);
}

TEST(SpecialVillagers, NamedVillagerTakesItsNamesAgeUnlessBorn)
{
	EXPECT_EQ(NamedAge(25, 40), 40u);
	EXPECT_EQ(NamedAge(1, 40), 1u);
}
