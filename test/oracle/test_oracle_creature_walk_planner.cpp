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
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureRoute.h"
#include "Creature/CreatureWalkPlanner.h"
#include "OracleData.h"

using namespace openblack;
namespace planner = openblack::creature_walk_planner;
namespace route = openblack::creature_route;

namespace
{
constexpr uint32_t k_Unchanged = 0x7FC0DEADu;
constexpr uint32_t k_NoClip = 0xFFFFFFFFu;
/// The phase numbers of the game's walk, and the number that the stubbed start chooser gives back
constexpr uint32_t k_TurnPhase = 4;
constexpr uint32_t k_StepPhase = 5;
constexpr uint32_t k_WalkPhase = 6;
constexpr uint32_t k_ChooserRan = 9;
/// The side numbers of the game: the left steps or spins, and the right ones
constexpr uint32_t k_LeftSide = 1;
constexpr uint32_t k_RightSide = 2;

uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}

/// The columns of a table, looked up by name once
class Columns
{
public:
	explicit Columns(const oracle::Table& table)
	    : _table(table)
	{
	}
	[[nodiscard]] uint32_t Bits(size_t row, std::string_view name) const { return _table.Bits(row, Index(name)); }
	[[nodiscard]] float Float(size_t row, std::string_view name) const { return _table.Float(row, Index(name)); }
	[[nodiscard]] glm::vec2 Point(size_t row, std::string_view x, std::string_view z) const
	{
		return {Float(row, x), Float(row, z)};
	}

private:
	[[nodiscard]] size_t Index(std::string_view name) const
	{
		const auto column = _table.Column(name);
		EXPECT_TRUE(column.has_value()) << name;
		return column.value_or(0);
	}
	const oracle::Table& _table;
};

route::Leg ArcLeg(glm::vec2 from, glm::vec2 to, const route::Circle& circle, uint32_t way)
{
	return {.from = from,
	        .to = to,
	        .arc = true,
	        .round = circle,
	        .roundWay = way == 2 ? route::Way::Anticlockwise : route::Way::Clockwise};
}

/// The follower of the oracle's route: its place and heading, and how far along which leg it is
struct Follower
{
	std::vector<route::Leg> legs;
	size_t segment {0};
	float travelled {0.0f};
	glm::vec2 position {0.0f};
	GameHeading heading {};

	[[nodiscard]] bool Finished() const { return segment >= legs.size(); }

	void Place()
	{
		if (const auto placed = route::PlaceAlong(legs[segment], travelled))
		{
			position = placed->position;
			heading = placed->heading;
		}
	}

	/// The move along the route: a leg is passed only when the distance travelled is beyond its length
	void Advance(float distance)
	{
		travelled += distance;
		while (!Finished() && route::LengthOf(legs[segment]) < travelled)
		{
			travelled -= route::LengthOf(legs[segment]);
			++segment;
		}
		if (Finished())
		{
			position = legs.back().to;
			return;
		}
		Place();
	}
};
} // namespace

// The place and the heading at a distance along a straight leg or an arc, and the leg's length
TEST(OracleCreatureWalkPlanner, ThePlaceAlongALegIsTheGames)
{
	const auto table = oracle::Table::Load("creature_route_place.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GE(table->Rows(), 1500u);
	const Columns c(*table);
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto from = c.Point(row, "from_x", "from_z");
		const auto to = c.Point(row, "to_x", "to_z");
		const auto leg =
		    c.Bits(row, "kind") == 1
		        ? ArcLeg(from, to, {.centre = c.Point(row, "circle_x", "circle_z"), .radius = c.Float(row, "radius")},
		                 c.Bits(row, "way"))
		        : route::Leg {.from = from, .to = to};
		EXPECT_EQ(Bits(route::LengthOf(leg)), c.Bits(row, "length")) << "row " << row;
		const auto placed = route::PlaceAlong(leg, c.Float(row, "distance"));
		if (c.Bits(row, "heading") == k_Unchanged)
		{
			EXPECT_FALSE(placed.has_value()) << "row " << row;
			continue;
		}
		ASSERT_TRUE(placed.has_value()) << "row " << row;
		EXPECT_EQ(Bits(placed->position.x), c.Bits(row, "place_x")) << "row " << row;
		EXPECT_EQ(Bits(placed->position.y), c.Bits(row, "place_z")) << "row " << row;
		EXPECT_EQ(Bits(placed->heading.Radians()), c.Bits(row, "heading")) << "row " << row;
	}
}

