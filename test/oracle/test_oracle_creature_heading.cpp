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

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "Common/GameHeading.h"
#include "Creature/CreatureCastMoves.h"
#include "Creature/CreatureLocomotion.h"
#include "OracleData.h"

using namespace openblack;

// The game moves an angle one full turn at most to bring it into the half turns each side of 0
TEST(OracleCreatureHeading, WrapAsInTheGame)
{
	const auto table = oracle::Table::Load("heading_wrap.txt");
	ASSERT_TRUE(table.has_value());
	const auto angle = table->Column("angle");
	const auto wrapped = table->Column("wrapped");
	ASSERT_TRUE(angle && wrapped);
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const float ours = GameHeading::Wrap(table->Float(row, *angle));
		const float game = table->Float(row, *wrapped);
		if (std::bit_cast<uint32_t>(ours) != std::bit_cast<uint32_t>(game))
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives " << oracle::Hex(game) << ", ours gives "
				              << oracle::Hex(ours);
			}
		}
	}
	EXPECT_EQ(different, 0u);
}

// A creature faces a thing when the heading from its map position to the thing's is near its own heading, or when it
// stands on top of the thing
TEST(OracleCreatureHeading, FacingAsInTheGame)
{
	constexpr uint32_t k_GameFaces = 2;
	const auto table = oracle::Table::Load("creature_facing.txt");
	ASSERT_TRUE(table.has_value());
	const auto cx = table->Column("creature_x");
	const auto cz = table->Column("creature_z");
	const auto px = table->Column("point_x");
	const auto pz = table->Column("point_z");
	const auto heading = table->Column("heading");
	const auto result = table->Column("result");
	ASSERT_TRUE(cx && cz && px && pz && heading && result);
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const glm::vec2 creature {table->Float(row, *cx), table->Float(row, *cz)};
		const glm::vec2 point {table->Float(row, *px), table->Float(row, *pz)};
		const bool ours = creature_cast_moves::Facing(creature, GameHeading(table->Float(row, *heading)), point);
		const bool game = table->Bits(row, *result) == k_GameFaces;
		if (ours != game)
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives " << game << ", ours gives " << ours;
			}
		}
	}
	EXPECT_EQ(different, 0u);
}

// To face a thing, a creature turns to the heading from its map position to the thing's. To turn its back to the thing,
// it turns to that heading plus a half turn. On top of the thing, it does not turn.
TEST(OracleCreatureHeading, TurnHeadingAsInTheGame)
{
	constexpr uint32_t k_NoTurn = 0xFFFFFFFFu;
	const auto table = oracle::Table::Load("creature_turn_heading.txt");
	ASSERT_TRUE(table.has_value());
	const auto stage = table->Column("stage");
	const auto cx = table->Column("creature_x");
	const auto cz = table->Column("creature_z");
	const auto px = table->Column("point_x");
	const auto pz = table->Column("point_z");
	const auto turn = table->Column("turn_heading");
	ASSERT_TRUE(stage && cx && cz && px && pz && turn);
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const glm::vec2 creature {table->Float(row, *cx), table->Float(row, *cz)};
		const glm::vec2 point {table->Float(row, *px), table->Float(row, *pz)};
		uint32_t ours = k_NoTurn;
		if (!(creature_cast_moves::MapPointDistance(point, creature) < creature_cast_moves::k_OnTopDistance))
		{
			const auto heading = table->Bits(row, *stage) == 0 ? creature_cast_moves::HeadingTo(creature, point)
			                                                   : creature_cast_moves::HeadingAwayFrom(creature, point);
			ours = std::bit_cast<uint32_t>(heading.Radians());
		}
		const uint32_t game = table->Bits(row, *turn);
		if (ours != game)
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives " << std::hex << game << ", ours gives " << ours;
			}
		}
	}
	EXPECT_EQ(different, 0u);
}

// A turn on the spot keeps its target as given, wraps the turn one time, takes the spins to the right for a turn of 0 or
// less, and blends the two spins of the side by the share of a quarter turn
TEST(OracleCreatureHeading, TurnStartAsInTheGame)
{
	constexpr uint32_t k_GameRightSpins = 2;
	const auto table = oracle::Table::Load("creature_turn_start.txt");
	ASSERT_TRUE(table.has_value());
	const auto heading = table->Column("heading");
	const auto target = table->Column("target");
	const auto kept = table->Column("target_kept");
	const auto side = table->Column("side");
	const auto weight = table->Column("weight");
	ASSERT_TRUE(heading && target && kept && side && weight);
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const GameHeading from(table->Float(row, *heading));
		const GameHeading to(table->Float(row, *target));
		const float angle = to.TurnFrom(from);
		const bool right = creature_locomotion::SpinSideOf(angle) == creature_locomotion::Side::Right;
		const auto pair = creature_locomotion::SpinPairFor(creature_locomotion::animations::k_RightSpin, angle);
		const bool same = std::bit_cast<uint32_t>(to.Radians()) == table->Bits(row, *kept) &&
		                  right == (table->Bits(row, *side) == k_GameRightSpins) &&
		                  std::bit_cast<uint32_t>(pair.weight) == table->Bits(row, *weight);
		if (!same)
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives side " << table->Bits(row, *side) << " weight " << std::hex
				              << table->Bits(row, *weight) << ", ours gives right " << right << " weight "
				              << std::bit_cast<uint32_t>(pair.weight);
			}
		}
	}
	EXPECT_EQ(different, 0u);
}

// A step of a turn without spins closes the part of the gap that the time before the step gives, and the turn ends with
// the target once the time reaches the stand's length
TEST(OracleCreatureHeading, TurnEaseAsInTheGame)
{
	const auto table = oracle::Table::Load("creature_turn_ease.txt");
	ASSERT_TRUE(table.has_value());
	const auto heading = table->Column("heading");
	const auto target = table->Column("target");
	const auto time = table->Column("time");
	const auto length = table->Column("length");
	const auto step = table->Column("step");
	const auto after = table->Column("heading_after");
	const auto ended = table->Column("ended");
	ASSERT_TRUE(heading && target && time && length && step && after && ended);
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const GameHeading to(table->Float(row, *target));
		const auto timeMs = static_cast<float>(table->Bits(row, *time));
		const auto lengthMs = static_cast<float>(table->Bits(row, *length));
		const auto stepMs = static_cast<float>(table->Bits(row, *step));
		auto ours = creature_locomotion::LerpHeading(GameHeading(table->Float(row, *heading)), to, timeMs, lengthMs);
		const bool end = timeMs + stepMs >= lengthMs;
		if (end)
		{
			ours = to;
		}
		const bool same =
		    std::bit_cast<uint32_t>(ours.Radians()) == table->Bits(row, *after) && end == (table->Bits(row, *ended) == 1);
		if (!same)
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives " << std::hex << table->Bits(row, *after) << ", ours gives "
				              << std::bit_cast<uint32_t>(ours.Radians());
			}
		}
	}
	EXPECT_EQ(different, 0u);
}
