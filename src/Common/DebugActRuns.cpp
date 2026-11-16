/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DebugActRuns.h"

#include <stdexcept>
#include <tuple>
#include <utility>

#include "Common/DebugActs.h"
#include "Common/DeveloperToolsGate.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/SkyClockSystemInterface.h"
#include "EngineConfig.h"
#include "Help/Spirits.h"
#include "LHScriptX/Script.h"
#include "Locator.h"

namespace openblack::debug_acts
{

namespace
{

/// The world of the game for the changes of the debug menus
class GameWorld final: public WorldTarget
{
public:
	void SetTimeOfDay(float hours) override
	{
		if (Locator::skyClockSystem::has_value())
		{
			Locator::skyClockSystem::value().SetTime(hours);
		}
	}

	void SetPlayerAlignment(PlayerNames player, float alignment) override
	{
		if (Locator::alignmentSystem::has_value())
		{
			Locator::alignmentSystem::value().SetPlayerAlignment(player, alignment);
		}
	}

	void SetBonusUnlocked(size_t bonus, bool unlocked) override
	{
		// The scripts read the bonus creatures that are unlocked, thus each computer keeps the same set
		if (Locator::config::has_value())
		{
			Locator::config::value().unlockedBonusCreatures.set(bonus, unlocked);
		}
	}
};

void Submit(player_commands::Action action)
{
	if (Locator::playerCommandSystem::has_value())
	{
		Locator::playerCommandSystem::value().Submit(std::move(action));
	}
}

/// Runs the land script commands of the text. Returns the error, none when the text ran.
std::optional<std::string> RunText(const std::string& text)
{
	try
	{
		lhscriptx::Script script;
		script.Load(text);
	}
	catch (const std::runtime_error& error)
	{
		return std::string(error.what());
	}
	return std::nullopt;
}

} // namespace

bool RunAdvisorAct(const player_commands::DebugAdvisor& act)
{
	if (!Locator::advisorSystem::has_value() || !Locator::advisorSystem::value().IsLoaded())
	{
		return false;
	}
	return ApplyAdvisorAct(Locator::advisorSystem::value().GetController(), act);
}

bool RunWorldAct(const player_commands::DebugWorld& act)
{
	GameWorld world;
	return ApplyWorldAct(world, act);
}

std::optional<std::string> RunConsoleLine(const player_commands::DebugConsoleLine& line)
{
	if (const auto refusal = ConsoleLineRefusal(line))
	{
		return std::string(*refusal);
	}
	return RunText(line.text);
}

bool SendsCommands()
{
	return developer_tools::NetworkSessionLive();
}

void Do(const player_commands::DebugAdvisor& act)
{
	if (SendsCommands())
	{
		Submit(act);
		return;
	}
	std::ignore = RunAdvisorAct(act);
}

void Do(const player_commands::DebugWorld& act)
{
	if (SendsCommands())
	{
		Submit(act);
		return;
	}
	std::ignore = RunWorldAct(act);
}

std::optional<std::string> Do(const player_commands::DebugConsoleLine& line)
{
	if (SendsCommands())
	{
		if (const auto refusal = ConsoleLineRefusal(line))
		{
			return std::string(*refusal);
		}
		Submit(line);
		return std::nullopt;
	}
	// A game of this computer alone runs the line at once, as before
	return RunText(line.text);
}

} // namespace openblack::debug_acts
