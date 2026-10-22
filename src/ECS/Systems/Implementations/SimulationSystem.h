/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <utility>

#include "ECS/Systems/SimulationSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class SimulationSystem final: public SimulationSystemInterface
{
public:
	[[nodiscard]] const sim_run::Settings& GetSettings() const override { return _settings; }
	void SetSettings(const sim_run::Settings& settings) override { _settings = settings; }
	void TurnPlayed(float ms) override { _timings.AddTurn(ms); }
	void SetTurnEndWatcher(TurnEndWatcher watcher) override { _turnEnd = std::move(watcher); }
	void TurnEnded(uint32_t turn) override
	{
		if (_turnEnd)
		{
			_turnEnd(turn);
		}
	}
	void FrameEnded(float ms) override { _timings.AddFrame(ms); }
	void StagesEnded(std::span<const float> ms) override { _timings.AddStages(ms); }
	[[nodiscard]] sim_run::Timings TakeTimings() override;
	[[nodiscard]] std::optional<uint64_t> ProcessMemoryBytes() const override;

private:
	sim_run::Settings _settings {};
	sim_run::Timings _timings {};
	TurnEndWatcher _turnEnd;
};

} // namespace openblack::ecs::systems
