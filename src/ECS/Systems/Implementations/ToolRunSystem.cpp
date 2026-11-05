/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ToolRunSystem.h"

#include <span>
#include <utility>

#include <spdlog/spdlog.h>

#include "3D/TempleInteriorInterface.h"
#include "Commands/PlayerCommands.h"
#if defined(OPENBLACK_DEVELOPER_TOOLS)
#include "Common/DrawTrace.h"
#endif
#include "Common/GameRandom.h"
#include "Debug/FightAutopilotInterface.h"
#include "Debug/FightAutopilotRules.h"
#include "Debug/FrameWriteGuard.h"
#include "Debug/GuardedFrameStep.h"
#include "Debug/SimRun.h"
#include "ECS/Systems/FightTranscriptSystemInterface.h"
#include "ECS/Systems/InspectorSystemInterface.h"
#include "ECS/Systems/LandSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/SyncChecksumSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Locator.h"
#include "Simulation/ChecksumLog.h"
#include "Simulation/CommandLog.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{

/// What the guard of a frame step reads from the game's services
struct ServiceStepWorld
{
	[[nodiscard]] uint32_t Turn() const { return Locator::time::value().GetTurn(); }
	[[nodiscard]] temple::FrameWrites TakeFrameWrites() const
	{
		return Locator::temple::has_value() ? Locator::temple::value().TakeFrameWrites() : temple::FrameWrites {};
	}
	[[nodiscard]] uint32_t LandLoads() const
	{
		return Locator::landSystem::has_value() ? Locator::landSystem::value().LandLoads() : 0U;
	}
	[[nodiscard]] bool IsNetworkGame() const { return Locator::time::value().IsNetworkGame(); }
};

} // namespace

ToolRunSystem::ToolRunSystem(const std::optional<sim_run::Request>& simulation,
                             std::optional<FightAutopilotRequest> fightAutopilot,
                             std::optional<std::filesystem::path> fightTranscriptFolder)
    : _simulation(simulation)
    , _fightAutopilot(std::move(fightAutopilot))
    , _fightTranscriptFolder(std::move(fightTranscriptFolder))
{
}

ToolRunSystem::~ToolRunSystem() = default;

void ToolRunSystem::StartSimulationLogs()
{
	if (!_simulation.has_value())
	{
		return;
	}
	if (_simulation->frameGuard != sim_run::HashDetail::None)
	{
		_frameGuard = std::make_unique<frame_write_guard::Guard>(_simulation->frameGuard == sim_run::HashDetail::Fields,
		                                                         _simulation->frameGuardSystems);
	}
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	if (_simulation->traceDrawsPath.has_value())
	{
		if (!draw_trace::Start(Locator::gameRandom::value(), *_simulation->traceDrawsPath))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The file of the draw trace cannot open: {}",
			                    _simulation->traceDrawsPath->string());
		}
	}
#endif
	if (_simulation->checksumsPath.has_value())
	{
		_checksumLog = std::make_unique<checksum_log::Writer>(*_simulation->checksumsPath);
		if (_checksumLog->IsOpen())
		{
			auto& checksums = Locator::syncChecksumSystem::value();
			checksums.SetEnabled(true);
			checksums.SetKeepParts(_simulation->checksumParts != sim_run::HashDetail::None,
			                       _simulation->checksumParts == sim_run::HashDetail::Fields);
			if (_simulation->checksumDump.has_value())
			{
				checksums.SetDump(_simulation->checksumDump->first, _simulation->checksumDump->second);
			}
			checksums.SetTurnListener([log = _checksumLog.get()](const SyncChecksumSystemInterface::TurnChecksum& turn) {
				log->Write(turn.turn, turn.checksum, turn.parts, turn.unregistered);
				if (!turn.dump.empty())
				{
					log->WriteDump(turn.turn, turn.dump);
				}
			});
		}
		else
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The file of the checksums cannot open: {}",
			                    _simulation->checksumsPath->string());
		}
	}
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	if (_simulation->rowDumpPath.has_value())
	{
		// The rows come from the checksum of each turn, so the checksum is on also in a single player game
		auto& checksums = Locator::syncChecksumSystem::value();
		if (checksums.StartRowDump(*_simulation->rowDumpPath, _simulation->rowDump))
		{
			checksums.SetEnabled(true);
		}
		else
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The file of the row dump cannot open: {}",
			                    _simulation->rowDumpPath->string());
		}
	}
	if (_simulation->worldStates.has_value())
	{
		Locator::syncChecksumSystem::value().WriteWorldStates(*_simulation->worldStates);
	}
#endif
	if (_simulation->playCommandsPath.has_value())
	{
		auto turns = command_log::ReadFile(*_simulation->playCommandsPath);
		if (!turns.has_value())
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The file of the commands does not read: {}",
			                    _simulation->playCommandsPath->string());
			return;
		}
		auto& commands = Locator::playerCommandSystem::value();
		for (auto& [turn, turnCommands] : *turns)
		{
			commands.Receive(turn, std::move(turnCommands));
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Commands of {} turns to play from {}", turns->size(),
		                   _simulation->playCommandsPath->string());
	}
}

