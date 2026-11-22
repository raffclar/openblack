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

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>

/// The checks of a testbed scenario: what the scenario states about its own outcome, and the verdict that tools read
/// (the exit code of a command line run, and the inspector). This is developer tooling. It reads the world and never
/// changes it.
namespace openblack::testbed_scenarios::checks
{

/// What a check reads from the world
enum class Quantity : uint8_t
{
	/// How many of the scenario's creatures are still in the world
	CreaturesAlive,
	/// How many of the scenario's objects are still in the world
	ObjectsAlive,
	/// How many things of a kind there are in all: "placed", "creatures", "villagers", "abodes", "towns", "vortices",
	/// "highlights", "street lanterns", "features", "shield miracles", "closed shield miracles", "domes", "fading domes",
	/// "standing shield reactions" or "shield fall reactions"
	Entities,
	/// A need of the scenario's creature at the index
	CreatureEnergy,
	CreatureThirst,
	CreatureExhaustion,
	CreaturePoo,
	CreatureWarmth,
	/// The size of the scenario's creature at the index
	CreatureSize,
	/// The life of the scenario's creature at the index, from 0 to 1, as a script reads its health
	CreatureLife,
	/// The distance in metres from the scenario's creature at the index to the point, on the ground
	CreatureDistance,
	/// The food and the wood that the scenario's object at the index holds together: a pile or a pot its own, a store
	/// all of its piles
	ObjectResource,
	/// The distance in metres from the scenario's object at the index to the point, on the ground
	ObjectDistance,
	/// The temperature of the scenario's object at the index
	ObjectTemperature,
	/// The life of the scenario's object at the index, from 0 to 1: a villager's or an animal's
	ObjectLife,
	/// 1 when a script's test for a fire near the point finds a burning thing within the radius, else 0
	FireNear,
	/// How many reward chests in the world have a town
	RewardChestsInATown,
	/// The kind of reaction, by its number, that the scenario's creature at the index keeps now; 0 when it keeps none
	CreatureReaction,
	/// 1 when the game draws the player's hand in this frame, else 0
	HandShown,
	/// 1 when the hand of the player holds the rope of the tied leash, else 0
	RopeHeld,
	/// 1 when the hand of the player grips the help's tip bubble, else 0
	TipBubbleGripped,
	/// 1 when the hand of the player grips the totem of a town or of a puzzle, else 0
	TotemGripped,
	/// 1 when the cursor picks the scenario's object at the index, else 0
	ObjectPicked,
	/// 1 when the hand of the player shows a tooltip, else 0
	HandToolTipShown,
	/// 1 when the hand of the player holds a miracle, else 0
	HandMiracle,
	/// The number of physical shields on the land
	PhysicalShields,
	/// 1 when the scenario's creature at the index fights its duel, else 0
	CreatureInDuel,
	/// The fight health of the scenario's creature at the index, from 0 to 1, as a script reads it
	CreatureFightHealth,
	/// The state of the match on the football pitch that is the scenario's object at the index: 0 when it waits for
	/// players, 1 when it plays, 2 when it is paused, 3 after a goal
	FootballMatchState,
	/// How many duels the scenario's creature at the index has started, as its fight record counts them
	CreatureDuels,
	/// 1 when the empty hand of the player, at the place of the scenario's object at the index, may take that object,
	/// else 0
	HandMayTake,
	/// How many lessons of a stroke or a slap the help told of the scenario's creature at the index, with a text
	CreatureLessonsTold,
	/// How many things within the radius of the point paint a footprint into the land: things in the map's cells with a
	/// model that has a footprint for the ground
	FootprintsNear,
	/// 1 when the particles keep the sphere of a shield miracle around the point on the land, else 0. A sphere that the
	/// particles keep is drawn, and it turns away the particles of other effects.
	ShieldSphereAt,
	/// 1 when the land plays by the rules of a skirmish, else 0
	SkirmishRules,
	/// 1 when the end of the skirmish or of the network game is decided, else 0
	GameDecided,
	/// 1 when the creature file of the profile on the disk knows the miracle (index, by its magic type), else 0. None
	/// when the profile has no creature file.
	ProfileCreatureKnowsMiracle,
	/// The value of a local variable (variable) of the first task of a script (kind, by its name) that runs. None when
	/// no task of the script runs.
	ScriptLocal,
	/// 1 when the scenario's creature at the index wears a leash, else 0
	CreatureLeashed,
	/// 1 when a help script or a special script of the temple has the dialogue, else 0
	TempleScriptHasDialogue,
	/// How many players of the two teams on the football pitch that is the scenario's object at the index wait for the
	/// kick-off
	FootballPlayersWaiting,
	/// The distance in metres on the ground from the ball of a football pitch to the middle of the pitch. The pitch is
	/// the scenario's object at the index.
	FootballBallFromCentre,
};

/// One statement about the world. It is read once at its turn; with byTurn it is read again at each frame from its
/// turn until it holds or its last turn goes by.
struct Check
{
	/// What the check says, in plain words, for the report
	std::string_view label;
	Quantity quantity {Quantity::CreaturesAlive};
	/// The creature or object of the scenario, for the quantities of one thing
	size_t index {0};
	/// For Entities: the kind of thing. For ScriptLocal: the name of the script
	std::string_view kind;
	/// For ScriptLocal: the name of the local variable
	std::string_view variable;
	/// For CreatureDistance, ObjectDistance, FireNear, FootprintsNear and ShieldSphereAt: the point, from the middle of the
	/// map, x east and y north in metres
	glm::vec2 point {0.0f};
	/// For FireNear and FootprintsNear: the radius in metres
	float radius {0.0f};
	/// The game turns after the scenario starts. A turn is a tenth of a second.
	uint32_t afterTurns {0};
	/// With a value: the check holds when it holds at any read from afterTurns to this turn, and fails after it
	std::optional<uint32_t> byTurn;
	/// The value holds when it is not below this and not above that
	std::optional<double> atLeast;
	std::optional<double> atMost;
};

/// The answer of a check
struct Result
{
	/// The check, by its place in the scenario's list
	size_t check {0};
	bool passed {false};
	/// The value that was read last. None when the world gave none.
	std::optional<double> observed;
	/// The turn of the last read
	uint32_t turn {0};
};

enum class Verdict : uint8_t
{
	/// The scenario states no outcome
	NoChecks,
	/// Some checks are not decided yet
	Waiting,
	Passed,
	Failed,
};

[[nodiscard]] std::string_view Name(Verdict verdict);
[[nodiscard]] std::string_view Name(Quantity quantity);

/// The exit code of a run of a scenario's checks: 0 passed, 1 failed, 2 the scenario states no checks or the verdict is
/// not decided
[[nodiscard]] int ExitCodeOf(Verdict verdict);

/// Whether a value holds for the check's limits
[[nodiscard]] bool Holds(const Check& check, double value);

/// The words of one answer, such as "creature 0 distance to the point: 3.2 (at most 4.0) at turn 120 passed"
[[nodiscard]] std::string Describe(const Check& check, const Result& result);

/// Reads a check from the world. None when the world has no such value, which fails the check.
using Probe = std::function<std::optional<double>(const Check&)>;

/// Decides the checks of a scenario as its turns go by
class Evaluator
{
public:
	/// Begins a scenario. The list must stay alive while the evaluator uses it.
	void Start(std::span<const Check> checks);
	/// Reads each check that is due at this turn, once, or at each call while it waits for its value to hold
	void Update(uint32_t turnsSinceStart, const Probe& probe);

