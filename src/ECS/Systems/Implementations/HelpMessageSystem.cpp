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

#include "3D/TempleInteriorInterface.h"
#include "CHLApi.h"
#include "Common/GameRandom.h"
#include "ECS/HelpQueryFacts.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/DialogueControlSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "Help/HelpProfile.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "ScriptHeaders/ScriptStart.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace messages = openblack::help_messages;

namespace
{
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

void HelpMessageSystem::SaveState(reflection::BinaryWriter& out) const
{
	reflection::Save(out, _sent);
	reflection::Save(out, _sentTurn);
	reflection::Save(out, _categoryTurn);
	reflection::Save(out, _banterUsed);
	reflection::Save(out, _lastSet);
	reflection::Save(out, _messageTurn);
	reflection::Save(out, _started);
	reflection::Save(out, _randomSeed);
}

bool HelpMessageSystem::LoadState(reflection::BinaryReader& in)
{
	reflection::Load(in, _sent);
	reflection::Load(in, _sentTurn);
	reflection::Load(in, _categoryTurn);
	reflection::Load(in, _banterUsed);
	reflection::Load(in, _lastSet);
	reflection::Load(in, _messageTurn);
	reflection::Load(in, _started);
	// A save of an older build has no seed of the help
	if (in.Remaining() > 0)
	{
		reflection::Load(in, _randomSeed);
	}
	return !in.Failed();
}

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
	vm.StartScript(name, script::start::StartableScripts());
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
	const auto texts = messages::TextsOf(set, thing, [this](int32_t n) { return HelpRand(n); });
	if (!texts.has_value())
	{
		// The script alone
		const auto name = std::string(messages::ScriptName(row.script));
		Locator::vm::value().StartScript(name, script::start::StartableScripts());
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
	// What is in the hand, else what the hand is over, else the ground under it
	std::optional<entt::entity> held;
	if (Locator::handGrabSystem::has_value())
	{
		held = Locator::handGrabSystem::value().GetHeld();
	}
	std::optional<entt::entity> underHand;
	std::optional<glm::vec3> land;
	if (Locator::pickingSystem::has_value())
	{
		const auto& pick = Locator::pickingSystem::value().GetPick();
		underHand = pick.object;
		land = pick.land;
	}
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	const auto [thing, ground] = help_query::TargetOf(held, underHand, land, inTemple);
	// The turn says the help, so that the help starts its script at the same turn at all frame rates
	if (Locator::playerCommandSystem::has_value())
	{
		Locator::playerCommandSystem::value().Submit(player_commands::HelpQuery {
		    .thing = thing.value_or(entt::null),
		    .hasGround = ground.has_value(),
		    .ground = ground.value_or(glm::vec3(0.0f)),
		});
	}
}

void HelpMessageSystem::DoHelpQuery(const player_commands::HelpQuery& query)
{
	// Not while a script's cut scene has the player's controls
	if (InScriptCutScene())
	{
		return;
	}
	if (query.thing != entt::null && Locator::entitiesRegistry::value().Valid(query.thing))
	{
		Say(help_query::About(ecs::help_query_facts::ThingOf(query.thing)), ecs::help_query_facts::TextsOf(query.thing));
		return;
	}
	const auto ground = query.hasGround ? std::optional(query.ground) : std::nullopt;
	Say(help_query::AboutGround(ecs::help_query_facts::GroundAt(ground)), std::nullopt);
}

uint32_t HelpMessageSystem::HelpRand(int32_t n)
{
	return n == 0 ? 0 : game_random::LHRand(static_cast<uint32_t>(n), _randomSeed);
}

float HelpMessageSystem::HelpFloatRand(float x)
{
	return game_random::FloatRand(x, _randomSeed);
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
		const auto set = messages::PickBanter(_sent, _banterUsed, [this](int32_t n) { return HelpRand(n); });
		RunMessageSet(set);
	}
}
