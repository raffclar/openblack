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
#include <string_view>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureSway.h"
#include "OracleData.h"
#include "Particles/ParticleDrawFrame.h"
#include "Particles/ParticleSpellLink.h"

using namespace openblack;

namespace
{
std::vector<size_t> Columns(const oracle::Table& table, const std::vector<std::string>& names)
{
	std::vector<size_t> column;
	for (const auto& name : names)
	{
		const auto found = table.Column(name);
		EXPECT_TRUE(found.has_value()) << name;
		column.push_back(found.value_or(0));
	}
	return column;
}

std::vector<std::string> Names(std::initializer_list<std::string_view> names)
{
	return {names.begin(), names.end()};
}

/// Compares our bits with the game's bits of a row, from a first column on, and counts the differences
size_t Compare(const oracle::Table& table, const std::vector<size_t>& column, const std::vector<std::string>& names, size_t row,
               size_t first, const std::vector<uint32_t>& ours, size_t& shown)
{
	size_t different = 0;
	for (size_t i = 0; i < ours.size(); ++i)
	{
		const auto game = table.Bits(row, column.at(first + i));
		if (ours.at(i) != game)
		{
			++different;
			if (++shown <= 20)
			{
				ADD_FAILURE() << "row " << row << " " << names.at(first + i) << ": the game gives " << std::hex << game
				              << ", ours gives " << ours.at(i);
			}
		}
	}
	return different;
}

void Append(std::vector<uint32_t>& out, const glm::vec3& v)
{
	out.push_back(std::bit_cast<uint32_t>(v.x));
	out.push_back(std::bit_cast<uint32_t>(v.y));
	out.push_back(std::bit_cast<uint32_t>(v.z));
}
} // namespace

// One game turn of the spring on the two parts of the body
TEST(OracleSwayRibbon, ATurnOfTheSwaySpringGivesTheGameBits)
{
	const auto table = oracle::Table::Load("creature_sway_spring.txt");
	ASSERT_TRUE(table.has_value());
	const auto names = Names({"seconds",      "mass",         "lower_vx",
	                          "lower_vy",     "lower_vz",     "upper_vx",
	                          "upper_vy",     "upper_vz",     "lower_ox",
	                          "lower_oy",     "lower_oz",     "upper_ox",
	                          "upper_oy",     "upper_oz",     "out_lower_vx",
	                          "out_lower_vy", "out_lower_vz", "out_upper_vx",
	                          "out_upper_vy", "out_upper_vz", "out_lower_ox",
	                          "out_lower_oy", "out_lower_oz", "out_upper_ox",
	                          "out_upper_oy", "out_upper_oz", "on"});
	const auto column = Columns(*table, names);
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	size_t shown = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto in = [&](size_t i) { return table->Float(row, column.at(i)); };
		creature_sway::Sway sway;
		sway.active = true;
		sway.lowerVelocity = {in(2), in(3), in(4)};
		sway.upperVelocity = {in(5), in(6), in(7)};
		sway.lowerOffset = {in(8), in(9), in(10)};
		sway.upperOffset = {in(11), in(12), in(13)};
		creature_sway::Step(sway, in(0), in(1));
		std::vector<uint32_t> ours;
		Append(ours, sway.lowerVelocity);
		Append(ours, sway.upperVelocity);
		Append(ours, sway.lowerOffset);
		Append(ours, sway.upperOffset);
		ours.push_back(sway.active ? 1u : 0u);
		different += Compare(*table, column, names, row, 14, ours, shown);
	}
	EXPECT_EQ(different, 0u);
}

