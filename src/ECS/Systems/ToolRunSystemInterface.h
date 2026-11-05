/*******************************************************************************
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
#include <string>

#include "GameFrameOrder.h"

namespace openblack
{

/// The fight autopilot asked for on the command line, as the command line gives it: its policy, the seed, the lesson stage
/// to start at and the moves of a sequence
struct FightAutopilotRequest
{
	std::string policy;
	uint32_t seed {1};
	std::string stage;
	std::string sequence;
};

} // namespace openblack

namespace openblack::ecs::systems
{

/// Which logs and tools of a tool's run are open
struct ToolRunState
{
	/// The file of the commands of each turn
	bool commandLog {false};
	/// The file of the commands that the testbed scenario gives
	bool scenarioGives {false};
	/// The file of the checksum of each turn
	bool checksumLog {false};
	/// The guard that finds the frame steps that change the synced world
	bool frameGuard {false};
	/// The command line asks for the fight autopilot
	bool fightAutopilot {false};
	/// The player left the network game at the turn that the command line gives
	bool testLeaveDone {false};
};

/// The logs and tools of a tool's run on this computer, as the command line asks for them: the commands of each turn, the
/// commands that the testbed scenario gives, the checksum of each turn, the guard of the frame steps, the recorded input
/// and the fight autopilot. The turns of the world do not read it, and the dedicated server does not make it.
class ToolRunSystemInterface
{
public:
	virtual ~ToolRunSystemInterface() = default;

	/// The checksum of each turn and the commands to play of a tool's run, from the command line. These need no inspector.
	virtual void StartSimulationLogs() = 0;
	/// Once the inspector has started: the folder of the fight transcript and the fight autopilot from the command line.
	/// networkLandPending: a join from the command line waits for the land of the game server, which starts the input.
	virtual void StartFightTools(bool networkLandPending) = 0;
	/// The recorded input of a tool's run and the record of the input, from the first frame of the next land
	virtual void StartInputTools() = 0;
	/// A run that writes the commands of each turn also writes each command of the testbed scenario that the game gives
	/// before a turn, so that a dedicated server gives it before the same turn (command_log::GivesPathOf)
	virtual void RecordScenarioGive(uint32_t turn, size_t command) = 0;
	/// Plays one step of the frame. The guard of a tool's run looks at the step, if the step is not the turns.
	[[nodiscard]] virtual bool PlayFrameStep(game_frame::Step step, const std::function<bool()>& play) = 0;

	/// Tests: the player left the network game at the turn that the command line gives
	[[nodiscard]] virtual bool IsTestLeaveDone() const = 0;
	virtual void SetTestLeaveDone() = 0;

	[[nodiscard]] virtual ToolRunState GetState() const = 0;
};

} // namespace openblack::ecs::systems
