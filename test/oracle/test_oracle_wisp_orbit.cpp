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
#include <string_view>

#include <gtest/gtest.h>

#include "OracleData.h"
#include "Particles/CreatureSpellMaths.h"

using namespace openblack;
using namespace openblack::particles;

// One step of one wisp: the new angles, the place between the hand and the orbit, the scale and the opacity
TEST(OracleWispOrbit, AStepOfAWispGivesTheGameBits)
{
	const auto table = oracle::Table::Load("wisp_orbit.txt");
	ASSERT_TRUE(table.has_value());
	constexpr std::array<std::string_view, 24> k_Names {
	    "centre_x", "centre_z",  "bottom",     "height",   "radius", "hand_x", "hand_y", "hand_z",
	    "theta",    "phi",       "theta_rate", "phi_rate", "dt",     "age",    "taper",  "base_scale",
	    "size",     "out_theta", "out_phi",    "x",        "y",      "z",      "scale",  "alpha"};
	std::array<size_t, k_Names.size()> column {};
	for (size_t i = 0; i < k_Names.size(); ++i)
	{
		const auto found = table->Column(k_Names.at(i));
		ASSERT_TRUE(found.has_value()) << k_Names.at(i);
		column.at(i) = *found;
	}
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto in = [&](size_t i) { return table->Float(row, column.at(i)); };
		const maths::WispBox box {.centreX = in(0), .centreZ = in(1), .bottom = in(2), .height = in(3), .radius = in(4)};
		const glm::vec3 hand(in(5), in(6), in(7));
		const float dt = in(12);
		const float age = in(13);
		const float theta = maths::WispTurn(in(8), in(10), dt);
		const float phi = maths::WispTurn(in(9), in(11), dt);
		const auto orbit = maths::WispOrbit(box, theta, phi, in(14), age);
		const auto place = maths::WispPosition(hand, orbit, age);
		const float scale = in(15) * in(16);
		const std::array<uint32_t, 7> ours {std::bit_cast<uint32_t>(theta),
		                                    std::bit_cast<uint32_t>(phi),
		                                    std::bit_cast<uint32_t>(place.x),
		                                    std::bit_cast<uint32_t>(place.y),
		                                    std::bit_cast<uint32_t>(place.z),
		                                    std::bit_cast<uint32_t>(scale),
		                                    maths::WispAlpha(age)};
		for (size_t i = 0; i < ours.size(); ++i)
		{
			const auto game = table->Bits(row, column.at(17 + i));
			if (ours.at(i) != game)
			{
				++different;
				if (different <= 20)
				{
					ADD_FAILURE() << "row " << row << " " << k_Names.at(17 + i) << ": the game gives " << std::hex << game
					              << ", ours gives " << ours.at(i);
				}
			}
		}
	}
	EXPECT_EQ(different, 0u);
}