// A kick by a force at a point: the upper or lower body, the speed across the land and its limit
TEST(OracleSwayRibbon, AKickOfTheSwayGivesTheGameBits)
{
	const auto table = oracle::Table::Load("creature_sway_kick.txt");
	ASSERT_TRUE(table.has_value());
	const auto names =
	    Names({"seconds",      "mass",         "size",         "point_y",      "land",         "force_x",      "force_y",
	           "force_z",      "lower_vx",     "lower_vy",     "lower_vz",     "upper_vx",     "upper_vy",     "upper_vz",
	           "out_lower_vx", "out_lower_vy", "out_lower_vz", "out_upper_vx", "out_upper_vy", "out_upper_vz", "on"});
	const auto column = Columns(*table, names);
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	size_t shown = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto in = [&](size_t i) { return table->Float(row, column.at(i)); };
		creature_sway::Sway sway;
		sway.turnSeconds = in(0);
		sway.lowerVelocity = {in(8), in(9), in(10)};
		sway.upperVelocity = {in(11), in(12), in(13)};
		const float heightAboveLand = in(3) - in(4);
		const float splitHeight = in(2) * creature_sway::k_UpperHeightPerSize;
		creature_sway::Kick(sway, {in(5), in(6), in(7)}, heightAboveLand, splitHeight, in(1));
		std::vector<uint32_t> ours;
		Append(ours, sway.lowerVelocity);
		Append(ours, sway.upperVelocity);
		ours.push_back(sway.active ? 1u : 0u);
		different += Compare(*table, column, names, row, 14, ours, shown);
	}
	EXPECT_EQ(different, 0u);
}

// The corners of a ribbon of four joints as seen from an eye
TEST(OracleSwayRibbon, RibbonCornersGiveTheGameBits)
{
	const auto table = oracle::Table::Load("ribbon_corners.txt");
	ASSERT_TRUE(table.has_value());
	std::vector<std::string> names;
	for (int j = 0; j < 4; ++j)
	{
		for (const char axis : {'x', 'y', 'z'})
		{
			names.push_back("joint" + std::to_string(j) + "_" + axis);
		}
	}
	for (int j = 0; j < 4; ++j)
	{
		names.push_back("width" + std::to_string(j));
	}
	for (const char* eye : {"eye_x", "eye_y", "eye_z"})
	{
		names.emplace_back(eye);
	}
	for (int c = 0; c < 12; ++c)
	{
		for (const char axis : {'x', 'y', 'z'})
		{
			names.push_back("corner" + std::to_string(c) + "_" + axis);
		}
	}
	const auto column = Columns(*table, names);
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	size_t shown = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto in = [&](size_t i) { return table->Float(row, column.at(i)); };
		std::array<particles::draw::ChainJoint, 4> joints {};
		for (size_t j = 0; j < joints.size(); ++j)
		{
			joints.at(j) = {.position = {in(3 * j), in((3 * j) + 1), in((3 * j) + 2)}, .halfWidth = in(12 + j)};
		}
		std::array<particles::draw::ChainVertex, 12> corners {};
		particles::draw::PlaceChainCorners(corners, joints, {in(16), in(17), in(18)});
		std::vector<uint32_t> ours;
		for (const auto& corner : corners)
		{
			Append(ours, corner.position);
		}
		different += Compare(*table, column, names, row, 19, ours, shown);
	}
	EXPECT_EQ(different, 0u);
}

// The heading from a creature's hands to the point it casts at
TEST(OracleSwayRibbon, ACreatureCastHeadingGivesTheGameBits)
{
	const auto table = oracle::Table::Load("cast_heading.txt");
	ASSERT_TRUE(table.has_value());
	const auto names =
	    Names({"target_x", "target_y", "target_z", "hands_x", "hands_y", "hands_z", "heading_x", "heading_y", "heading_z"});
	const auto column = Columns(*table, names);
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	size_t shown = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto in = [&](size_t i) { return table->Float(row, column.at(i)); };
		std::vector<uint32_t> ours;
		Append(ours, particles::CastHeading({in(3), in(4), in(5)}, {in(0), in(1), in(2)}));
		different += Compare(*table, column, names, row, 6, ours, shown);
	}
	EXPECT_EQ(different, 0u);
}

// The heading that an effect keeps: across the land only, else along x
TEST(OracleSwayRibbon, AnEffectsFlatHeadingGivesTheGameBits)
{
	const auto table = oracle::Table::Load("flat_heading.txt");
	ASSERT_TRUE(table.has_value());
	const auto names = Names({"x", "y", "z", "flat_x", "flat_y", "flat_z"});
	const auto column = Columns(*table, names);
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	size_t shown = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto in = [&](size_t i) { return table->Float(row, column.at(i)); };
		std::vector<uint32_t> ours;
		Append(ours, particles::FlatHeading({in(0), in(1), in(2)}));
		different += Compare(*table, column, names, row, 3, ours, shown);
	}
	EXPECT_EQ(different, 0u);
}