void ToolRunSystem::StartFightTools(bool networkLandPending)
{
	// A tool that steps a simulation run has each land start paused
	if (_simulation.has_value() && _simulation->startPaused && Locator::inspector::has_value())
	{
		Locator::inspector::value().PauseOnLoad(true);
	}
	// The run record of a simulation run starts with the inspector
	if (_simulation.has_value() && _simulation->recordPath.has_value() && Locator::inspector::has_value())
	{
		if (const auto error = Locator::inspector::value().StartSimRecord(*_simulation->recordPath, _simulation->recordEvery,
		                                                                  _simulation->limitsPath, _simulation->hashParts,
		                                                                  _simulation->recordFieldsEvery);
		    error.has_value())
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The simulation record cannot start: {}", *error);
		}
	}
	// A join from the command line starts the input on the server's land
	if (!networkLandPending)
	{
		StartInputTools();
	}
	// A tool's run can write the commands of each turn, for a dedicated server that does the same commands
	if (_simulation.has_value() && _simulation->commandsPath.has_value())
	{
		_commandLog = std::make_unique<command_log::Writer>(*_simulation->commandsPath);
		_scenarioGives = std::make_unique<command_log::GivesWriter>(command_log::GivesPathOf(*_simulation->commandsPath));
		if (_commandLog->IsOpen())
		{
			Locator::playerCommandSystem::value().SetTurnListener(
			    [log = _commandLog.get()](uint32_t turn, std::span<const player_commands::Command> commands) {
				    log->Write(turn, commands);
			    });
		}
		else
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The file of the commands cannot open: {}",
			                    _simulation->commandsPath->string());
		}
	}
	if (_fightTranscriptFolder.has_value() && Locator::fightTranscript::has_value())
	{
		Locator::fightTranscript::value().SetOutputFolder(_fightTranscriptFolder);
	}
	if (!_fightAutopilot.has_value())
	{
		return;
	}
	auto* autopilot = Locator::inspector::has_value() ? Locator::inspector::value().GetFightAutopilot() : nullptr;
	const auto policy = fight_autopilot::PolicyFromName(_fightAutopilot->policy);
	const auto stage = fight_autopilot::LessonStageFromName(_fightAutopilot->stage);
	const auto sequence = fight_autopilot::ParseSequence(_fightAutopilot->sequence);
	if (autopilot == nullptr || !policy.has_value() || (!_fightAutopilot->stage.empty() && !stage.has_value()) ||
	    !sequence.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The fight autopilot cannot start: check its policy, stage and sequence");
		return;
	}
	const auto error = autopilot->Start(
	    {.policy = *policy, .seed = _fightAutopilot->seed, .stage = stage, .sequence = *sequence, .enterView = true});
	if (!error.empty())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The fight autopilot cannot start: {}", error);
	}
}

void ToolRunSystem::StartInputTools()
{
	// A tool's run can record the input of the hand from the first frame of the next land, for a later run to make again
	if (_simulation.has_value() && _simulation->recordInputPath.has_value() && Locator::inspector::has_value())
	{
		Locator::inspector::value().RecordInputOnLoad(*_simulation->recordInputPath);
	}
	// A tool's run can give the hand recorded input from the first frame of the next land
	if (_simulation.has_value() && _simulation->inputPath.has_value() && Locator::inspector::has_value())
	{
		if (const auto error = Locator::inspector::value().ReplayOnLoad(*_simulation->inputPath); error.has_value())
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The recorded input cannot start: {}", *error);
		}
	}
}

void ToolRunSystem::RecordScenarioGive(uint32_t turn, size_t command)
{
	if (_scenarioGives != nullptr)
	{
		_scenarioGives->Write(turn, command);
	}
}

bool ToolRunSystem::PlayFrameStep(game_frame::Step step, const std::function<bool()>& play)
{
	ServiceStepWorld world;
	// A guard is there only in a tool's run
	const uint32_t guardFrom = _frameGuard != nullptr ? _simulation->frameGuardFrom : 0U;
	return frame_write_guard::PlayGuardedStep(_frameGuard.get(), step, guardFrom, world, play);
}

ToolRunState ToolRunSystem::GetState() const
{
	return {
	    .commandLog = _commandLog != nullptr && _commandLog->IsOpen(),
	    .scenarioGives = _scenarioGives != nullptr,
	    .checksumLog = _checksumLog != nullptr && _checksumLog->IsOpen(),
	    .frameGuard = _frameGuard != nullptr,
	    .fightAutopilot = _fightAutopilot.has_value(),
	    .testLeaveDone = _testLeaveDone,
	};
}