// One turn of a walk: the caps, the slope, the speed up, the move along the route and the stop at a sharp turn
TEST(OracleCreatureWalkPlanner, AWalkTurnIsTheGames)
{
	const auto table = oracle::Table::Load("creature_walk_turn.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GE(table->Rows(), 1500u);
	const Columns c(*table);
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		Follower follower {.legs = {route::Leg {.from = c.Point(row, "a_x", "a_z"), .to = c.Point(row, "b_x", "b_z")}},
		                   .travelled = c.Float(row, "travelled"),
		                   .heading = GameHeading(c.Float(row, "route_heading"))};
		planner::NextLeg next {.kind = planner::NextLeg::Kind::None};
		if (c.Bits(row, "more") == 1)
		{
			const auto from = c.Point(row, "c_x", "c_z");
			const auto to = c.Point(row, "d_x", "d_z");
			if (c.Bits(row, "second_arc") == 1)
			{
				const route::Circle circle {.centre = c.Point(row, "circle_x", "circle_z"), .radius = c.Float(row, "radius")};
				follower.legs.push_back(ArcLeg(from, to, circle, c.Bits(row, "way")));
				next = {.kind = planner::NextLeg::Kind::Arc, .radius = circle.radius};
			}
			else
			{
				follower.legs.push_back({.from = from, .to = to});
				next = {.kind = planner::NextLeg::Kind::Straight};
			}
		}
		const auto top = c.Float(row, "top");
		const auto radius = c.Float(row, "body_radius");
		auto heading = GameHeading(c.Float(row, "heading"));
		auto creature = c.Point(row, "pos_x", "pos_z");
		const auto cap = planner::SpeedCap(top, route::LengthOf(follower.legs[0]), follower.travelled, next);
		const auto slope = planner::SlopeFactor(c.Float(row, "altitude"), c.Float(row, "pos_y"), radius);
		auto speed = planner::NextSpeed(c.Float(row, "speed"), slope * top, cap);
		follower.Advance(planner::TurnDistance(speed));
		auto phase = k_WalkPhase;
		if (!follower.Finished())
		{
			if (planner::WalkTurnsTooFar(heading, follower.heading))
			{
				speed = 0.0f;
				if (follower.segment != 0)
				{
					follower.travelled = 0.0f;
					follower.Place();
				}
				phase = k_ChooserRan;
			}
			else
			{
				heading = follower.heading;
			}
			creature = follower.position;
		}
		EXPECT_EQ(Bits(speed), c.Bits(row, "speed_after")) << "row " << row;
		EXPECT_EQ(Bits(follower.travelled), c.Bits(row, "travelled_after")) << "row " << row;
		EXPECT_EQ(follower.Finished() ? k_NoClip : static_cast<uint32_t>(follower.segment), c.Bits(row, "leg"))
		    << "row " << row;
		EXPECT_EQ(Bits(follower.position.x), c.Bits(row, "follower_x")) << "row " << row;
		EXPECT_EQ(Bits(follower.position.y), c.Bits(row, "follower_z")) << "row " << row;
		EXPECT_EQ(Bits(follower.heading.Radians()), c.Bits(row, "follower_heading")) << "row " << row;
		EXPECT_EQ(Bits(creature.x), c.Bits(row, "creature_x")) << "row " << row;
		EXPECT_EQ(Bits(creature.y), c.Bits(row, "creature_z")) << "row " << row;
		EXPECT_EQ(Bits(heading.Radians()), c.Bits(row, "heading_after")) << "row " << row;
		EXPECT_EQ(phase, c.Bits(row, "phase")) << "row " << row;
	}
}

