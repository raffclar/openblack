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
#include <functional>
#include <limits>
#include <optional>
#include <span>
#include <vector>

#include "Creature/CreatureDesires.h"

/// How a creature decides what to do. Each desire it has gets a plan: the thing to act on (the goal), and the action to
/// take. The goal is the thing the creature finds most useful for the desire, by what its decision trees have learnt,
/// the nearer the better: a thing the desire is about by what it has learnt of such things as the point of a desire,
/// anything else by what it has learnt of acting on them. Among the actions that satisfy the desire, its opinion of each and
/// how long since it last did it break the ties. A plan's priority is the desire's strength times how useful the goal and the
/// action are. Two desires are planned again each game turn, and the creature changes what it is doing only for a plan more
/// than twice as pressing as the one it carries out.
namespace openblack::creature_planner
{
/// The farthest a goal counts as being, and how much a held thing is preferred
constexpr float k_MaxGoalDistance = 200.0f;
constexpr float k_HeldPriority = 1.6f;
/// How much a thing nothing is known about is worth, and the most an action's priority counts for in a plan. Every
/// action's priority is above that cap, so in a plan they all count the same.
constexpr float k_DefaultUsefulness = 0.1f;
constexpr float k_MaxActionPriority = 0.01f;
/// Scales priorities to readable numbers; it doesn't change which plan wins
constexpr float k_PriorityScale = 1e5f;
/// How long since an action was done before it is fully new again, in game turns, and the most novelty adds
constexpr uint32_t k_NoveltyTurns = 36000;
constexpr float k_MaxNovelty = 0.01f;
/// Desires planned each game turn: two, or one once there are this many creatures in the world
constexpr size_t k_GoalsPerTurn = 2;
constexpr size_t k_CreaturesForFewerGoals = 3;
/// The desires each creature plans each game turn, by how many creatures there are
[[nodiscard]] size_t GoalsPerTurn(size_t creatureCount);

/// How the distance to a goal weighs against it: 1.6 for something held, else less the further it is, by the desire's
/// distance weight, up to 200 away
[[nodiscard]] float DistancePriority(bool held, float distance, float distanceWeight);
/// How much the creature wants to take an action, by its opinion of it from -1 to 1; nothing for an action it can't
/// take now
[[nodiscard]] float ActionPriority(float opinion, bool disabled = false);
/// A little extra for an action not done for a long time
[[nodiscard]] float Novelty(uint32_t turnsSinceDone, bool servesFirstDesire);
/// A plan's priority from its parts: the desire's strength, the goal's usefulness (or none), the action's priority, and
/// the usefulness of what is acted on and of what is used (or none)
[[nodiscard]] float Priority(float desire, std::optional<float> goalUsefulness, float actionPriority, float objectUsefulness,
                             std::optional<float> usedUsefulness);
/// Whether a plan is pressing enough to change to: more than twice the priority of what the creature carries out
[[nodiscard]] bool ShouldSwitch(float best, float current);

/// Something a desire could be satisfied with
struct ObjectCandidate
{
	/// The thing's entity number
	uint32_t id {0};
	/// How much it is worth acting on for the desire, the distance to it already counted: the goal is the one worth the
	/// most, and only something worth more than nothing can be a goal
	float usefulness {0.0f};
	/// What it counts for in the plan's priority: as what the desire is about, and as what is acted on; the default
	/// usefulness for either it doesn't count as
	std::optional<float> activityUsefulness;
	std::optional<float> actedOnUsefulness;
};

/// Whether a desire's plan counts what it acts on in its priority. A desire that is about something in itself (a town to
/// help, a creature to befriend) counts only that, but for anger, which counts both; every other desire counts what it
/// acts on.
[[nodiscard]] bool CountsActedOn(creature_desires::Desire desire, bool aboutSomething);

/// An action that satisfies a desire, by its row in the game's action table
struct ActionCandidate
{
	uint32_t action {0};
	/// Whether it acts on a thing, so needs a goal
	bool needsObject {false};
	/// Whether it uses a thing (a rock to throw, a tree to give), so needs one
	bool usesThing {false};
	float opinion {0.0f};
	uint32_t turnsSinceDone {k_NoveltyTurns};
	/// Whether the table says it serves the first desire, which the game counts for less novelty
	bool servesFirstDesire {false};
	bool disabled {false};
};

struct Plan
{
	creature_desires::Desire desire {creature_desires::Desire::Impress};
	uint32_t action {0};
	std::optional<uint32_t> object;
	/// The whole thing that the desire is about, for a desire about something: a town for compassion, the town of the
	/// villager that anger acts on
	std::optional<uint32_t> about;
	float goalUsefulness {k_DefaultUsefulness};
	float actionPriority {0.0f};
	float priority {0.0f};
	/// For a plan forced on the creature: the thing the desire is served through (a burning house's town, say); and the
	/// thing used to do it, if any, for any plan
	std::optional<uint32_t> activityObject;
	std::optional<uint32_t> instrument;
};

/// The thing that a planned action uses, and its usefulness
struct UsedChoice
{
	uint32_t id {0};
	float usefulness {0.0f};
};

/// The order in which the plan for a desire finds its action and the thing that it acts on
enum class PlanOrder : uint8_t
{
	/// The action first, by its priority and novelty, the last of equals. Then the thing that the action acts on, and the
	/// thing that it uses. When the action has no thing to act on or to use, the next action is tried.
	ActionFirst,
	/// For a desire about a whole thing: the action first, by its priority and novelty multiplied by the usefulness of
	/// the thing that it uses, the last of equals. Then the part of the whole thing that it acts on. When the whole thing
	/// has no part for the action, the next action is tried. The part counts the default usefulness.
	ActionThenPart,
	/// For anger: the part of the whole thing is chosen before the plan. The action is the one that can act on the part,
	/// by its priority and novelty multiplied by the usefulness of the part and of the thing that it uses, the last of
	/// equals.
	PartThenAction,
};

/// What the plan for a desire needs
struct DesirePlanning
{
	creature_desires::Desire desire {creature_desires::Desire::Impress};
	float strength {0.0f};
	PlanOrder order {PlanOrder::ActionFirst};
	/// The actions of the desire, in the order of the desire's list
	std::span<const ActionCandidate> actions;
	/// The usefulness of the whole thing that the desire is about, none for a desire about nothing
	std::optional<float> activityUsefulness;
	/// The thing that an action acts on, none when there is none
	std::function<std::optional<ObjectCandidate>(const ActionCandidate&)> findObject;
	/// The thing that an action uses, other than the thing that it acts on; none when there is none
	std::function<std::optional<UsedChoice>(const ActionCandidate&, std::optional<uint32_t> object)> findUsed;
};

/// The plan for a desire: the action, the thing that it acts on, and the thing that it uses. None when no action can be
/// planned. At most thirty actions are tried.
[[nodiscard]] std::optional<Plan> PlanDesire(const DesirePlanning& planning);
/// How many actions a plan tries at most
constexpr size_t k_MostActionsTried = 30;

/// One desire that a creature examines for one thing
struct DesireOnThing
{
	creature_desires::Desire desire {creature_desires::Desire::Impress};
	/// The strength of the desire. The usefulness of the thing for the desire, with its distance included.
	float strength {0.0f};
	float usefulness {0.0f};
};
/// Find the desire that the creature most wants to do about one thing. Examine the desires in their order.
/// The score of a desire is its strength multiplied by the usefulness of the thing.
/// A desire becomes the best when its score is more than the usefulness of the best desire before it.
/// That comparison does not include the strength of the best desire.
/// If no desire has a score more than zero, there is no best desire.
[[nodiscard]] std::optional<creature_desires::Desire> BestDesireOnThing(std::span<const DesireOnThing> desires);

/// Something a planned action could use
struct UsedCandidate
{
	/// The thing's entity number
	uint32_t id {0};
	float distance {0.0f};
	bool held {false};
};
/// The thing to use for a planned action, among those it may use, never the goal itself: the one the distance weighs
/// least against (something held most of all), the first of equals; none when there is none, or every one is too far to
/// count
[[nodiscard]] std::optional<UsedChoice> ChooseUsed(std::span<const UsedCandidate> objects, float distanceWeight,
                                                   std::optional<uint32_t> goal);

/// A plan that a reaction, a script or an order forces on the creature has the highest possible score. No plan that the
/// creature makes itself can replace it.
constexpr float k_ForcedScore = std::numeric_limits<float>::max();
/// A catch and a gift from the hand have a score of 1. A plan that the creature makes itself replaces them when its score
/// is more than two times this score.
constexpr float k_CatchOrGiftScore = 1.0f;

/// What the planner holds between turns
struct PlannerState
{
	/// The best plan found for each desire, the last time it was planned
	std::array<std::optional<Plan>, creature_desires::k_DesireCount> best {};
	/// The desire planned next
	size_t nextGoal {0};
	/// The plan carried out
	std::optional<Plan> current;
};
/// The next desires to plan this turn, going round those that may be planned
[[nodiscard]] std::vector<creature_desires::Desire> NextGoals(PlannerState& state,
                                                              const std::array<bool, creature_desires::k_DesireCount>& eligible,
                                                              size_t count = k_GoalsPerTurn);
/// The most pressing plan of all the desires', if any
[[nodiscard]] std::optional<Plan> Best(const PlannerState& state);
/// The plan to change to now, if the best is pressing enough against what the creature carries out (nothing counts as 0)
[[nodiscard]] std::optional<Plan> Choose(const PlannerState& state);

} // namespace openblack::creature_planner
