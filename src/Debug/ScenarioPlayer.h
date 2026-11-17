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

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Simulation/CommandLog.h"

namespace openblack::testbed_scenarios
{

/// A testbed scenario that a program plays without a window of scenarios, as the dedicated server does. The scenario
/// code is only in a build with the developer tools. A build without them makes no player, so the program does not
/// contain the scenarios.
class ScenarioPlayer
{
public:
	virtual ~ScenarioPlayer() = default;

	/// The commands of the scenario go out before the turns in which the game that wrote them gave them
	virtual void FollowGives(std::vector<command_log::Give> gives) = 0;
	/// Gives the commands of the scenario that are due before the next turn
	virtual void BeforeTurn() = 0;
	/// Lays the scenario out, then gives the commands, throws and rewards that are due at its start
	virtual void Start() = 0;
};

/// The player of the scenario of this id. None when the id is not a scenario, and always none in a build without the
/// developer tools.
[[nodiscard]] std::unique_ptr<ScenarioPlayer> MakeScenarioPlayer(std::string_view id);

/// The ids of all the scenarios, in their order. None in a build without the developer tools.
[[nodiscard]] std::vector<std::string> ScenarioIds();

/// Why a network game cannot start again on the scenario of this id for the editor, or none when it can. The scenario
/// must be in the registry, and every computer must make its land from its own game folder: no scenario on the map for
/// 32 gods, whose file only the server makes. A build without the developer tools has no scenario.
[[nodiscard]] std::optional<std::string> NetworkRestartRefusal(std::string_view id);

} // namespace openblack::testbed_scenarios
