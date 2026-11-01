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
	/// How many things of a kind there are in all: "placed", "creatures", "villagers", "abodes" or "towns"
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
	/// 1 when a script's test for a fire near the point finds a burning thing within the radius, else 0
	FireNear,
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
	/// For Entities: the kind of thing
	std::string_view kind;
	/// For CreatureDistance, ObjectDistance and FireNear: the point, from the middle of the map, x east and y north in metres
	glm::vec2 point {0.0f};
	/// For FireNear: the radius in metres
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

/// What the game keeps of the scenario that ran last, for the inspector
struct Report
{
	std::string scenario;
	Verdict verdict {Verdict::NoChecks};
	std::vector<std::string> lines;
};

} // namespace openblack::testbed_scenarios::checks
