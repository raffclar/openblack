/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>

#include <gtest/gtest.h>

#include "ECS/WorshipperRules.h"
#include "Help/VillageVoices.h"
#include "OracleData.h"

using namespace openblack;

// The data comes from tools/oracle/cases/town_desire_voices_cases.py: the game's own functions on the real FPU.

TEST(OracleTownDesireVoices, TheScoreOfAVoice)
{
	const auto table = oracle::Table::Load("desire_voice_score.txt");
	ASSERT_TRUE(table.has_value());
	const auto heard = table->Column("since_heard");
	const auto seen = table->Column("since_seen");
	const auto distance = table->Column("distance");
	const auto desire = table->Column("desire");
	const auto same = table->Column("same");
	const auto score = table->Column("score");
	ASSERT_TRUE(heard && seen && distance && desire && same && score);
	ASSERT_GT(table->Rows(), 400u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const float ours = guidance::DesireVoiceScore({.turnsSinceHeard = table->Bits(row, *heard),
		                                               .turnsSinceSeen = table->Bits(row, *seen),
		                                               .distance = table->Float(row, *distance),
		                                               .desire = table->Float(row, *desire),
		                                               .sameAsLast = table->Bits(row, *same) != 0});
		if (std::bit_cast<uint32_t>(ours) != table->Bits(row, *score))
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives " << oracle::Hex(table->Float(row, *score))
				              << ", ours gives " << oracle::Hex(ours);
			}
		}
	}
	EXPECT_EQ(different, 0u) << "of " << table->Rows() << " rows";
}

TEST(OracleTownDesireVoices, TheVoiceOfARow)
{
	const auto table = oracle::Table::Load("desire_voice_pick.txt");
	ASSERT_TRUE(table.has_value());
	const auto row = table->Column("row");
	const auto value = table->Column("value");
	const auto draw = table->Column("draw");
	const auto sample = table->Column("sample");
	ASSERT_TRUE(row && value && draw && sample);
	ASSERT_GT(table->Rows(), 500u);
	size_t different = 0;
	for (size_t i = 0; i < table->Rows(); ++i)
	{
		const auto ours = guidance::DesireVoice(table->Bits(i, *row), table->Float(i, *value), table->Float(i, *draw));
		if (ours != table->Bits(i, *sample))
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << i << ": the game gives " << table->Bits(i, *sample) << ", ours gives " << ours;
			}
		}
	}
	EXPECT_EQ(different, 0u) << "of " << table->Rows() << " rows";
}

TEST(OracleTownDesireVoices, AWorshipSiteWantsFood)
{
	const auto table = oracle::Table::Load("worship_site_food_desire.txt");
	ASSERT_TRUE(table.has_value());
	const auto need = table->Column("need");
	const auto pot = table->Column("pot");
	const auto desire = table->Column("desire");
	ASSERT_TRUE(need && pot && desire);
	ASSERT_GT(table->Rows(), 300u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const float ours = ecs::worshipper_rules::SiteDesireForFood(table->Float(row, *need), table->Bits(row, *pot));
		if (std::bit_cast<uint32_t>(ours) != table->Bits(row, *desire))
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives " << oracle::Hex(table->Float(row, *desire))
				              << ", ours gives " << oracle::Hex(ours);
			}
		}
	}
	EXPECT_EQ(different, 0u) << "of " << table->Rows() << " rows";
}

TEST(OracleTownDesireVoices, TheStrainSoundOfATemple)
{
	const auto table = oracle::Table::Load("temple_strain_sound.txt");
	ASSERT_TRUE(table.has_value());
	const auto local = table->Column("local");
	const auto level = table->Column("level");
	const auto most = table->Column("most");
	const auto after = table->Column("level_after");
	ASSERT_TRUE(local && level && most && after);
	ASSERT_GT(table->Rows(), 200u);
	// A game turn is 100 milliseconds, and the level moves a thousandth for each millisecond
	const float step = 100.0f * 0.001f;
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		// Only the temple of the player at this machine changes its level
		const float ours =
		    table->Bits(row, *local) != 0
		        ? ecs::worshipper_rules::EaseStrainSound(table->Float(row, *level), table->Float(row, *most), step)
		        : table->Float(row, *level);
		if (std::bit_cast<uint32_t>(ours) != table->Bits(row, *after))
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives " << oracle::Hex(table->Float(row, *after))
				              << ", ours gives " << oracle::Hex(ours);
			}
		}
	}
	EXPECT_EQ(different, 0u) << "of " << table->Rows() << " rows";
}
