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
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <glm/mat3x3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureLandTilt.h"
#include "OracleData.h"

using namespace openblack;

namespace
{

constexpr std::array<const char*, 11> k_Inputs {"x", "y", "z", "heading", "scale", "radius", "held", "kx", "kz", "cos", "sin"};

struct Columns
{
	std::array<size_t, k_Inputs.size()> inputs {};
	std::array<size_t, 12> placed {};
};

std::optional<Columns> Find(const oracle::Table& table)
{
	Columns columns;
	for (size_t i = 0; i < k_Inputs.size(); ++i)
	{
		const auto column = table.Column(k_Inputs.at(i));
		if (!column.has_value())
		{
			return std::nullopt;
		}
		columns.inputs.at(i) = *column;
	}
	for (size_t i = 0; i < columns.placed.size(); ++i)
	{
		const auto column = table.Column("p" + std::to_string(i));
		if (!column.has_value())
		{
			return std::nullopt;
		}
		columns.placed.at(i) = *column;
	}
	return columns;
}

} // namespace

TEST(OracleCreatureLandTilt, DataIsComplete)
{
	const auto table = oracle::Table::Load("creature_land_tilt.txt");
	ASSERT_TRUE(table.has_value());
	EXPECT_TRUE(Find(*table).has_value());
	EXPECT_GT(table->Rows(), 300u);
}

// The placement leans the body with the land in the game's order, so every value has the game's bits
TEST(OracleCreatureLandTilt, PlacesTheBodyAsTheGameDoes)
{
	const auto table = oracle::Table::Load("creature_land_tilt.txt");
	ASSERT_TRUE(table.has_value());
	const auto columns = Find(*table);
	ASSERT_TRUE(columns.has_value());
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto in = [&](size_t i) { return table->Float(row, columns->inputs.at(i)); };
		const glm::vec3 position {in(0), in(1), in(2)};
		const float scale = in(4);
		const float radius = in(5);
		const bool held = table->Bits(row, columns->inputs.at(6)) != 0;
		const float kx = in(7);
		const float kz = in(8);
		const float cosine = in(9);
		const float sine = in(10);
		// The test land of the data: each step a float
		const auto land = [kx, kz](glm::vec2 point) { return ((point.x * point.x) * kx) + (point.y * kz); };
		creature_land_tilt::Tilt tilt;
		if (held)
		{
			tilt = creature_land_tilt::FromHeights(
			    {.plusX = position.y, .plusZ = position.y, .minusX = position.y, .minusZ = position.y}, radius);
		}
		else
		{
			const auto points = creature_land_tilt::SamplePoints(position, radius);
			tilt = creature_land_tilt::FromHeights(
			    {.plusX = land(points[0]), .plusZ = land(points[1]), .minusX = land(points[2]), .minusZ = land(points[3])},
			    radius);
		}
		// The game's turn: each axis of the body is a column here
		const glm::mat3 rotation {glm::vec3(cosine, 0.0f, sine), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(-sine, 0.0f, cosine)};
		const auto placed = creature_land_tilt::Placement(position, rotation, glm::vec3(scale), tilt);
		for (int axis = 0; axis < 4; ++axis)
		{
			for (int part = 0; part < 3; ++part)
			{
				const auto expected = table->Bits(row, columns->placed.at(static_cast<size_t>((axis * 3) + part)));
				const auto ours = std::bit_cast<uint32_t>(placed[axis][part]);
				// The game's zeros may carry a sign that no reader can see
				const bool zeros = ((expected | ours) & 0x7FFFFFFFu) == 0;
				if (ours != expected && !zeros)
				{
					++different;
					ADD_FAILURE() << "row " << row << " value " << ((axis * 3) + part) << ": the game gives " << std::hex
					              << expected << ", ours gives " << ours;
				}
			}
		}
	}
	EXPECT_EQ(different, 0u) << "of " << table->Rows() << " rows";
}