// The blend of the stand, the walk and the run for one turn
TEST(OracleCreatureWalkPlanner, TheGaitIsTheGames)
{
	const auto table = oracle::Table::Load("creature_walk_gait.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GE(table->Rows(), 1500u);
	const Columns c(*table);
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const planner::Clip stand {.durationMs = static_cast<int32_t>(c.Bits(row, "stand_ms")),
		                           .stride = c.Float(row, "stand_stride")};
		const planner::Clip walk {.durationMs = static_cast<int32_t>(c.Bits(row, "walk_ms")),
		                          .stride = c.Float(row, "walk_stride")};
		const planner::Clip run {.durationMs = static_cast<int32_t>(c.Bits(row, "run_ms")),
		                         .stride = c.Float(row, "run_stride")};
		const auto gait = planner::BlendGait(c.Float(row, "speed"), c.Float(row, "walk"), c.Float(row, "run"),
		                                     c.Float(row, "distance"), c.Float(row, "scale"), stand, walk, run,
		                                     static_cast<int32_t>(c.Bits(row, "walk_time")), c.Float(row, "breath"));
		if (c.Bits(row, "clip_a") == k_NoClip)
		{
			EXPECT_FALSE(gait.has_value()) << "row " << row;
			continue;
		}
		ASSERT_TRUE(gait.has_value()) << "row " << row;
		EXPECT_EQ(gait->animations[0], c.Bits(row, "clip_a")) << "row " << row;
		EXPECT_EQ(gait->animations[1], c.Bits(row, "clip_b")) << "row " << row;
		EXPECT_EQ(static_cast<uint32_t>(gait->timesMs[0]), c.Bits(row, "time_a")) << "row " << row;
		EXPECT_EQ(static_cast<uint32_t>(gait->timesMs[1]), c.Bits(row, "time_b")) << "row " << row;
		EXPECT_EQ(Bits(gait->weights[0]), c.Bits(row, "weight_a")) << "row " << row;
		EXPECT_EQ(Bits(gait->weights[1]), c.Bits(row, "weight_b")) << "row " << row;
	}
}

// The start towards a point: the phase, the heading, the side, and the step's length and move
TEST(OracleCreatureWalkPlanner, TheStartIsTheGames)
{
	const auto table = oracle::Table::Load("creature_walk_start.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GE(table->Rows(), 1500u);
	const Columns c(*table);
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto missing = c.Bits(row, "missing");
		const auto stepsFrom = [&](size_t first) -> planner::StepSet {
			std::array<planner::StepClip, 3> steps {};
			for (size_t i = 0; i < steps.size(); ++i)
			{
				const auto k = first + i;
				if (missing == k)
				{
					return std::nullopt;
				}
				const auto prefix = "move" + std::to_string(k) + "_";
				steps.at(i) = {
				    .displacement = {c.Float(row, prefix + "x"), c.Float(row, prefix + "y"), c.Float(row, prefix + "z")},
				    .frames = static_cast<int32_t>(c.Bits(row, "frames" + std::to_string(k)))};
			}
			return steps;
		};
		const auto heading = GameHeading(c.Float(row, "heading"));
		const auto start =
		    planner::ChooseStart(c.Point(row, "from_x", "from_z"), c.Point(row, "to_x", "to_z"), heading, c.Float(row, "scale"),
		                         c.Float(row, "walk"), stepsFrom(creature_locomotion::animations::k_RightStep),
		                         stepsFrom(creature_locomotion::animations::k_LeftStep));
		const auto phase = start.phase == planner::Phase::Turn   ? k_TurnPhase
		                   : start.phase == planner::Phase::Step ? k_StepPhase
		                                                         : k_WalkPhase;
		ASSERT_EQ(phase, c.Bits(row, "phase")) << "row " << row;
		EXPECT_EQ(Bits(start.heading.Radians()), c.Bits(row, "route_heading")) << "row " << row;
		// A turn on the spot takes the side of its spins
		const auto side =
		    start.phase == planner::Phase::Turn ? creature_locomotion::SpinSideOf(start.heading.TurnFrom(heading)) : start.side;
		EXPECT_EQ(side == creature_locomotion::Side::Right ? k_RightSide : k_LeftSide, c.Bits(row, "side")) << "row " << row;
		if (start.phase != planner::Phase::Step)
		{
			continue;
		}
		EXPECT_EQ(static_cast<uint32_t>(start.stepMs), c.Bits(row, "step_ms")) << "row " << row;
		EXPECT_EQ(Bits(start.stepDisplacement.x), c.Bits(row, "step_x")) << "row " << row;
		EXPECT_EQ(Bits(start.stepDisplacement.y), c.Bits(row, "step_y")) << "row " << row;
		EXPECT_EQ(Bits(start.stepDisplacement.z), c.Bits(row, "step_z")) << "row " << row;
	}
}

