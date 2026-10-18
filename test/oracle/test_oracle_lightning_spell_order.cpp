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
#include <vector>

#include <gtest/gtest.h>

#include "Common/X87Vector.h"
#include "OracleData.h"
#include "Particles/LightningMaths.h"

using namespace openblack;
using namespace openblack::particles;

namespace
{
template <size_t N>
std::array<size_t, N> Columns(const oracle::Table& table, const std::array<std::string_view, N>& names)
{
	std::array<size_t, N> column {};
	for (size_t i = 0; i < N; ++i)
	{
		const auto found = table.Column(names.at(i));
		EXPECT_TRUE(found.has_value()) << names.at(i);
		column.at(i) = found.value_or(0);
	}
	return column;
}
} // namespace

// A newer bolt from a hand clashes with an older one: whether they meet, and where
TEST(OracleLightningSpellOrder, AClashGivesTheGameBits)
{
	const auto table = oracle::Table::Load("lightning_clash.txt");
	ASSERT_TRUE(table.has_value());
	constexpr std::array<std::string_view, 19> k_Names {"newer_x",  "newer_y",       "newer_z",  "newer_cx",      "newer_cy",
	                                                    "newer_cz", "newer_heading", "older_x",  "older_y",       "older_z",
	                                                    "older_cx", "older_cy",      "older_cz", "older_heading", "reach",
	                                                    "clash",    "meet_x",        "meet_y",   "meet_z"};
	const auto column = Columns(*table, k_Names);
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	size_t clashes = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto in = [&](size_t i) { return table->Float(row, column.at(i)); };
		const maths::BoltPose newer {.origin = {in(0), in(1), in(2)}, .centroid = {in(3), in(4), in(5)}, .heading = in(6)};
		const maths::BoltPose older {.origin = {in(7), in(8), in(9)}, .centroid = {in(10), in(11), in(12)}, .heading = in(13)};
		const auto meeting = maths::ClashPoint(newer, older, in(14));
		const glm::vec3 point = meeting.value_or(glm::vec3(0.0f));
		const std::array<uint32_t, 4> ours {meeting.has_value() ? 1u : 0u, std::bit_cast<uint32_t>(point.x),
		                                    std::bit_cast<uint32_t>(point.y), std::bit_cast<uint32_t>(point.z)};
		clashes += meeting.has_value() ? 1 : 0;
		for (size_t i = 0; i < ours.size(); ++i)
		{
			const auto game = table->Bits(row, column.at(15 + i));
			if (ours.at(i) != game)
			{
				++different;
				if (different <= 20)
				{
					ADD_FAILURE() << "row " << row << " " << k_Names.at(15 + i) << ": the game gives " << std::hex << game
					              << ", ours gives " << ours.at(i);
				}
			}
		}
	}
	EXPECT_EQ(different, 0u);
	EXPECT_GT(clashes, 0u);
}

// The middle of a bolt's targets is the sum of their tips times the inverse of their count, and its distance from the
// origin adds the squares z first
TEST(OracleLightningSpellOrder, TheMiddleOfTheTargetsGivesTheGameBits)
{
	const auto table = oracle::Table::Load("lightning_middle.txt");
	ASSERT_TRUE(table.has_value());
	constexpr std::array<std::string_view, 11> k_Names {"sum_x",    "sum_y", "sum_z", "count", "origin_x", "origin_y",
	                                                    "origin_z", "mid_x", "mid_y", "mid_z", "distance"};
	const auto column = Columns(*table, k_Names);
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto in = [&](size_t i) { return table->Float(row, column.at(i)); };
		// The sum as the first tip and zeros for the others: adding a zero changes no bit
		std::vector<glm::vec3> tips(table->Bits(row, column.at(3)), glm::vec3(0.0f));
		tips.front() = {in(0), in(1), in(2)};
		const glm::vec3 origin(in(4), in(5), in(6));
		const auto middle = maths::TipsMiddle(tips);
		const float distance = x87::LengthZyx(middle - origin);
		const std::array<uint32_t, 4> ours {std::bit_cast<uint32_t>(middle.x), std::bit_cast<uint32_t>(middle.y),
		                                    std::bit_cast<uint32_t>(middle.z), std::bit_cast<uint32_t>(distance)};
		for (size_t i = 0; i < ours.size(); ++i)
		{
			const auto game = table->Bits(row, column.at(7 + i));
			if (ours.at(i) != game)
			{
				++different;
				if (different <= 20)
				{
					ADD_FAILURE() << "row " << row << " " << k_Names.at(7 + i) << ": the game gives " << std::hex << game
					              << ", ours gives " << ours.at(i);
				}
			}
		}
	}
	EXPECT_EQ(different, 0u);
}
