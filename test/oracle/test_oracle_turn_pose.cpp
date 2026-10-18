/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <bit>
#include <string>

#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureTurnPose.h"
#include "OracleData.h"

using namespace openblack;

namespace
{
constexpr size_t k_Numbers = 12;

/// The place of each of the twelve numbers of a bone in a matrix: the three axes, then the place
constexpr std::array<std::pair<glm::length_t, glm::length_t>, k_Numbers> k_Cells {{
    {0, 0},
    {0, 1},
    {0, 2},
    {1, 0},
    {1, 1},
    {1, 2},
    {2, 0},
    {2, 1},
    {2, 2},
    {3, 0},
    {3, 1},
    {3, 2},
}};
} // namespace

// One bone of the drawn body between the poses of the last two game turns, as the game blends it
TEST(OracleTurnPose, TheDrawnBoneGivesTheGameBits)
{
	const auto table = oracle::Table::Load("turn_pose_blend.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 0u);
	const auto column = [&table](const std::string& name) {
		const auto found = table->Column(name);
		EXPECT_TRUE(found.has_value()) << name;
		return found.value_or(0);
	};
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto weight = table->Float(row, column("weight"));
		std::array<glm::mat4, 1> older {glm::mat4(1.0f)};
		std::array<glm::mat4, 1> newer {glm::mat4(1.0f)};
		for (size_t i = 0; i < k_Numbers; ++i)
		{
			const auto [col, line] = k_Cells.at(i);
			older[0][col][line] = table->Float(row, column("older" + std::to_string(i)));
			newer[0][col][line] = table->Float(row, column("newer" + std::to_string(i)));
		}
		std::array<glm::mat4, 1> drawn {};
		creature_turn_pose::Blend(older, newer, weight, drawn);
		for (size_t i = 0; i < k_Numbers; ++i)
		{
			const auto [col, line] = k_Cells.at(i);
			const auto game = table->Bits(row, column("drawn" + std::to_string(i)));
			const auto ours = std::bit_cast<uint32_t>(drawn[0][col][line]);
			const auto one =
			    std::bit_cast<uint32_t>(creature_turn_pose::BlendNumber(older[0][col][line], newer[0][col][line], weight));
			if (ours != game || one != game)
			{
				++different;
				ADD_FAILURE() << "row " << row << " number " << i << ": the game gives " << std::hex << game << ", ours gives "
				              << ours << " and " << one;
			}
		}
	}
	EXPECT_EQ(different, 0u);
}
