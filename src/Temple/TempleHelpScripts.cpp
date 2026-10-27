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
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/DialogueControlSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "Help/Spirits.h"
#include "Locator.h"
#include "ScriptHeaders/ScriptStart.h"

namespace openblack::temple_help
{

void GameScripts::Start(std::string_view name)
{
	if (!Locator::vm::has_value())
	{
		return;
	}
	Locator::vm::value().StartScript(std::string(name), script::start::StartableScripts());
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

void GameDialogue::SendAdvisorsHome()
{
	if (!Locator::advisorSystem::has_value() || !Locator::advisorSystem::value().IsLoaded())
	{
		return;
	}
	auto& advisors = Locator::advisorSystem::value().GetController();
	advisors.SpiritHome(2, false);
	advisors.SpiritHome(1, false);
}

bool GameDialogue::IsControlled() const
{
	if (!Locator::dialogueControlSystem::has_value())
	{
		return false;
	}
	const auto wideScreenOwner =
	    Locator::cinematicDirectorSystem::has_value() ? Locator::cinematicDirectorSystem::value().GetWideScreenOwner() : 0u;
	return Locator::dialogueControlSystem::value().IsControlled(wideScreenOwner);
}

uint32_t GameDialogue::GetOwner() const
{
	return Locator::dialogueControlSystem::has_value() ? Locator::dialogueControlSystem::value().GetOwner() : 0u;
}

bool GameDialogue::IsTempleScript(uint32_t task) const
{
	if (!Locator::vm::has_value())
	{
		return false;
	}
	const auto& tasks = Locator::vm::value().GetTasks();
	const auto found = tasks.find(task);
	if (found == tasks.end())
	{
		return false;
	}
	return found->second.type & (lhvm::ScriptType::TempleHelp | lhvm::ScriptType::TempleSpecial);
}

void GameDialogue::GiveTo(uint32_t task)
{
	if (!Locator::dialogueControlSystem::has_value())
	{
		return;
	}
	const auto wideScreenOwner =
	    Locator::cinematicDirectorSystem::has_value() ? Locator::cinematicDirectorSystem::value().GetWideScreenOwner() : 0u;
	Locator::dialogueControlSystem::value().Request(task, wideScreenOwner);
}

void GameDialogue::TakeAway()
{
	if (Locator::dialogueControlSystem::has_value())
	{
		Locator::dialogueControlSystem::value().Reset();
	}
	if (Locator::helpTextSystem::has_value())
	{
		Locator::helpTextSystem::value().CloseTextBox();
	}
}

help::DialogueText::Held GameDialogue::HoldText() const
{
	return Locator::helpTextSystem::has_value() ? Locator::helpTextSystem::value().HoldText() : help::DialogueText::Held {};
}

void GameDialogue::RestoreText(const help::DialogueText::Held& held)
{
	if (Locator::helpTextSystem::has_value())
	{
		Locator::helpTextSystem::value().RestoreText(held);
	}
}

void GameDialogue::ShowTextAgain()
{
	if (Locator::helpTextSystem::has_value())
	{
		Locator::helpTextSystem::value().ShowTextAgain();
	}
}

bool HelpSystemOn()
{
	return Locator::chlapi::has_value() && Locator::chlapi::value().IsHelpSystemOn();
}

} // namespace openblack::temple_help
