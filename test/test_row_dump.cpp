/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#if defined(OPENBLACK_DEVELOPER_TOOLS)

#include <cstdint>

#include <array>
#include <sstream>
#include <string>
#include <utility>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "Debug/RowDump.h"

using namespace openblack;

namespace
{

/// The rows of a turn: one entity with one field, of which the value is the turn
ecs::reflection::WorldRows RowsOf(uint32_t turn)
{
	ecs::reflection::WorldRows rows;
	rows.fields = {"Transform.position"};
	rows.bytes = {static_cast<uint8_t>(turn), 0xab};
	rows.rows.push_back({.entity = static_cast<entt::entity>(7), .field = 0, .offset = 0, .size = 2});
	return rows;
}

} // namespace

TEST(RowDump, APartWritesTheRowsOfItsTurn)
{
	std::ostringstream out;
	row_dump::RowDump dump(out, {.keptTurns = 4, .atTurn = std::nullopt, .mostWrites = 8});
	for (uint32_t turn = 1; turn <= 6; ++turn)
	{
		dump.Keep(turn, RowsOf(turn));
	}
	EXPECT_TRUE(dump.Part(5, "slot 0 parts"));
	EXPECT_EQ(out.str(), "turn 5 slot 0 parts\nTransform.position 7 05ab\n");
}

TEST(RowDump, ATurnThatTheDumpDoesNotKeepWritesANote)
{
	std::ostringstream out;
	row_dump::RowDump dump(out, {.keptTurns = 2, .atTurn = std::nullopt, .mostWrites = 8});
	for (uint32_t turn = 1; turn <= 6; ++turn)
	{
		dump.Keep(turn, RowsOf(turn));
	}
	EXPECT_FALSE(dump.Part(3, "parts"));
	EXPECT_EQ(out.str(), "turn 3 parts (the rows of the turn are not kept)\n");
	EXPECT_EQ(dump.Writes(), 0u);
}

TEST(RowDump, APartThatContinuesWritesNothing)
{
	std::ostringstream out;
	row_dump::RowDump dump(out, {.keptTurns = 8, .atTurn = std::nullopt, .mostWrites = 8});
	for (uint32_t turn = 1; turn <= 8; ++turn)
	{
		dump.Keep(turn, RowsOf(turn));
	}
	EXPECT_TRUE(dump.Part(3, "parts"));
	EXPECT_FALSE(dump.Part(4, "parts"));
	// Another source that parts at the next turn starts its own part
	EXPECT_TRUE(dump.Part(4, "slot 1 parts", 1));
	// The same turn writes one time only
	EXPECT_FALSE(dump.Part(4, "slot 2 parts", 2));
	// A new part after the world was the same again
	EXPECT_TRUE(dump.Part(7, "parts"));
	EXPECT_EQ(dump.Writes(), 3u);
}

TEST(RowDump, TheTurnOfTheSettingsWritesAsItEnds)
{
	std::ostringstream out;
	row_dump::RowDump dump(out, {.keptTurns = 0, .atTurn = 2, .mostWrites = 8});
	dump.Keep(1, RowsOf(1));
	dump.Keep(2, RowsOf(2));
	dump.Keep(3, RowsOf(3));
	EXPECT_EQ(out.str(), "turn 2 turn\nTransform.position 7 02ab\n");
	// The dump kept no turn
	EXPECT_FALSE(dump.Part(3, "parts"));
}

TEST(RowDump, AValueOutsideTheComponentsHasTheNullEntity)
{
	auto rows = RowsOf(1);
	const std::array<uint8_t, 2> draws {0x10, 0x20};
	row_dump::AddValue(rows, "GameRandom.draws", draws);
	std::ostringstream out;
	row_dump::RowDump dump(out, {.keptTurns = 0, .atTurn = 1, .mostWrites = 8});
	dump.Keep(1, std::move(rows));
	EXPECT_EQ(out.str(), "turn 1 turn\nTransform.position 7 01ab\nGameRandom.draws 4294967295 1020\n");
}

TEST(RowDump, TheDumpStopsAfterTheMostWrites)
{
	std::ostringstream out;
	row_dump::RowDump dump(out, {.keptTurns = 16, .atTurn = std::nullopt, .mostWrites = 2});
	for (uint32_t turn = 1; turn <= 10; ++turn)
	{
		dump.Keep(turn, RowsOf(turn));
	}
	EXPECT_TRUE(dump.Part(2, "parts"));
	EXPECT_TRUE(dump.Part(5, "parts"));
	EXPECT_FALSE(dump.Part(8, "parts"));
	EXPECT_EQ(dump.Writes(), 2u);
}

#endif
