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

#include "Creature/CreatureFriendship.h"
#include "Creature/CreatureLearning.h"
#include "OracleData.h"

using namespace openblack;
using creature_learning::CreatureAttitude;

namespace
{

/// The columns of a table row, read by name
struct Row
{
	const oracle::Table& table;
	size_t row;

	[[nodiscard]] float F(const std::string& name) const { return table.Float(row, table.Column(name).value_or(0)); }
	[[nodiscard]] uint32_t U(const std::string& name) const { return table.Bits(row, table.Column(name).value_or(0)); }
};

/// The number of rows that differ from the game, with the first few reported
class Mismatches
{
public:
	void Check(size_t row, uint32_t ours, uint32_t game)
	{
		if (ours != game && ++_count <= 10)
		{
			ADD_FAILURE() << "row " << row << ": ours " << std::bit_cast<float>(ours) << " game " << std::bit_cast<float>(game);
		}
	}
	[[nodiscard]] size_t Count() const { return _count; }

private:
	size_t _count {0};
};

} // namespace

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(OracleCreatureFriendship, TheFadeOfAThinkingTurnMatchesTheGame)
{
	const auto table = oracle::Table::Load("friend_fade.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 0u);
	Mismatches mismatches;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const Row row {*table, index};
		std::vector<CreatureAttitude> attitudes {
		    {.creature = 1, .howImpressive = row.F("impressive"), .attention = row.F("attention")}};
		creature_friendship::Fade(attitudes);
		mismatches.Check(index, std::bit_cast<uint32_t>(attitudes.front().howImpressive), row.U("impressive_after"));
		mismatches.Check(index, std::bit_cast<uint32_t>(attitudes.front().attention), row.U("attention_after"));
	}
	EXPECT_EQ(mismatches.Count(), 0u);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(OracleCreatureFriendship, HowMuchACreatureMissesItsFriendsMatchesTheGame)
{
	const auto table = oracle::Table::Load("friend_missed.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 0u);
	Mismatches mismatches;
	size_t missing = 0;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const Row row {*table, index};
		std::vector<CreatureAttitude> attitudes;
		for (uint32_t slot = 0; slot < row.U("count"); ++slot)
		{
			attitudes.push_back({.creature = slot,
			                     .howNice = row.F("nice" + std::to_string(slot)),
			                     .attention = row.F("attention" + std::to_string(slot))});
		}
		const float ours = creature_friendship::MissedFriends(attitudes);
		if (ours > 0.0f)
		{
			++missing;
		}
		mismatches.Check(index, std::bit_cast<uint32_t>(ours), row.U("missed"));
	}
	EXPECT_EQ(mismatches.Count(), 0u);
	// The data must hold many cases where the creature misses a friend, else it tests little
	EXPECT_GT(missing, table->Rows() / 5);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(OracleCreatureFriendship, AChangeToTheAttentionMatchesTheGame)
{
	const auto table = oracle::Table::Load("friend_attention.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 0u);
	Mismatches mismatches;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const Row row {*table, index};
		CreatureAttitude attitude {.creature = 1, .attention = row.F("attention")};
		creature_friendship::ChangeAttention(attitude, row.F("change"));
		mismatches.Check(index, std::bit_cast<uint32_t>(attitude.attention), row.U("attention_after"));
	}
	EXPECT_EQ(mismatches.Count(), 0u);
}
