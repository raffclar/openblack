/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <set>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/ComputerPlayerPersonality.h"

using namespace openblack;
using namespace openblack::computer_player::personality;

TEST(ComputerPlayerPersonality, TheNeedsAreNineteenDifferentNames)
{
	std::set<std::string_view> names;
	for (const auto& aspect : k_Aspects)
	{
		names.insert(aspect.name);
		EXPECT_GE(aspect.builtIn, 0.0f);
		EXPECT_LE(aspect.builtIn, k_HighestWeight);
	}
	EXPECT_EQ(names.size(), 19u);
}

TEST(ComputerPlayerPersonality, ASeedAndAGodAlwaysGiveTheSameWeights)
{
	const auto first = WeightsFor(5u, PlayerNames::PLAYER_THREE);
	const auto second = WeightsFor(5u, PlayerNames::PLAYER_THREE);
	ASSERT_EQ(first.size(), k_Aspects.size());
	ASSERT_EQ(second.size(), first.size());
	for (size_t i = 0; i < first.size(); ++i)
	{
		EXPECT_EQ(first[i].name, k_Aspects[i].name);
		EXPECT_EQ(first[i].value, second[i].value);
	}
}

TEST(ComputerPlayerPersonality, AnotherSeedOrAnotherGodGivesOtherWeights)
{
	const auto differs = [](const std::vector<Weight>& a, const std::vector<Weight>& b) {
		for (size_t i = 0; i < a.size(); ++i)
		{
			if (a[i].value != b[i].value)
			{
				return true;
			}
		}
		return false;
	};
	const auto base = WeightsFor(5u, PlayerNames::PLAYER_THREE);
	EXPECT_TRUE(differs(base, WeightsFor(6u, PlayerNames::PLAYER_THREE)));
	EXPECT_TRUE(differs(base, WeightsFor(5u, PlayerNames::PLAYER_FOUR)));
}

TEST(ComputerPlayerPersonality, EveryWeightIsBetweenTheLeastShareOfItsBuiltInWeightAndTheHighestWeight)
{
	for (uint32_t seed = 0; seed < 50; ++seed)
	{
		const auto weights = WeightsFor(seed, PlayerNames::PLAYER_ONE);
		for (size_t i = 0; i < weights.size(); ++i)
		{
			EXPECT_GE(weights[i].value, k_Aspects[i].builtIn * k_LeastShare);
			EXPECT_LE(weights[i].value, k_HighestWeight);
		}
	}
}

TEST(ComputerPlayerPersonality, TheWeightScalesTheBuiltInWeightByTheShare)
{
	const Aspect aspect {"Test", 0.4f};
	EXPECT_FLOAT_EQ(WeightOf(aspect, 0.0f), 0.4f * k_LeastShare);
	EXPECT_FLOAT_EQ(WeightOf(aspect, 1.0f), 0.4f * k_MostShare);
	// A built-in weight of 1 never goes above the highest weight
	EXPECT_FLOAT_EQ(WeightOf({"Test", 1.0f}, 1.0f), k_HighestWeight);
}

TEST(ComputerPlayerPersonality, TheStreamGivesSharesBelowOneThatSpreadOverTheRange)
{
	Stream stream(1u, PlayerNames::PLAYER_ONE);
	float least = 1.0f;
	float most = 0.0f;
	for (int i = 0; i < 1000; ++i)
	{
		const auto share = stream.NextShare();
		ASSERT_GE(share, 0.0f);
		ASSERT_LT(share, 1.0f);
		least = std::min(least, share);
		most = std::max(most, share);
	}
	EXPECT_LT(least, 0.05f);
	EXPECT_GT(most, 0.95f);
}
