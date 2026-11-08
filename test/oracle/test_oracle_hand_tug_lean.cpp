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
#include <format>
#include <string>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Common/Zoomer.h"
#include "Hand/HandGrabRules.h"
#include "OracleData.h"

using namespace openblack;
using namespace openblack::hand_grab;

namespace
{

/// The columns of a table row, read by name
struct Row
{
	const oracle::Table& table;
	size_t row;

	[[nodiscard]] float F(const std::string& name) const { return table.Float(row, table.Column(name).value_or(0)); }
	[[nodiscard]] uint32_t U(const std::string& name) const { return table.Bits(row, table.Column(name).value_or(0)); }
	[[nodiscard]] glm::vec3 V(const std::string& prefix, int first) const
	{
		return {F(std::format("{}{}", prefix, first)), F(std::format("{}{}", prefix, first + 1)),
		        F(std::format("{}{}", prefix, first + 2))};
	}
};

/// The data file of the game's frames of a pull on a heavy tree
oracle::Table Frames()
{
	auto table = oracle::Table::Load("hand_tug_lean.txt");
	EXPECT_TRUE(table.has_value());
	return table.value_or(oracle::Table {});
}

/// The text of each part of a vector that differs from the game, or an empty text
std::string Mismatch(const std::string& name, const glm::vec3& ours, const glm::vec3& game)
{
	std::string text;
	for (glm::length_t part = 0; part < 3; ++part)
	{
		const auto a = std::bit_cast<uint32_t>(ours[part]);
		const auto b = std::bit_cast<uint32_t>(game[part]);
		if (a != b)
		{
			text += std::format(" {}[{}]: ours {:08x} game {:08x}", name, part, a, b);
		}
	}
	return text;
}

} // namespace

// Each row is one frame of the game's pull on a heavy tree. The lean, the spin, the speed of the middle, the stretch and
// the hand point on the tree must have the same bits as the game. A new pull starts where the axes of a row are not
// the axes that the row before it gave.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(OracleHandTugLean, TheLeanMatchesTheGame)
{
	const auto table = Frames();
	ASSERT_GT(table.Rows(), 0u);
	int failures = 0;
	Zoomer stretch {1.0f};
	for (size_t index = 0; index < table.Rows(); ++index)
	{
		const Row row {table, index};
		if (index == 0 || row.V("m", 0) != Row {table, index - 1}.V("om", 0) ||
		    row.V("m", 3) != Row {table, index - 1}.V("om", 3))
		{
			stretch.Reset(1.0f);
		}
		Tug tug {.axes = glm::mat3(row.V("m", 0), row.V("m", 3), row.V("m", 6)), .base = row.V("m", 9), .spin = row.V("s", 0)};
		const float grip = row.F("grip");
		const auto result = PullAt(tug, {.hand = {row.F("hx"), row.F("hy"), row.F("hz")},
		                                 .holdDistance = grip,
		                                 .maxForce = MaxForce(0.0f),
		                                 .weight = 1.0e6f,
		                                 .height = row.F("height"),
		                                 .tree = true,
		                                 .leans = row.U("rock") == 0,
		                                 .seconds = row.F("dt")});
		EXPECT_FALSE(result.comesFree) << "row " << index;
		stretch.SetDestination(result.stretchTarget, k_TugStretchSeconds);
		stretch.UpdateInlineValue(row.F("dt"));
		const auto hand = TugHandPoint(tug, grip, stretch.GetValue());
		const auto mismatch = Mismatch("row0", tug.axes[0], row.V("om", 0)) + Mismatch("row1", tug.axes[1], row.V("om", 3)) +
		                      Mismatch("row2", tug.axes[2], row.V("om", 6)) + Mismatch("base", tug.base, row.V("om", 9)) +
		                      Mismatch("spin", tug.spin, row.V("os", 0)) + Mismatch("speed", tug.pullVelocity, row.V("ov", 0)) +
		                      Mismatch("hand", hand, row.V("oh", 0)) +
		                      Mismatch("stretch", glm::vec3(stretch.GetValue()), glm::vec3(row.F("ostretch")));
		if (!mismatch.empty() && ++failures <= 5)
		{
			ADD_FAILURE() << "row " << index << mismatch;
		}
	}
	EXPECT_EQ(failures, 0);
}
