/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureRoute.h"
#include "Creature/CreatureRouteSearch.h"
#include "ECS/FootpathPlanning.h"
#include "OracleData.h"

// The game's own functions give the data (tools/oracle/cases/footpath_planning_cases.py). Each test compares our result
// with every row, bit for bit, and shows the first rows that differ.

using namespace openblack;
using namespace openblack::ecs;

namespace
{
constexpr size_t k_FailuresShown = 20;
constexpr size_t k_MostNodes = 64;

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

/// Reads the named columns of a row
struct Columns
{
	const oracle::Table& table;
	size_t row;

	[[nodiscard]] size_t Index(const std::string& name) const { return table.Column(name).value(); }
	[[nodiscard]] float Float(const std::string& name) const { return table.Float(row, Index(name)); }
	[[nodiscard]] uint32_t Bits(const std::string& name) const { return table.Bits(row, Index(name)); }
	[[nodiscard]] glm::vec2 Point(const std::string& prefix) const { return {Float(prefix + "_x"), Float(prefix + "_z")}; }
	[[nodiscard]] glm::ivec2 Place(const std::string& prefix) const
	{
		return {static_cast<int32_t>(Bits(prefix + "_x")), static_cast<int32_t>(Bits(prefix + "_z"))};
	}
};

creature_route::Way WayOf(uint32_t way)
{
	return way == 1 ? creature_route::Way::Clockwise : creature_route::Way::Anticlockwise;
}

} // namespace

TEST(OracleFootpathPlanning, RouteNodesMatchTheGame)
{
	const auto table = oracle::Table::Load("footpath_route_nodes.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 0u);
	Differences differences;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const Columns at {*table, row};
		const auto start = at.Point("start");
		const auto goal = at.Point("goal");
		const auto a = at.Point("a");
		const auto b = at.Point("b");
		const auto c = at.Point("c");
		const auto d = at.Point("d");
		std::vector<creature_route::Obstacle> circles {
		    {.circle = {.centre = at.Point("c0"), .radius = at.Float("c0_r")}},
		    {.circle = {.centre = at.Point("c1"), .radius = at.Float("c1_r")}},
		};
		const std::vector<creature_route::Leg> legs {
		    {.from = start, .to = a, .heading = 0u, .way = WayOf(at.Bits("way0"))},
		    {.from = a, .to = b, .arc = true},
		    {.from = b, .to = c, .heading = 1u, .way = WayOf(at.Bits("way1"))},
		    {.from = c, .to = d, .arc = true},
		    {.from = d, .to = goal},
		};
		const auto ours = footpath_planning::RoutePlaces(legs, circles, start, goal);
		const auto count = at.Bits("count");
		bool same = ours.size() == count;
		for (size_t i = 0; same && i < count && i < k_MostNodes; ++i)
		{
			same = ours[i] == at.Place("n" + std::to_string(i));
		}
		if (!same)
		{
			differences.Add("row " + std::to_string(row) + ": " + std::to_string(ours.size()) + " nodes, the game " +
			                std::to_string(count));
		}
	}
	EXPECT_EQ(differences.Count(), 0u);
}

TEST(OracleFootpathPlanning, EndClearOfACircleMatchesTheGame)
{
	const auto table = oracle::Table::Load("footpath_end_clear.txt");
	ASSERT_TRUE(table.has_value());
	Differences differences;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const Columns at {*table, row};
		const std::vector<creature_route::Circle> circles {{.centre = at.Point("centre"), .radius = at.Float("radius")}};
		// Not on the building's place, so only the circle moves it
		const auto end = at.Point("end");
		const auto building = at.Point("building");
		if (end == building)
		{
			continue;
		}
		const auto ours = footpath_planning::ClearOfBuilding(end, building, end, circles);
		if (Bits(ours.x) != at.Bits("moved_x") || Bits(ours.y) != at.Bits("moved_z"))
		{
			differences.Add("row " + std::to_string(row) + ": " + oracle::Hex(ours.x) + " " + oracle::Hex(ours.y));
		}
	}
	EXPECT_EQ(differences.Count(), 0u);
}

TEST(OracleFootpathPlanning, EndOffTheMiddleMatchesTheGame)
{
	const auto table = oracle::Table::Load("footpath_off_middle.txt");
	ASSERT_TRUE(table.has_value());
	Differences differences;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const Columns at {*table, row};
		const auto end = at.Point("end");
		const auto ours = footpath_planning::ClearOfBuilding(end, end, at.Point("other"), {});
		if (Bits(ours.x) != at.Bits("moved_x") || Bits(ours.y) != at.Bits("moved_z"))
		{
			differences.Add("row " + std::to_string(row) + ": " + oracle::Hex(ours.x) + " " + oracle::Hex(ours.y));
		}
	}
	EXPECT_EQ(differences.Count(), 0u);
}

TEST(OracleFootpathPlanning, PointsAndRangeMatchTheGame)
{
	const auto table = oracle::Table::Load("footpath_points.txt");
	ASSERT_TRUE(table.has_value());
	Differences differences;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const Columns at {*table, row};
		const auto point = footpath_planning::ToPoint(at.Place("place"));
		const auto back = footpath_planning::ToPlace(at.Point("other"));
		const auto range = footpath_planning::Range(at.Point("point"), at.Point("other"));
		if (Bits(point.x) != at.Bits("point_x") || Bits(point.y) != at.Bits("point_z") || back != at.Place("back") ||
		    Bits(range) != at.Bits("range"))
		{
			differences.Add("row " + std::to_string(row));
		}
	}
	EXPECT_EQ(differences.Count(), 0u);
}
