/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <functional>
#include <optional>
#include <span>

#include "Debug/SimRun.h"

namespace openblack::ecs::systems
{

/// The fast simulation mode of a tool run, and the times of its turns and frames for the run record. The mode is off
/// by default, and then the game runs as usual.
class SimulationSystemInterface
{
public:
	virtual ~SimulationSystemInterface() = default;

	[[nodiscard]] virtual const sim_run::Settings& GetSettings() const = 0;
	virtual void SetSettings(const sim_run::Settings& settings) = 0;
	/// The game played a turn, and the turn's work took this time
	virtual void TurnPlayed(float ms) = 0;
	/// A function that the game calls after the last step of each turn, with the number of the turn after it
	using TurnEndWatcher = std::function<void(uint32_t turn)>;
	/// Sets the function that sees the end of each turn. An empty function stops the calls.
	virtual void SetTurnEndWatcher(TurnEndWatcher watcher) = 0;
	/// The game did the last step of a turn. The world is now as the turn left it, before the steps of the frame.
	virtual void TurnEnded(uint32_t turn) = 0;
	/// The frame ended, and the frame took this time
	virtual void FrameEnded(float ms) = 0;
	/// The time that each stage of the profiler took in the frame, by the stage's number
	virtual void StagesEnded(std::span<const float> ms) = 0;
	/// The times since the last call; the count starts again from none
	[[nodiscard]] virtual sim_run::Timings TakeTimings() = 0;
	/// The memory that the game's process uses, in bytes, where the system can tell it
	[[nodiscard]] virtual std::optional<uint64_t> ProcessMemoryBytes() const = 0;
};

} // namespace openblack::ecs::systems