	[[nodiscard]] Verdict GetVerdict() const;
	/// Whether every check is decided
	[[nodiscard]] bool Done() const;
	[[nodiscard]] std::span<const Result> GetResults() const { return _results; }
	[[nodiscard]] std::span<const Check> GetChecks() const { return _checks; }
	/// The report: a line for each check and the verdict
	[[nodiscard]] std::vector<std::string> Report(std::string_view scenario) const;

private:
	std::span<const Check> _checks;
	std::vector<Result> _results;
	std::vector<bool> _decided;
};

/// Counts the turns that a scenario's checks wait for. A frame in which the world plays no turn while the temple holds
/// the world adds its time as turns, so that a check on the temple's rooms and their boxes can run.
class Clock
{
public:
	/// Begins a scenario at the world's turn
	void Start(uint32_t worldTurn);
	/// Takes one frame. templeHoldsWorld is true while the player is in the temple of a game alone. When the world's
	/// turns start again from 0, as a new land loads, the count continues.
	void Advance(uint32_t worldTurn, bool templeHoldsWorld, float seconds);
	/// The turns since the start: the world's turns and the turns of the time that the temple held the world
	[[nodiscard]] uint32_t Elapsed(uint32_t worldTurn) const;

private:
	uint32_t _startTurn {0};
	uint32_t _lastTurn {0};
	double _heldSeconds {0.0};
};

/// What the game keeps of the scenario that ran last, for the inspector
struct Report
{
	std::string scenario;
	Verdict verdict {Verdict::NoChecks};
	std::vector<std::string> lines;
};

/// The report of an evaluator as it stands now. Checks that are not decided show as waiting, and the verdict is waiting
/// until all are decided.
[[nodiscard]] Report MakeReport(const Evaluator& evaluator, std::string_view scenario);

} // namespace openblack::testbed_scenarios::checks
