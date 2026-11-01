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

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

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
	virtual void FollowGives(std::map<uint32_t, std::vector<size_t>> gives) = 0;
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

} // namespace openblack::testbed_scenarios
