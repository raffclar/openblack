/******************************************************************************
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
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/LandLine.h"
#include "3D/MapCoords.h"
#include "Animals/AnimalRules.h"
#include "OracleData.h"

// The game's own functions give the data (tools/oracle/cases/formation_land_line_cases.py). Each test compares our
// result with every row, bit for bit, and shows the first rows that differ.

using namespace openblack;

namespace
{
constexpr size_t k_FailuresShown = 20;
constexpr int32_t k_Side = 17;

/// Counts the rows that differ and shows the first of them
class Differences
{
public:
	void Add(const std::string& what)
	{
		++_count;
		if (_count <= k_FailuresShown)
		{
			ADD_FAILURE() << what;
		}
	}
	[[nodiscard]] size_t Count() const { return _count; }

private:
	size_t _count {0};
};

[[nodiscard]] uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}

/// The columns of the land line table
enum class LandColumn : size_t
{
	Kind,
	X,
	Z,
	A,
	B,
	C,
	D,
	E,
	F,
	Hit,
	HitX,
	HitZ,
};

struct LandTable
{
	std::optional<oracle::Table> table;
	std::vector<size_t> columns;

	LandTable()
	    : table(oracle::Table::Load("land_line.txt"))
	{
		if (!table.has_value())
		{
			return;
		}
		for (const auto* name : {"kind", "x", "z", "a", "b", "c", "d", "e", "f", "hit", "hit_x", "hit_z"})
		{
			const auto index = table->Column(name);
			if (!index.has_value())
			{
				table.reset();
				return;
			}
			columns.push_back(*index);
		}
	}

	[[nodiscard]] uint32_t Bits(size_t row, LandColumn column) const
	{
		return table->Bits(row, columns.at(static_cast<size_t>(column)));
	}
	[[nodiscard]] float Float(size_t row, LandColumn column) const
	{
		return table->Float(row, columns.at(static_cast<size_t>(column)));
	}
	[[nodiscard]] glm::vec3 Point(size_t row, LandColumn first) const
	{
		const auto at = static_cast<size_t>(first);
		return {Float(row, first), Float(row, static_cast<LandColumn>(at + 1)), Float(row, static_cast<LandColumn>(at + 2))};
	}

	/// The land of the table: one block of corners, the same in each 16 by 16 cells of the map
	[[nodiscard]] land_line::CellLookup Land() const
	{
		std::array<uint8_t, k_Side * k_Side> heights {};
		for (size_t row = 0; row < table->Rows(); ++row)
		{
			if (Bits(row, LandColumn::Kind) == 0)
			{
				heights.at(Bits(row, LandColumn::A)) = static_cast<uint8_t>(Bits(row, LandColumn::C));
			}
		}
		return [heights](int32_t x, int32_t z) -> std::optional<land_line::CellHeights> {
			const auto at = [&heights](int32_t cx, int32_t cz) { return heights.at(cx * k_Side + cz); };
			const int32_t lx = x & 15;
			const int32_t lz = z & 15;
			return land_line::CellHeights {
			    .here = at(lx, lz), .acrossX = at(lx + 1, lz), .acrossZ = at(lx, lz + 1), .acrossBoth = at(lx + 1, lz + 1)};
		};
	}
};

[[nodiscard]] std::string Describe(const std::optional<glm::vec2>& hit)
{
	if (!hit.has_value())
	{
		return "no hit";
	}
	return std::to_string(Bits(hit->x)) + ", " + std::to_string(Bits(hit->y));
}

void Compare(Differences& differences, const std::string& what, const std::optional<glm::vec2>& ours, uint32_t hit, uint32_t x,
             uint32_t z)
{
	const bool same = ours.has_value() ? (hit == 1 && Bits(ours->x) == x && Bits(ours->y) == z) : hit == 0;
	if (!same)
	{
		differences.Add(what + ": the game gives " + (hit == 1 ? std::to_string(x) + ", " + std::to_string(z) : "no hit") +
		                ", ours gives " + Describe(ours));
	}
}
} // namespace

TEST(OracleFormationLandLine, AFollowerGoesToTheGamesFormationGoal)
{
	const auto table = oracle::Table::Load("formation_goal.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 600u);
	std::vector<size_t> columns;
	for (const auto* name : {"place", "leader_x", "leader_z", "follower_x", "follower_z", "goal_x", "goal_z"})
	{
		const auto index = table->Column(name);
		ASSERT_TRUE(index.has_value());
		columns.push_back(*index);
	}
	const auto value = [&](size_t row, size_t column) { return static_cast<int32_t>(table->Bits(row, columns.at(column))); };
	Differences differences;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const glm::ivec2 leader {value(row, 1), value(row, 2)};
		const glm::ivec2 follower {value(row, 3), value(row, 4)};
		const glm::ivec2 expected {value(row, 5), value(row, 6)};
		const auto goal = animals::FormationGoal(leader, follower, animals::FormationSlotOf(value(row, 0)));
		if (goal != expected)
		{
			differences.Add("row " + std::to_string(row) + ": the game gives " + std::to_string(expected.x) + ", " +
			                std::to_string(expected.y) + ", ours gives " + std::to_string(goal.x) + ", " +
			                std::to_string(goal.y));
		}
	}
	EXPECT_EQ(differences.Count(), 0u);
}

TEST(OracleFormationLandLine, ALineInMetresMeetsTheLandWhereTheGameFindsIt)
{
	const LandTable pinned;
	ASSERT_TRUE(pinned.table.has_value());
	const auto land = pinned.Land();
	Differences differences;
	size_t lines = 0;
	for (size_t row = 0; row < pinned.table->Rows(); ++row)
	{
		if (pinned.Bits(row, LandColumn::Kind) != 1)
		{
			continue;
		}
		++lines;
		const auto ours = land_line::LandAlong(pinned.Point(row, LandColumn::A), pinned.Point(row, LandColumn::D), land);
		Compare(differences, "row " + std::to_string(row), ours, pinned.Bits(row, LandColumn::Hit),
		        pinned.Bits(row, LandColumn::HitX), pinned.Bits(row, LandColumn::HitZ));
	}
	EXPECT_GT(lines, 500u);
	EXPECT_EQ(differences.Count(), 0u);
}

TEST(OracleFormationLandLine, TheCellTestHasTheGamesPlaneLimit)
{
	const LandTable pinned;
	ASSERT_TRUE(pinned.table.has_value());
	const auto land = pinned.Land();
	Differences differences;
	size_t cells = 0;
	for (size_t row = 0; row < pinned.table->Rows(); ++row)
	{
		if (pinned.Bits(row, LandColumn::Kind) != 2)
		{
			continue;
		}
		++cells;
		const auto x = static_cast<int32_t>(pinned.Bits(row, LandColumn::X));
		const auto z = static_cast<int32_t>(pinned.Bits(row, LandColumn::Z));
		const auto ours = land_line::HitInCell(x, z, pinned.Point(row, LandColumn::A), pinned.Point(row, LandColumn::D), land);
		Compare(differences, "row " + std::to_string(row), ours, pinned.Bits(row, LandColumn::Hit),
		        pinned.Bits(row, LandColumn::HitX), pinned.Bits(row, LandColumn::HitZ));
	}
	EXPECT_GT(cells, 300u);
	EXPECT_EQ(differences.Count(), 0u);
}

TEST(OracleFormationLandLine, ALineThatClimbsTheLimitMeetsAFlatCell)
{
	// The cell (2, 2) is flat at height 0. A climb of exactly the float 0.0001 meets it, and the float below does not
	const LandTable pinned;
	ASSERT_TRUE(pinned.table.has_value());
	const auto land = pinned.Land();
	constexpr float k_Limit = 0.0001f;
	const glm::vec3 start {2.5f, 0.0f, 2.5f};
	EXPECT_TRUE(land_line::HitInCell(2, 2, start, {2.5f, k_Limit, 2.5f}, land).has_value());
	const float below = std::bit_cast<float>(std::bit_cast<uint32_t>(k_Limit) - 1u);
	EXPECT_FALSE(land_line::HitInCell(2, 2, start, {2.5f, below, 2.5f}, land).has_value());
}
