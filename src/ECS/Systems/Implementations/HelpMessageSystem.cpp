/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "HelpMessageSystem.h"

#include <string>
#include <variant>

#include <LHVM.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "Common/GameRandom.h"
#include "ECS/HelpQueryFacts.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/DialogueControlSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "Help/HelpProfile.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace messages = openblack::help_messages;

namespace
{
/// The kinds of script a single player game starts by name
constexpr auto k_SinglePlayerScripts = static_cast<lhvm::ScriptType>(0x7f);
constexpr size_t k_StartedKept = 16;

uint32_t Turn()
{
	return Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
}

/// A new help message may start: nothing holds the dialogue, or a help script does, which is stopped with the other
/// help scripts; a story script holding it keeps it
bool StopRunningHelp()
{
	if (!Locator::vm::has_value())
	{
		return false;
	}
	auto& vm = Locator::vm::value();
	const auto owner = Locator::dialogueControlSystem::has_value() ? Locator::dialogueControlSystem::value().GetOwner() : 0u;
	if (owner != 0)
	{
		const auto& tasks = vm.GetTasks();
		const auto found = tasks.find(owner);
		const auto type = found != tasks.end() ? found->second.type : lhvm::ScriptType::None;
		if (!(type & (lhvm::ScriptType::Help | lhvm::ScriptType::MultiplayerHelp)))
		{
			return false;
		}
	}
	vm.StopTasksOfType(lhvm::ScriptType::Help | lhvm::ScriptType::TempleHelp | lhvm::ScriptType::MultiplayerHelp);
	return true;
}

uint32_t LocalRand(int32_t n)
{
	return Locator::gameRandom::has_value() ? Locator::gameRandom::value().LocalRand(n) : 0;
}

/// A script's cut scene: a script task has the cinema bars in
bool InScriptCutScene()
{
	if (!Locator::cinematicDirectorSystem::has_value())
	{
		return false;
	}
	const auto& director = Locator::cinematicDirectorSystem::value();
	return director.IsWideScreenOn() && director.GetWideScreenOwner() != 0;
}
} // namespace

void HelpMessageSystem::Remember(Started started)
{
	_started.push_back(std::move(started));
	if (_started.size() > k_StartedKept)
	{
		_started.erase(_started.begin());
	}
}

bool HelpMessageSystem::RunMessage(uint32_t first, uint32_t last, messages::Script script,
                                   std::optional<std::pair<uint32_t, float>> townNeed)
{
	if (first > last || !StopRunningHelp())
	{
		return false;
	}
	_messageTurn = Turn();
	auto& vm = Locator::vm::value();
	if (townNeed.has_value())
	{
		vm.Pushf(static_cast<float>(townNeed->first));
		vm.Pushf(townNeed->second);
	}
	vm.Pushf(static_cast<float>(first));
	vm.Pushf(static_cast<float>(last));
	const auto name = std::string(messages::ScriptName(script));
	vm.StartScript(name, k_SinglePlayerScripts);
	Remember({.script = name,
	          .first = first,
	          .last = last,
	          .statText = townNeed.has_value() ? std::optional(townNeed->first) : std::nullopt,
	          .need = townNeed.has_value() ? std::optional(townNeed->second) : std::nullopt,
	          .turn = _messageTurn});
	return true;
}

bool HelpMessageSystem::RunMessageSet(size_t set, std::optional<messages::Texts> thing)
{
	if (set >= messages::k_SetCount || !StopRunningHelp())
	{
		return false;
	}
	const auto& row = messages::Set(set);
	const auto turn = Turn();
	// The set counts as said: its turn and its kind's, its count, and the help's count of help given
	_sentTurn.at(set) = turn;
	if (row.category < _categoryTurn.size())
	{
		_categoryTurn.at(row.category) = turn;
	}
	++_sent.at(set);
	_lastSet = set;
	_messageTurn = turn;
	if (Locator::helpProfileSystem::has_value())
	{
		Locator::helpProfileSystem::value().Trigger(messages::k_SetSaidEvent);
	}
	const auto texts = messages::TextsOf(set, thing, LocalRand);
	if (!texts.has_value())
	{
		// The script alone
		const auto name = std::string(messages::ScriptName(row.script));
		Locator::vm::value().StartScript(name, k_SinglePlayerScripts);
		Remember({.script = name, .first = 0, .last = 0, .turn = turn});
		return true;
	}
	RunMessage(texts->first, texts->last, row.script);
	return true;
}

void HelpMessageSystem::TriggerCategory(uint32_t category)
{
	if (category < _categoryTurn.size())
	{
		_categoryTurn.at(category) = Turn();
	}
}

void HelpMessageSystem::HelpQuery()
{
	// Not while a script's cut scene has the player's controls
	if (InScriptCutScene())
	{
		return;
	}
	// What is in the hand, else what the hand is over, else the ground under it
	std::optional<entt::entity> thing;
	if (Locator::handGrabSystem::has_value())
	{
		thing = Locator::handGrabSystem::value().GetHeld();
	}
	std::optional<glm::vec3> ground;
	if (Locator::pickingSystem::has_value())
	{
		const auto& pick = Locator::pickingSystem::value().GetPick();
		if (!thing.has_value())
		{
			thing = pick.object;
		}
		ground = pick.land;
	}
	if (thing.has_value() && Locator::entitiesRegistry::value().Valid(*thing))
	{
		Say(help_query::About(ecs::help_query_facts::ThingOf(*thing)), ecs::help_query_facts::TextsOf(*thing));
		return;
	}
	Say(help_query::AboutGround(ecs::help_query_facts::GroundAt(ground)), std::nullopt);
}

void HelpMessageSystem::Say(const help_query::Answer& answer, std::optional<messages::Texts> thing)
{
	if (const auto* set = std::get_if<help_query::SaySet>(&answer))
	{
		RunMessageSet(set->set, thing);
		return;
	}
	const auto& say = std::get<help_query::SayTexts>(answer);
	RunMessage(say.texts.first, say.texts.last, say.script, say.townNeed);
}

void HelpMessageSystem::ProcessTurn()
{
	if (!Locator::chlapi::has_value() || !Locator::chlapi::value().IsHelpSystemOn() || !Locator::helpProfileSystem::has_value())
	{
		return;
	}
	auto& profile = Locator::helpProfileSystem::value().Get();
	const auto since = profile.Count(messages::k_AnyHelpEvent).SecondsSince(profile.Clock());
	if (messages::BanterDue(since, InScriptCutScene()))
	{
		const auto set = messages::PickBanter(_sent, _banterUsed, LocalRand);
		RunMessageSet(set);
	}
}
