/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <filesystem>
#include <memory>
#include <optional>

#include "ECS/Systems/ToolRunSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp"
#endif

namespace openblack
{
namespace sim_run
{
struct Request;
}
namespace command_log
{
class Writer;
class GivesWriter;
} // namespace command_log
namespace checksum_log
{
class Writer;
}
namespace frame_write_guard
{
class Guard;
}
} // namespace openblack

namespace openblack::ecs::systems
{

class ToolRunSystem final: public ToolRunSystemInterface
{
public:
	/// simulation: the request of a tool's run, which the caller keeps for the life of the system
	ToolRunSystem(const std::optional<sim_run::Request>& simulation, std::optional<FightAutopilotRequest> fightAutopilot,
	              std::optional<std::filesystem::path> fightTranscriptFolder);
	~ToolRunSystem() override;
	ToolRunSystem(const ToolRunSystem&) = delete;
	ToolRunSystem& operator=(const ToolRunSystem&) = delete;
	ToolRunSystem(ToolRunSystem&&) = delete;
	ToolRunSystem& operator=(ToolRunSystem&&) = delete;

	void StartSimulationLogs() final;
	void StartFightTools(bool networkLandPending) final;
	void StartInputTools() final;
	void RecordScenarioGive(uint32_t turn, size_t command) final;
	[[nodiscard]] bool PlayFrameStep(game_frame::Step step, const std::function<bool()>& play) final;

	[[nodiscard]] bool IsTestLeaveDone() const final { return _testLeaveDone; }
	void SetTestLeaveDone() final { _testLeaveDone = true; }

	[[nodiscard]] ToolRunState GetState() const final;

private:
	const std::optional<sim_run::Request>& _simulation;
	std::optional<FightAutopilotRequest> _fightAutopilot;
	std::optional<std::filesystem::path> _fightTranscriptFolder;
	/// The file that a tool's run writes the commands of each turn to
	std::unique_ptr<command_log::Writer> _commandLog;
	/// The file that a tool's run writes the commands of the testbed scenario to, with the turn before which each went
	std::unique_ptr<command_log::GivesWriter> _scenarioGives;
	/// The file that a tool's run writes the checksum of each turn to
	std::unique_ptr<checksum_log::Writer> _checksumLog;
	/// A tool's run that looks for the frame steps that change the synced world
	std::unique_ptr<frame_write_guard::Guard> _frameGuard;
	bool _testLeaveDone {false};
};

} // namespace openblack::ecs::systems
