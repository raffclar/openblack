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

#include "ECS/ComputerHand.h"
#include "ECS/ComputerHandSteps.h"
#include "OracleData.h"

#include "../ComputerHandFake.h"

using namespace openblack;
using namespace openblack::computer_hand;

namespace
{

[[nodiscard]] bool SameBits(float a, float b)
{
	return std::bit_cast<uint32_t>(a) == std::bit_cast<uint32_t>(b);
}

} // namespace

// A turn of the hand that moves near a place or a thing: the hand moves to the side when it is too near, else toward
// the target, and the step ends when the hand is near enough
TEST(OracleComputerHandMoves, NearMovesAsInTheGame)
{
	const auto table = oracle::Table::Load("computer_hand_near.txt");
	ASSERT_TRUE(table.has_value());
	const auto kind = table->Column("kind");
	const auto hx = table->Column("hand_x");
	const auto hy = table->Column("hand_y");
	const auto hz = table->Column("hand_z");
	const auto tx = table->Column("target_x");
	const auto ty = table->Column("target_y");
	const auto tz = table->Column("target_z");
	const auto speed = table->Column("speed");
	const auto outX = table->Column("out_x");
	const auto outZ = table->Column("out_z");
	const auto outTargetY = table->Column("out_target_y");
	const auto result = table->Column("result");
	ASSERT_TRUE(kind && hx && hy && hz && tx && ty && tz && speed && outX && outZ && outTargetY && result);
	ASSERT_GT(table->Rows(), 0u);
	FakeWorld world;
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto step = table->Bits(row, *kind) == 4 ? StepKind::MoveHandNearPos : StepKind::MoveHandNearObject;
		HandAction action;
		action.hand = {table->Float(row, *hx), table->Float(row, *hy), table->Float(row, *hz)};
		action.target = {table->Float(row, *tx), table->Float(row, *ty), table->Float(row, *tz)};
		action.speed = table->Float(row, *speed);
		const auto ours = steps::Table().at(static_cast<size_t>(step)).process(action, world.view, Step {});
		const auto game = table->Bits(row, *result) == 0 ? steps::Result::Next : steps::Result::Continue;
		if (ours != game || !SameBits(action.hand.x, table->Float(row, *outX)) ||
		    !SameBits(action.hand.z, table->Float(row, *outZ)) || !SameBits(action.target.y, table->Float(row, *outTargetY)))
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives " << oracle::Hex(table->Float(row, *outX)) << " "
				              << oracle::Hex(table->Float(row, *outZ)) << ", ours gives " << oracle::Hex(action.hand.x) << " "
				              << oracle::Hex(action.hand.z);
			}
		}
	}
	EXPECT_EQ(different, 0u);
}

// The speed at which the hand throws what it holds so that it falls at a place
TEST(OracleComputerHandMoves, ThrowSpeedAsInTheGame)
{
	const auto table = oracle::Table::Load("computer_hand_throw.txt");
	ASSERT_TRUE(table.has_value());
	const auto hx = table->Column("hand_x");
	const auto hy = table->Column("hand_y");
	const auto hz = table->Column("hand_z");
	const auto px = table->Column("place_x");
	const auto py = table->Column("place_y");
	const auto pz = table->Column("place_z");
	const auto vx = table->Column("velocity_x");
	const auto vy = table->Column("velocity_y");
	const auto vz = table->Column("velocity_z");
	ASSERT_TRUE(hx && hy && hz && px && py && pz && vx && vy && vz);
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const glm::vec3 hand {table->Float(row, *hx), table->Float(row, *hy), table->Float(row, *hz)};
		const glm::vec3 place {table->Float(row, *px), table->Float(row, *py), table->Float(row, *pz)};
		const auto ours = ThrowVelocity(hand, place);
		if (!SameBits(ours.x, table->Float(row, *vx)) || !SameBits(ours.y, table->Float(row, *vy)) ||
		    !SameBits(ours.z, table->Float(row, *vz)))
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives " << oracle::Hex(table->Float(row, *vy)) << ", ours gives "
				              << oracle::Hex(ours.y);
			}
		}
	}
	EXPECT_EQ(different, 0u);
}
