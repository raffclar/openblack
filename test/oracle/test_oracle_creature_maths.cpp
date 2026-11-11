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
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreatureReactionRules.h"
#include "Magic/AreaEffect.h"
#include "OracleData.h"

using namespace openblack;

namespace
{
/// Loads a table, finds its columns, and compares the bits of each row with the game's bits in the last column. The
/// function gives the bits of our value for a row, from the values of the columns in the order of the names.
void ExpectTheBitsOfTheGame(std::string_view file, const std::vector<std::string>& names,
                            const std::function<uint32_t(const std::vector<uint32_t>&)>& ours)
{
	const auto table = oracle::Table::Load(std::string(file));
	ASSERT_TRUE(table.has_value());
	std::vector<size_t> columns;
	for (const auto& name : names)
	{
		const auto column = table->Column(name);
		ASSERT_TRUE(column.has_value()) << name;
		columns.push_back(*column);
	}
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		std::vector<uint32_t> values;
		for (const auto column : columns)
		{
			values.push_back(table->Bits(row, column));
		}
		const auto game = values.back();
		const auto result = ours(values);
		if (result != game)
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << file << " row " << row << ": the game gives " << oracle::Hex(std::bit_cast<float>(game))
				              << ", ours gives " << oracle::Hex(std::bit_cast<float>(result));
			}
		}
	}
	EXPECT_EQ(different, 0u);
}

float F(uint32_t bits)
{
	return std::bit_cast<float>(bits);
}

uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}
} // namespace

// The priority of an action from the opinion has the bits of the game
TEST(OracleCreatureMaths, TheActionPriorityHasTheBitsOfTheGame)
{
	ExpectTheBitsOfTheGame(
	    "creature_action_priority.txt", {"opinion", "disabled", "priority"},
	    [](const std::vector<uint32_t>& v) { return Bits(creature_planner::ActionPriority(F(v[0]), v[1] != 0)); });
}

// The novelty of an action has the bits of the game
TEST(OracleCreatureMaths, TheNoveltyHasTheBitsOfTheGame)
{
	ExpectTheBitsOfTheGame("creature_novelty.txt", {"turns", "first", "novelty"},
	                       [](const std::vector<uint32_t>& v) { return Bits(creature_planner::Novelty(v[0], v[1] != 0)); });
}

// The speed that a route asks for has the bits of the game, also for a share that is not above 0
TEST(OracleCreatureMaths, TheTargetSpeedHasTheBitsOfTheGame)
{
	ExpectTheBitsOfTheGame("creature_target_speed.txt", {"fraction", "run", "speed"}, [](const std::vector<uint32_t>& v) {
		return Bits(creature_locomotion::TargetSpeed(F(v[0]), F(v[1])));
	});
}

// An alignment change that the alignment damps has the bits of the game
TEST(OracleCreatureMaths, TheDampedAlignmentChangeHasTheBitsOfTheGame)
{
	ExpectTheBitsOfTheGame("creature_alignment_damp.txt", {"change", "alignment", "damped"},
	                       [](const std::vector<uint32_t>& v) { return Bits(magic::DampAlignmentChange(F(v[0]), F(v[1]))); });
}

// The damage of a blow has the bits of the game
TEST(OracleCreatureMaths, TheBlowDamageHasTheBitsOfTheGame)
{
	ExpectTheBitsOfTheGame(
	    "creature_blow_damage.txt",
	    {"strength", "size", "speed", "other_strength", "other_size", "special", "share", "blocked", "damage"},
	    [](const std::vector<uint32_t>& v) {
		    const creature_fight::Blow blow {.attackerSize = F(v[1]),
		                                     .defenderSize = F(v[4]),
		                                     .attackerStrength = F(v[0]),
		                                     .defenderStrength = F(v[3]),
		                                     .speed = F(v[2]),
		                                     .special = v[5] != 0,
		                                     .heightShare = F(v[6]),
		                                     .blocked = v[7] != 0};
		    return Bits(creature_fight::ResolveBlow(blow, creature_fight::RecoilDirection::Plain).damage);
	    });
}

// The height band of a blow is the band of the game: 0 high, 1 middle and 2 low
TEST(OracleCreatureMaths, TheBlowBandIsTheBandOfTheGame)
{
	ExpectTheBitsOfTheGame("creature_blow_damage.txt", {"share", "band"}, [](const std::vector<uint32_t>& v) {
		switch (creature_fight::BandOf(F(v[0])))
		{
		case creature_fight::Band::High:
			return 0u;
		case creature_fight::Band::Mid:
			return 1u;
		case creature_fight::Band::Low:
			return 2u;
		}
		return 3u;
	});
}

// The priority of a thing by its distance has the bits of the game
TEST(OracleCreatureMaths, TheDistancePriorityHasTheBitsOfTheGame)
{
	ExpectTheBitsOfTheGame(
	    "creature_distance_priority.txt", {"distance", "weight", "priority"},
	    [](const std::vector<uint32_t>& v) { return Bits(creature_planner::DistancePriority(false, F(v[0]), F(v[1]))); });
}

// The attitude to the player after a feedback has the bits of the game
TEST(OracleCreatureMaths, TheAttitudeAfterAFeedbackHasTheBitsOfTheGame)
{
	ExpectTheBitsOfTheGame(
	    "creature_feedback_attitude.txt", {"attitude", "feedback", "new_attitude"},
	    [](const std::vector<uint32_t>& v) { return Bits(creature_feedback::AttitudeAfter(F(v[0]), F(v[1]))); });
}

// The running average of the feedback has the bits of the game
TEST(OracleCreatureMaths, TheAverageFeedbackHasTheBitsOfTheGame)
{
	ExpectTheBitsOfTheGame(
	    "creature_feedback_attitude.txt", {"average", "feedback", "new_average"},
	    [](const std::vector<uint32_t>& v) { return Bits(creature_feedback::AverageAfter(F(v[0]), F(v[1]))); });
}

// The worth of a desire about a thing has the bits of the game
TEST(OracleCreatureMaths, TheReactionWorthHasTheBitsOfTheGame)
{
	ExpectTheBitsOfTheGame(
	    "creature_reaction_worth.txt", {"value", "usefulness", "nearness", "worth"},
	    [](const std::vector<uint32_t>& v) { return Bits(creature_reaction::WorthAbout(F(v[0]), F(v[1]), F(v[2]))); });
}
