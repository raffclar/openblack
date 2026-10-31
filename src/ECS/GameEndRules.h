/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>
#include <span>
#include <vector>

#include "ECS/Components/GameStanding.h"
#include "Enums.h"

// The end of a skirmish or a network game. Each turn, after the players' turns:
//
// - In a network game with winning conditions, each god gets its share of the conditions that it completed. A god that
//   completed all of them decides the game. The time limit of a network game also decides it.
// - When the game is decided, the god with the largest share wins. The share of the influence breaks a tie. The other
//   gods are out, the worst first.
// - A god among the first four players with no temple is out. Its creature goes from the land.
// - For each god that is now out, the game counts the god out. When only one god is left, the game is decided.
// - A god that the game finished with stops its computer mind. When the game is decided, every god finished.
// - Each ten turns, each god keeps its score in a history: its share of the conditions in a game with conditions, or its
//   share of the influence of the gods when no conditions are set or a god with no temple is not out yet.
//
// Only gods that a human or a computer mind plays take part. Players five to seven never lose by the temple rule.
//
// Pure, tested with a fake world.

namespace openblack::game_end
{

/// The winning conditions of a network game, by their number
enum class Condition : uint8_t
{
	VillagersKilled,
	VillagersKilledByCreature,
	VillagersBorn,
	HousesBuilt,
	WondersBuilt,
	FoodInStores,
	WoodInStores,
	TownsTakenOver,
	PrayerPowerGenerated,
	BuildingsSmashedByPlayer,
	BuildingsSmashedByCreature,
	BeliefInWorld,
	NewTownsBuilt,
	VillagersConverted,
	TreesGrown,

	_Count
};
static_assert(static_cast<size_t>(Condition::_Count) == ecs::components::GameStanding::k_ConditionCount);

/// A condition goes up by an amount. It is completed when it reaches its target.
void Add(ecs::components::WinCondition& condition, int32_t amount);
/// A condition takes a value. It is completed when the value reaches its target. A completed condition stays completed.
void Set(ecs::components::WinCondition& condition, int32_t value);

/// The share of each condition of a game of a number of conditions: none for a game with no conditions
[[nodiscard]] float ShareOfEachCondition(size_t count);
/// The share of the conditions that a god completed, and whether the god completed all of them. A god with no
/// conditions completed all of them.
struct ConditionsDone
{
	float share;
	bool all;
};
[[nodiscard]] ConditionsDone ShareOfConditions(const ecs::components::GameStanding& standing, float shareOfEach);

/// The score history of a god takes one more value. When the history is full, the values move down one place and the
/// oldest goes.
void AddScore(ecs::components::GameStanding& standing, float score);
/// The part of the score history that the game shows: the values before the place of the last value, so the last value
/// is not shown
[[nodiscard]] std::span<const float> ShownScores(const ecs::components::GameStanding& standing);
/// The score history takes a value on each tenth turn
inline constexpr uint32_t k_ScoreTurns = 10;

/// The world that the end of the game looks at and changes
class World
{
public:
	virtual ~World() = default;
	/// The players that a human or a computer mind plays, in the order of the players, without the neutral player
	[[nodiscard]] virtual std::vector<PlayerNames> ActiveGods() const = 0;
	/// A human plays the player
	[[nodiscard]] virtual bool IsHuman(PlayerNames player) const = 0;
	[[nodiscard]] virtual bool HasTemple(PlayerNames player) const = 0;
	[[nodiscard]] virtual ecs::components::GameStanding& Standing(PlayerNames player) = 0;
	/// The share of all the influence of the land that the player has
	[[nodiscard]] virtual float InfluenceShare(PlayerNames player) const = 0;
	/// The influence of the player
	[[nodiscard]] virtual float InfluencePower(PlayerNames player) const = 0;
	/// The creature of the player goes from the land, if the player has one
	virtual void RemoveCreature(PlayerNames player) = 0;
	/// The statistics of the player count one more god defeated
	virtual void CountGodDefeated(PlayerNames player) = 0;
	/// The computer mind of the player stops, if the player has one
	virtual void StopMind(PlayerNames player) = 0;
};

/// The kind of game
struct Facts
{
	bool networkGame {false};
	bool skirmish {false};
	/// The turn now, and the turn of the time limit of a network game. None for a game with no time limit.
	uint32_t turn {0};
	std::optional<uint32_t> timeLimitTurn;
	/// The number of the winning conditions of the network game
	size_t conditionCount {0};
	/// A god with no temple is out. A developer tool's run on the testbed sets this to false, as the testbed has no temples
	/// and the game would be decided at its first turn.
	bool templesNeeded {true};
};

/// What the end of the game keeps between turns
struct State
{
	/// The game is decided
	bool decided {false};
	/// The last place that a god took
	int32_t lastPlace {0};
	/// The number of the winning conditions that each god got at the start of a network game. 0: the gods did not get
	/// conditions yet, or the game has none.
	uint32_t conditionCount {0};
};

/// What happened in the turn, which the players see
struct Event
{
	enum class Kind : uint8_t
	{
		/// The god is out and the game goes on
		GodOut,
		/// The god is out and the game is decided
		GodOutAndDecided,
		/// The game is decided by its winning conditions or its time limit, and this god won
		Won,
	};
	Kind kind;
	PlayerNames god;
};

/// Whether a player takes part in the end of the game: only the first four players win or lose
[[nodiscard]] constexpr bool CanLose(PlayerNames player)
{
	return static_cast<uint8_t>(player) < 4;
}

/// A god is out: a human god gets its place. Every other human god that is not out counts one more god defeated, when
/// the god is human or the game is a skirmish.
void CountOut(World& world, State& state, PlayerNames god, ecs::components::OutReason reason, bool skirmish);

/// Whether a god has a temple, or the game needs none
[[nodiscard]] bool HasTempleOrNeedsNone(const World& world, const Facts& facts, PlayerNames god);

/// The end of the game for one turn
[[nodiscard]] std::vector<Event> ProcessTurn(World& world, const Facts& facts, State& state);

/// The number of events of a turn that decide the game. The game tells the lobby one time for each of these events.
[[nodiscard]] size_t DecidingEvents(std::span<const Event> events);

/// A human who left the network game, as the other computers tell their players
struct Leave
{
	/// The god of the human. None for an observer, which has no god.
	std::optional<PlayerNames> god;
	/// The god was out of the game before its human left. The god of the observers is never out.
	bool godOut {false};
	/// The game was decided before the human left
	bool decided {false};
};

/// What a computer shows when a human of another computer leaves the network game
struct LeaveNotice
{
	/// The advisors can say that an enemy leaves the game (a remark said at most one time a game)
	bool remark {false};
	/// A box with OK tells the name of the player who left
	bool box {false};
};

/// The notice of a leave: a human whose god was out gives no notice. Once the game is decided, the box does not show,
/// but the advisors can still say the remark.
[[nodiscard]] constexpr LeaveNotice NoticeOf(const Leave& leave) noexcept
{
	return {.remark = !leave.godOut, .box = !leave.godOut && !leave.decided};
}

} // namespace openblack::game_end
