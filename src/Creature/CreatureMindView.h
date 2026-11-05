/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <deque>
#include <optional>
#include <string_view>
#include <vector>

#include "Creature/CreatureDesires.h"
#include "Creature/CreaturePlanner.h"

/// The data of the developer view of a creature's mind: its active desires, the sources that drive each desire, and
/// the plans that the planner made. The functions only read the mind. The view never changes the world.
namespace openblack::creature_mind_view
{
/// The trace keeps the desire values of this many turns, and this many plan steps
constexpr size_t k_SampleTurns = 300;
constexpr size_t k_PlanSteps = 64;

/// A short English name for a kind of desire source, by its place in the game's source table
[[nodiscard]] std::string_view SourceName(uint32_t type);

/// One source of a desire, and its part of the desire
struct SourcePart
{
	uint32_t type {creature_desires::k_NoSource};
	float value {0.0f};
	float threshold {1.0f};
	float multiplier {1.0f};
	/// The drive that the source gives the desire in one turn (the sigmoid of its value past its threshold)
	float part {0.0f};
	/// The drive that the source gave since the creature last decided on the desire
	float drive {0.0f};
};

/// One active desire, as the view shows it
struct DesireRow
{
	creature_desires::Desire desire {creature_desires::Desire::Impress};
	float value {0.0f};
	float max {1.0f};
	/// The change of the value since the turn before, when the trace has that turn
	std::optional<float> change;
	/// The sum of the parts of all its sources
	float drive {0.0f};
	/// How much the value grows in one turn with this drive. Zero while the desire is held back or has no drive.
	float growthPerTurn {0.0f};
	uint32_t suppressedTurns {0};
	/// The desire with the highest value. The creature shows this desire.
	bool winner {false};
	std::vector<SourcePart> sources;
};

/// A desire value or drive less than this does not make a desire active in the view. Values that fade never get to 0.
constexpr float k_ActiveFloor = 0.001f;

/// Whether the view shows a desire: it is activated, and its value or its drive is at least k_ActiveFloor
[[nodiscard]] bool IsActive(const creature_desires::Desires& desires, size_t desire);
/// The parts of the sources of one desire
[[nodiscard]] std::vector<SourcePart> SourcesOf(const creature_desires::DesireState& desire);
/// The active desires, the highest value first (the first desire of equals first). `before` is the values of the turn
/// before, if the trace has them.
[[nodiscard]] std::vector<DesireRow> ActiveDesires(const creature_desires::Desires& desires,
                                                   const std::array<float, creature_desires::k_DesireCount>* before,
                                                   float turnsPerSecond);

/// What became of a plan that the planner made
enum class Outcome : uint8_t
{
	/// The creature started the plan: it was more than twice as pressing as the plan before
	Chosen,
	/// An order, a reaction or a script forced the plan on the creature
	Forced,
	/// The plan is the plan that the creature already does
	Kept,
	/// The plan of another desire is more pressing
	NotBest,
	/// The plan is the most pressing, but not more than twice as pressing as the current plan
	NotPressing,
	/// The planner found no plan for the desire, or the desire cannot have a plan now
	NoPlan,
	/// The plan that the creature did stopped, and no plan replaced it
	Ended,
};

[[nodiscard]] std::string_view Name(Outcome outcome);

/// One step of the planner, as the trace saw it at the end of a turn
struct PlanStep
{
	uint32_t turn {0};
	creature_desires::Desire desire {creature_desires::Desire::Impress};
	/// The plan, none for NoPlan
	std::optional<creature_planner::Plan> plan;
	Outcome outcome {Outcome::NoPlan};
	/// The priority of the plan that the creature did at that time, none when it did no plan
	std::optional<float> current;
	/// For NotBest: the desire of the most pressing plan, and its priority
	std::optional<creature_desires::Desire> best;
	float bestPriority {0.0f};
};

/// The desire values at the end of one turn
struct Sample
{
	uint32_t turn {0};
	std::array<float, creature_desires::k_DesireCount> values {};
};

/// The record of one creature's mind over the last turns. Record reads the mind once each turn and finds what the
/// planner did by a comparison with the turn before.
class Trace
{
public:
	void Record(uint32_t turn, const creature_desires::Desires& desires, const creature_planner::PlannerState& planner,
	            bool planForced);
	void Clear();

	[[nodiscard]] const std::deque<Sample>& Samples() const { return _samples; }
	[[nodiscard]] const std::deque<PlanStep>& Steps() const { return _steps; }
	/// The values of the turn before the last recorded turn, none with fewer than two turns
	[[nodiscard]] const std::array<float, creature_desires::k_DesireCount>* Before() const;

private:
	void Add(PlanStep step);

	std::deque<Sample> _samples;
	std::deque<PlanStep> _steps;
	std::array<std::optional<creature_planner::Plan>, creature_desires::k_DesireCount> _lastBest {};
	std::optional<creature_planner::Plan> _lastCurrent;
	bool _started {false};
};

/// Whether two plans are the same plan: the same desire, action, thing and priority
[[nodiscard]] bool SamePlan(const std::optional<creature_planner::Plan>& a, const std::optional<creature_planner::Plan>& b);

} // namespace openblack::creature_mind_view
