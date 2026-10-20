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

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Camera/PushScroll.h"
#include "OracleData.h"

using namespace openblack;

namespace
{
[[nodiscard]] int Int(const oracle::Table& table, size_t row, size_t column)
{
	return static_cast<int>(table.Bits(row, column));
}

[[nodiscard]] bool Same(float ours, float game)
{
	return std::bit_cast<uint32_t>(ours) == std::bit_cast<uint32_t>(game);
}
} // namespace

// A frame's push adds to the camera's speed exactly as the game's key handling does on its default screen
TEST(OraclePushScroll, PushAsInTheGame)
{
	namespace push = camera_push_scroll;
	const auto table = oracle::Table::Load("push_scroll_push.txt");
	ASSERT_TRUE(table.has_value());
	const auto ms = table->Column("ms");
	const auto dx = table->Column("dx");
	const auto dy = table->Column("dy");
	const auto x = table->Column("x");
	const auto y = table->Column("y");
	const auto bx = table->Column("beyond_x");
	const auto by = table->Column("beyond_y");
	const auto camera = table->Column("camera_height");
	const auto focus = table->Column("focus_height");
	const auto sx = table->Column("speed_x");
	const auto sy = table->Column("speed_y");
	const auto sz = table->Column("speed_z");
	const auto ox = table->Column("out_x");
	const auto oy = table->Column("out_y");
	const auto oz = table->Column("out_z");
	ASSERT_TRUE(ms && dx && dy && x && y && bx && by && camera && focus && sx && sy && sz && ox && oy && oz);
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	size_t changed = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const push::Pointer pointer {
		    .beyond = {Int(*table, row, *bx), Int(*table, row, *by)},
		    .position = {Int(*table, row, *x), Int(*table, row, *y)},
		    .delta = {Int(*table, row, *dx), Int(*table, row, *dy)},
		};
		glm::vec3 speed {table->Float(row, *sx), table->Float(row, *sy), table->Float(row, *sz)};
		const glm::vec3 before = speed;
		push::Push(speed, pointer, push::Speed(table->Float(row, *camera), table->Float(row, *focus)),
		           push::Seconds(table->Bits(row, *ms)));
		const glm::vec3 game {table->Float(row, *ox), table->Float(row, *oy), table->Float(row, *oz)};
		if (game != before)
		{
			++changed;
		}
		if (!Same(speed.x, game.x) || !Same(speed.y, game.y) || !Same(speed.z, game.z))
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives " << oracle::Hex(game.x) << " " << oracle::Hex(game.y)
				              << " " << oracle::Hex(game.z) << ", ours gives " << oracle::Hex(speed.x) << " "
				              << oracle::Hex(speed.y) << " " << oracle::Hex(speed.z);
			}
		}
	}
	EXPECT_EQ(different, 0u);
	// The cases push in many of the rows, not only leave the speed as it was
	EXPECT_GT(changed, table->Rows() / 4);
}

// The speed moves the camera, its focus and its turning point, and turns the heading, exactly as the game's strafe does
TEST(OraclePushScroll, StrafeAsInTheGame)
{
	namespace push = camera_push_scroll;
	const auto table = oracle::Table::Load("push_scroll_strafe.txt");
	ASSERT_TRUE(table.has_value());
	const auto column = [&table](const char* name) {
		const auto found = table->Column(name);
		EXPECT_TRUE(found.has_value()) << name;
		return found.value_or(0);
	};
	const auto sx = column("speed_x");
	const auto sy = column("speed_y");
	const auto sz = column("speed_z");
	const auto seconds = column("seconds");
	const auto heading = column("heading");
	const auto pitch = column("pitch");
	const auto features = column("features");
	const auto start = column("start_x");
	const auto pos = column("pos_x");
	const auto focus = column("focus_x");
	const auto turnPoint = column("turn_x");
	const auto outHeading = column("out_heading");
	const auto outSpeed = column("out_speed_x");
	const auto turned = column("turned");
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		glm::vec3 speed {table->Float(row, sx), table->Float(row, sy), table->Float(row, sz)};
		const auto strafe = push::StrafeStep(speed, table->Float(row, seconds), table->Float(row, heading),
		                                     table->Float(row, pitch), table->Bits(row, features));
		bool same = true;
		for (size_t axis = 0; axis < 3; ++axis)
		{
			const float moved = table->Float(row, start + axis) + strafe.move[static_cast<glm::length_t>(axis)];
			same = same && Same(moved, table->Float(row, pos + axis)) && Same(moved, table->Float(row, focus + axis)) &&
			       Same(moved, table->Float(row, turnPoint + axis)) &&
			       Same(speed[static_cast<glm::length_t>(axis)], table->Float(row, outSpeed + axis));
		}
		same = same && Same(table->Float(row, heading) + strafe.turn, table->Float(row, outHeading)) &&
		       strafe.turned == (table->Bits(row, turned) != 0);
		if (!same)
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": game pos " << oracle::Hex(table->Float(row, pos)) << " "
				              << oracle::Hex(table->Float(row, pos + 2)) << " heading "
				              << oracle::Hex(table->Float(row, outHeading)) << ", ours move " << oracle::Hex(strafe.move.x)
				              << " " << oracle::Hex(strafe.move.z) << " turn " << oracle::Hex(strafe.turn);
			}
		}
	}
	EXPECT_EQ(different, 0u);
}
