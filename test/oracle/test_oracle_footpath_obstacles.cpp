/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>

#include <bit>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "ECS/FootpathObstacles.h"
#include "ECS/FootpathPlanning.h"
#include "OracleData.h"

// The game's own functions give the data (tools/oracle/cases/footpath_obstacles_cases.py): a path sent round a thing
// put down on it, and a path returned when the thing is removed. The game's route planner is replaced there by a
// stand-in that records the stretches it is asked for and either finds no way or lays an empty route. The fakes here do
// the same. Each test compares our nodes and the stretches asked for with every row and shows the first rows that
// differ.

using namespace openblack;
using namespace openblack::ecs;
namespace obstacle_rules = openblack::ecs::footpath_obstacles;

namespace
{
constexpr size_t k_FailuresShown = 20;
constexpr uint32_t k_Planned = 0x2;
constexpr uint32_t k_Hidden = 0x4;
constexpr uint32_t k_Marker = 0x8;
constexpr uint32_t k_NoIndex = 0xFFFFFFFF;

struct Columns
{
	const oracle::Table& table;
	size_t row;

	[[nodiscard]] uint32_t Bits(const std::string& name) const { return table.Bits(row, table.Column(name).value()); }
	[[nodiscard]] glm::ivec2 Place(const std::string& prefix) const
	{
		return {static_cast<int32_t>(Bits(prefix + "_x")), static_cast<int32_t>(Bits(prefix + "_z"))};
	}
	[[nodiscard]] footpath_planning::PlanNode Node(const std::string& prefix) const
	{
		const auto flags = Bits(prefix + "_flags");
		return {.place = Place(prefix),
		        .planned = (flags & k_Planned) != 0,
		        .hidden = (flags & k_Hidden) != 0,
		        .marker = (flags & k_Marker) != 0};
	}
};

struct Plan
{
	glm::ivec2 from;
	glm::ivec2 to;
};

/// Runs one row through our rules; the differences from the game's row, or nothing when they agree
std::optional<std::string> Compare(const Columns& at, bool goRound)
{
	std::vector<obstacle_rules::KeyedNode> nodes;
	for (uint32_t i = 0; i < at.Bits("in_count"); ++i)
	{
		nodes.push_back({.node = at.Node("in" + std::to_string(i)), .key = i + 1});
	}
	const bool found = at.Bits("found") != 0;
	std::vector<Plan> plans;
	const obstacle_rules::Hooks hooks {
	    .planRoute = [&plans, found](glm::ivec2 from, glm::ivec2 to) -> std::optional<std::vector<glm::ivec2>> {
		    plans.push_back({from, to});
		    return found ? std::optional(std::vector<glm::ivec2> {}) : std::nullopt;
	    },
	    .lookAgain = [](uint32_t /*key*/, bool /*towardsStart*/) {},
	    .sendOn = [](uint32_t /*gone*/, uint32_t /*back*/, glm::ivec2 /*backPlace*/, uint32_t /*on*/,
	                 glm::ivec2 /*onPlace*/) {},
	    .cutOff = [](uint32_t /*gone*/, uint32_t /*onwardFrom*/, uint32_t /*first*/) {},
	    .altitude = [](glm::ivec2 /*place*/) { return 0.0f; },
	};
	const obstacle_rules::Obstacle obstacle {.radius = std::bit_cast<float>(at.Bits("radius")), .place = at.Place("centre")};
	if (goRound)
	{
		obstacle_rules::GoRound(nodes, obstacle, hooks);
	}
	else
	{
		obstacle_rules::StopGoingRound(nodes, obstacle, hooks);
	}

	if (plans.size() != at.Bits("plan_count"))
	{
		return std::to_string(plans.size()) + " stretches planned, the game " + std::to_string(at.Bits("plan_count"));
	}
	for (size_t i = 0; i < plans.size(); ++i)
	{
		const auto prefix = "plan" + std::to_string(i);
		if (plans[i].from != at.Place(prefix + "_from") || plans[i].to != at.Place(prefix + "_to"))
		{
			return "stretch " + std::to_string(i) + " is not the game's";
		}
		if ((at.Bits(prefix + "_marker") != 0) != goRound)
		{
			return "stretch " + std::to_string(i) + " has a marker only when the path goes round";
		}
	}

	std::vector<footpath_planning::PlanNode> expected;
	for (uint32_t i = 0; i < at.Bits("out_count"); ++i)
	{
		expected.push_back(at.Node("out" + std::to_string(i)));
	}
	// The game's planner stand-in lays no marker; ours lays the marker after the empty route, after the first anchor
	if (goRound && found && at.Bits("first_index") != k_NoIndex)
	{
		const auto after = static_cast<std::ptrdiff_t>(at.Bits("first_index")) + 1;
		expected.insert(expected.begin() + after,
		                {.place = at.Place("centre"), .planned = true, .hidden = false, .marker = true});
	}
	if (nodes.size() != expected.size())
	{
		return std::to_string(nodes.size()) + " nodes, the game " + std::to_string(expected.size());
	}
	for (size_t i = 0; i < nodes.size(); ++i)
	{
		if (nodes[i].node != expected[i])
		{
			return "node " + std::to_string(i) + " differs";
		}
	}
	return std::nullopt;
}

void CompareTable(const std::string& name, bool goRound)
{
	const auto table = oracle::Table::Load(name);
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 0u);
	size_t failures = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		if (const auto difference = Compare(Columns {*table, row}, goRound); difference.has_value())
		{
			if (++failures <= k_FailuresShown)
			{
				ADD_FAILURE() << name << " row " << row << ": " << *difference;
			}
		}
	}
	EXPECT_EQ(failures, 0u);
}

} // namespace

TEST(OracleFootpathObstacles, GoRoundMatchesTheGame)
{
	CompareTable("footpath_go_round.txt", true);
}

TEST(OracleFootpathObstacles, StopGoingRoundMatchesTheGame)
{
	CompareTable("footpath_stop_going_round.txt", false);
}
