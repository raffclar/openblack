/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleHelpScripts.h"

#include <string>

#include <LHVM.h>

#include "CHLApi.h"
#include "Locator.h"

namespace openblack::temple_help
{

void GameScripts::Start(std::string_view name)
{
	if (!Locator::vm::has_value())
	{
		return;
	}
	// The kinds of script a single player game starts by name
	constexpr auto k_SinglePlayerScripts = static_cast<lhvm::ScriptType>(0x7f);
	Locator::vm::value().StartScript(std::string(name), k_SinglePlayerScripts);
}

void GameScripts::StopHelp()
{
	if (!Locator::vm::has_value())
	{
		return;
	}
	Locator::vm::value().StopTasksOfType(lhvm::ScriptType::Help | lhvm::ScriptType::TempleHelp |
	                                     lhvm::ScriptType::MultiplayerHelp);
}

void GameScripts::Stop(std::string_view name)
{
	if (!Locator::vm::has_value())
	{
		return;
	}
	Locator::vm::value().StopScripts([name](const std::string& script, const std::string&) { return script == name; });
}

bool HelpSystemOn()
{
	return Locator::chlapi::has_value() && Locator::chlapi::value().IsHelpSystemOn();
}

} // namespace openblack::temple_help
