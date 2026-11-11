/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureDecisionTree.h"
#include "OracleData.h"

using namespace openblack;

namespace
{
constexpr size_t k_MostExamples = 16;
constexpr uint32_t k_NoStep = 0xFFFFFFFF;

std::vector<creature_tree::Episode> ExamplesOf(const oracle::Table& table, size_t row)
{
	const auto count = table.Bits(row, *table.Column("count"));
	std::vector<creature_tree::Episode> episodes;
	for (uint32_t i = 0; i < count && i < k_MostExamples; ++i)
	{
		episodes.push_back({.feedback = table.Float(row, *table.Column("f" + std::to_string(i)))});
	}
	return episodes;
}
} // namespace

// The base 2 logarithm of each share of up to 128 examples, bit for bit
TEST(OracleCreatureTreeMaths, TheLogarithmOfAShareIsTheGames)
{
	const auto table = oracle::Table::Load("tree_share_log2.txt");
	ASSERT_TRUE(table.has_value());
	const auto share = table->Column("share");
	const auto log2 = table->Column("log2");
	ASSERT_TRUE(share && log2);
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const float ours = creature_tree::ShareLog2(table->Float(row, *share));
		if (std::bit_cast<uint32_t>(ours) != table->Bits(row, *log2))
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": share " << oracle::Hex(table->Float(row, *share)) << ": the game gives "
				              << oracle::Hex(table->Float(row, *log2)) << ", ours gives " << oracle::Hex(ours);
			}
		}
	}
	EXPECT_EQ(different, 0u);
}

// How mixed the feedback of a node's examples is, bit for bit
TEST(OracleCreatureTreeMaths, TheEntropyOfTheExamplesIsTheGames)
{
	const auto table = oracle::Table::Load("tree_entropy.txt");
	ASSERT_TRUE(table.has_value());
	const auto entropy = table->Column("entropy");
	ASSERT_TRUE(entropy && table->Column("count"));
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const float ours = creature_tree::Entropy(ExamplesOf(*table, row));
		if (std::bit_cast<uint32_t>(ours) != table->Bits(row, *entropy))
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives " << oracle::Hex(table->Float(row, *entropy))
				              << ", ours gives " << oracle::Hex(ours);
			}
		}
	}
	EXPECT_EQ(different, 0u);
}

// The step of a node comes from the mean feedback of its examples; a mean far from each step has none
TEST(OracleCreatureTreeMaths, TheStepOfANodeIsTheGames)
{
	const auto table = oracle::Table::Load("tree_leaf_step.txt");
	ASSERT_TRUE(table.has_value());
	const auto step = table->Column("step");
	ASSERT_TRUE(step && table->Column("count"));
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto found = creature_tree::StepOf(creature_tree::MeanFeedback(ExamplesOf(*table, row)));
		const auto ours = found.has_value() ? static_cast<uint32_t>(*found) : k_NoStep;
		if (ours != table->Bits(row, *step))
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives " << table->Bits(row, *step) << ", ours gives " << ours;
			}
		}
	}
	EXPECT_EQ(different, 0u);
}
