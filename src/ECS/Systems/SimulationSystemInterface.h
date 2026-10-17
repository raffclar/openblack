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

#include <optional>

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
	/// The frame ended, and the frame took this time
	virtual void FrameEnded(float ms) = 0;
	/// The times since the last call; the count starts again from none
	[[nodiscard]] virtual sim_run::Timings TakeTimings() = 0;
	/// The memory that the game's process uses, in bytes, where the system can tell it
	[[nodiscard]] virtual std::optional<uint64_t> ProcessMemoryBytes() const = 0;
};

} // namespace openblack::ecs::systems