TEST(CreatureLandTilt, FlatLandLeavesTheBodyUpright)
{
	const auto tilt = creature_land_tilt::FromHeights({.plusX = 4.0f, .plusZ = 4.0f, .minusX = 4.0f, .minusZ = 4.0f}, 3.0f);
	EXPECT_EQ(tilt.alongX, 0.0f);
	EXPECT_EQ(tilt.alongZ, 0.0f);
	const glm::mat3 turn {glm::vec3(0.6f, 0.0f, 0.8f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(-0.8f, 0.0f, 0.6f)};
	EXPECT_EQ(creature_land_tilt::Lean(turn, tilt), turn);
}

TEST(CreatureLandTilt, TheSlopeIsTheDifferenceTimesTheShareOverTwiceTheRadiusAndStopsAtTheLimits)
{
	// Along x the land rises 1 over 4 m: 0.6 / 4 = 0.15. Along z it rises 10, so the slope stops at 0.3
	const auto tilt = creature_land_tilt::FromHeights({.plusX = 6.0f, .plusZ = 15.0f, .minusX = 5.0f, .minusZ = 5.0f}, 2.0f);
	EXPECT_EQ(tilt.alongX, ((6.0f - 5.0f) * creature_land_tilt::k_SlopeShare) / 4.0f);
	EXPECT_EQ(tilt.alongZ, creature_land_tilt::k_MostRise);
	const auto down = creature_land_tilt::FromHeights({.plusX = 0.0f, .plusZ = 0.0f, .minusX = 10.0f, .minusZ = 0.0f}, 1.0f);
	EXPECT_EQ(down.alongX, -creature_land_tilt::k_MostRise);
}

TEST(CreatureLandTilt, ARadiusOfNothingDropsBothWaysToTheLimit)
{
	// Zero over zero is not a number, and the game keeps such a slope at the most drop
	const auto tilt = creature_land_tilt::FromHeights({.plusX = 2.0f, .plusZ = 2.0f, .minusX = 2.0f, .minusZ = 2.0f}, 0.0f);
	EXPECT_EQ(tilt.alongX, -creature_land_tilt::k_MostRise);
	EXPECT_EQ(tilt.alongZ, -creature_land_tilt::k_MostRise);
}

TEST(CreatureLandTilt, AHeldCreatureStaysFlatAndTheLandIsReadOnTheFourSides)
{
	std::vector<std::pair<int32_t, int32_t>> read;
	const auto altitude = [&read](int32_t x, int32_t z) {
		read.emplace_back(x, z);
		return static_cast<float>(x) * 0.0001f;
	};
	const glm::vec3 at {100.0f, 7.0f, 200.0f};
	const auto held = creature_land_tilt::Of(at, 5.0f, true, altitude);
	EXPECT_TRUE(read.empty());
	EXPECT_EQ(held.alongX, 0.0f);
	EXPECT_EQ(held.alongZ, 0.0f);

	const auto free = creature_land_tilt::Of(at, 5.0f, false, altitude);
	ASSERT_EQ(read.size(), 4u);
	using creature_land_tilt::SampleUnit;
	EXPECT_EQ(read[0], std::pair(SampleUnit(105.0f), SampleUnit(200.0f)));
	EXPECT_EQ(read[1], std::pair(SampleUnit(100.0f), SampleUnit(205.0f)));
	EXPECT_EQ(read[2], std::pair(SampleUnit(95.0f), SampleUnit(200.0f)));
	EXPECT_EQ(read[3], std::pair(SampleUnit(100.0f), SampleUnit(195.0f)));
	EXPECT_GT(free.alongX, 0.0f);
	EXPECT_EQ(free.alongZ, 0.0f);
}

TEST(CreatureLandTilt, AMetreIsAMapUnitInTwoFloatSteps)
{
	EXPECT_EQ(creature_land_tilt::SampleUnit(10.0f), 65536);
	EXPECT_EQ(creature_land_tilt::SampleUnit(-1.0f), -6553);
}

TEST(CreatureLandTilt, TheBodyIsShearedSoItsUpAxisStaysUpright)
{
	const glm::mat3 turn {glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f)};
	const auto leaned = creature_land_tilt::Lean(turn, {.alongX = 0.25f, .alongZ = -0.125f});
	EXPECT_EQ(leaned[1], glm::vec3(0.0f, 1.0f, 0.0f));
	EXPECT_EQ(leaned[0], glm::vec3(0.0f, -0.125f, 1.0f));
	EXPECT_EQ(leaned[2], glm::vec3(-1.0f, -0.25f, 0.0f));
}
