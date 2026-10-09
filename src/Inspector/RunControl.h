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
#include <string>
#include <string_view>
#include <vector>

#include <InspectorProvider.h>

namespace openblack::inspector
{

/// Stepping the game by frames or by turns while it is held paused. Told each frame whether the game is paused and
/// its turn, it says whether the game should be paused or running for the frame.
class RunControl
{
public:
	/// Stops any stepping
	void Cancel();
	/// Runs the game for this many frames from the next, then pauses it
	void StepFrames(uint32_t frames);
	/// Runs the game until this many more turns have been played, then pauses it
	void StepTurns(uint32_t turns, uint32_t currentTurn);

	/// Once a frame before the game's turn: whether the game is to be paused (true) or to run (false) this frame,
	/// none to leave it as it is
	[[nodiscard]] std::optional<bool> Frame(bool paused, uint32_t turn);

	[[nodiscard]] bool Stepping() const { return _framesLeft.has_value() || _untilTurn.has_value(); }
	[[nodiscard]] std::optional<uint32_t> FramesLeft() const { return _framesLeft; }
	[[nodiscard]] std::optional<uint32_t> UntilTurn() const { return _untilTurn; }

private:
	std::optional<uint32_t> _framesLeft;
	std::optional<uint32_t> _untilTurn;
};

/// A testbed scenario as the inspector lists it
struct ScenarioSummary
{
	std::string id;
	std::string name;
	std::string facet;
	std::string description;
};

/// What the inspector's run control drives: the game's clock and its scenarios
class RunTargetInterface
{
public:
	virtual ~RunTargetInterface() = default;
	[[nodiscard]] virtual bool IsPaused() const = 0;
	virtual void SetPaused(bool paused) = 0;
	[[nodiscard]] virtual uint32_t GetTurn() const = 0;
	/// The game's speed: 1 is normal, 2 twice as fast
	[[nodiscard]] virtual float GetSpeed() const = 0;
	virtual void SetSpeed(float speed) = 0;
	/// Each frame taking a fixed time in milliseconds, for deterministic stepping, or the wall clock's time again
	virtual void SetFixedFrameTime(std::optional<uint32_t> milliseconds) = 0;
	[[nodiscard]] virtual std::optional<uint32_t> GetFixedFrameTime() const = 0;
	/// The lock on the player's mouse and keyboard, as input.state gives it
	[[nodiscard]] virtual Json InputLock() const { return nullptr; }
	/// Starts a testbed scenario on a fresh testbed by its id: false if there is no such scenario
	virtual bool LoadScenario(std::string_view id) = 0;
	[[nodiscard]] virtual std::vector<ScenarioSummary> Scenarios() const = 0;
};

///   game.state                       paused, turn, frame, speed and any stepping
///   game.pause / game.resume         and the state after
///   game.step     {frames | turns}   runs that many frames or turns, then pauses
///   game.speed    {speed}            1 is normal, 2 twice as fast
///   game.frame_time {ms}             each frame takes this long (0 for the wall clock's time), for deterministic runs
///   game.scenario {id}               loads a testbed scenario on a fresh testbed
///   game.scenarios                   the testbed scenarios there are
class GameProvider final: public ProviderInterface
{
public:
	static constexpr float k_SlowestSpeed = 0.1f;
	static constexpr float k_FastestSpeed = 16.0f;

	explicit GameProvider(RunTargetInterface& target);

	[[nodiscard]] std::string_view Name() const override { return "game"; }
	[[nodiscard]] std::vector<QueryDescription> Describe() const override;
	[[nodiscard]] QueryResult Run(std::string_view query, const QueryContext& context) override;

	/// Once a frame before the game's turn: counts the frame and holds or releases the game for the stepping
	void Frame();

	[[nodiscard]] Json State() const;
	/// The frames served so far
	[[nodiscard]] uint64_t FrameNumber() const { return _frame; }

private:
	RunTargetInterface& _target;
	RunControl _control;
	/// The frame time to go back to once a step with a fixed frame time ends
	std::optional<std::optional<uint32_t>> _frameTimeAfterStep;
	uint64_t _frame {0};
};

} // namespace openblack::inspector