// One turn of a step off into the walk: the stop at a sharp turn, the end into the walk, or the step's move along the
// route
TEST(OracleCreatureWalkPlanner, AStepTurnIsTheGames)
{
	const auto table = oracle::Table::Load("creature_walk_step.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GE(table->Rows(), 1500u);
	const Columns c(*table);
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto b = c.Point(row, "b_x", "b_z");
		Follower follower {.legs = {route::Leg {.from = c.Point(row, "a_x", "a_z"), .to = b},
		                            route::Leg {.from = b, .to = c.Point(row, "c_x", "c_z")}},
		                   .travelled = c.Float(row, "travelled"),
		                   .heading = GameHeading(c.Float(row, "route_heading"))};
		auto speed = c.Float(row, "speed");
		auto heading = GameHeading(c.Float(row, "heading"));
		auto stepHeading = GameHeading(c.Float(row, "step_heading"));
		auto timeMs = static_cast<int32_t>(c.Bits(row, "time"));
		const auto stepMs = static_cast<int32_t>(c.Bits(row, "step_ms"));
		const auto turnMs = static_cast<int32_t>(c.Bits(row, "turn_ms"));
		uint32_t walkTime = k_Unchanged;
		std::optional<glm::vec2> creature;
		uint32_t phase = k_StepPhase;
		if (planner::StepTurnsTooFar(stepHeading, follower.heading))
		{
			heading = stepHeading;
			speed = 0.0f;
			follower.travelled = 0.0f;
			follower.Place();
			creature = follower.position;
			phase = k_ChooserRan;
		}
		else
		{
			heading = planner::StepHeading(heading, stepHeading, follower.heading);
			stepHeading = follower.heading;
			if (timeMs + turnMs >= stepMs)
			{
				heading = stepHeading;
				const auto side =
				    c.Bits(row, "side") == k_RightSide ? creature_locomotion::Side::Right : creature_locomotion::Side::Left;
				walkTime = static_cast<uint32_t>(
				    planner::WalkTimeAfterStep(timeMs, stepMs, static_cast<int32_t>(c.Bits(row, "walk_ms")), side));
				phase = k_WalkPhase;
			}
			else
			{
				// The step places the body through a call that the oracle stubs, so the place stays as it was
				timeMs += turnMs;
				const glm::vec3 move {c.Float(row, "move_x"), c.Float(row, "move_y"), c.Float(row, "move_z")};
				speed = planner::StepSpeed(move, turnMs, c.Float(row, "scale"), stepMs, heading);
				follower.Advance(planner::TurnDistance(speed));
			}
		}
		EXPECT_EQ(phase, c.Bits(row, "phase")) << "row " << row;
		EXPECT_EQ(Bits(heading.Radians()), c.Bits(row, "heading_after")) << "row " << row;
		EXPECT_EQ(Bits(stepHeading.Radians()), c.Bits(row, "step_heading_after")) << "row " << row;
		EXPECT_EQ(static_cast<uint32_t>(timeMs), c.Bits(row, "time_after")) << "row " << row;
		EXPECT_EQ(walkTime, c.Bits(row, "walk_time")) << "row " << row;
		EXPECT_EQ(Bits(speed), c.Bits(row, "speed_after")) << "row " << row;
		EXPECT_EQ(Bits(follower.travelled), c.Bits(row, "travelled_after")) << "row " << row;
		EXPECT_EQ(follower.Finished() ? k_NoClip : static_cast<uint32_t>(follower.segment), c.Bits(row, "leg"))
		    << "row " << row;
		EXPECT_EQ(Bits(follower.position.x), c.Bits(row, "follower_x")) << "row " << row;
		EXPECT_EQ(Bits(follower.position.y), c.Bits(row, "follower_z")) << "row " << row;
		EXPECT_EQ(Bits(follower.heading.Radians()), c.Bits(row, "follower_heading")) << "row " << row;
		EXPECT_EQ(creature.has_value() ? Bits(creature->x) : k_Unchanged, c.Bits(row, "creature_x")) << "row " << row;
		EXPECT_EQ(creature.has_value() ? Bits(creature->y) : k_Unchanged, c.Bits(row, "creature_z")) << "row " << row;
	}
}
