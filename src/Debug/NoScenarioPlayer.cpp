/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScenarioPlayer.h"

namespace openblack::testbed_scenarios
{

std::unique_ptr<ScenarioPlayer> MakeScenarioPlayer([[maybe_unused]] std::string_view id)
{
	return nullptr;
}

std::vector<std::string> ScenarioIds()
{
	return {};
}

} // namespace openblack::testbed_scenarios
