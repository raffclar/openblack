/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureHelpSystem.h"

#include <array>
#include <string>

#include <LHVM.h>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "Creature/CreatureLearning.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureHelp.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/DialogueControlSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace help = openblack::creature_help;
using creature_desires::Desire;

namespace
{
constexpr uint32_t k_TurnsPerSecond =
    static_cast<uint32_t>(std::chrono::milliseconds(std::chrono::seconds(1)) / TimeSystemInterface::k_TurnDuration);
/// The messages kept for the inspector
constexpr size_t k_ToldKept = 16;
/// The kinds of script a single player game starts by name
constexpr auto k_SinglePlayerScripts = static_cast<lhvm::ScriptType>(0x7f);

uint32_t Turn()
{
	return Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
}

/// The help talks only of the local player's creature: their first, as everything about "the player's creature" does
bool IsPlayersCreature(entt::entity creature)
{
	if (!Locator::playerSystem::has_value())
	{
		return false;
	}
	const auto& players = Locator::playerSystem::value();
	return players.GetPrimaryCreature(players.GetLocalPlayer()) == creature;
}

/// A script holds the dialogue, or has the cinema bars in
bool DialogueTaken()
{
	if (!Locator::dialogueControlSystem::has_value())
	{
		return false;
	}
	const auto owner =
	    Locator::cinematicDirectorSystem::has_value() ? Locator::cinematicDirectorSystem::value().GetWideScreenOwner() : 0u;
	return Locator::dialogueControlSystem::value().IsControlled(owner);
}

bool InsideTemple()
{
	return Locator::temple::has_value() && Locator::temple::value().Active();
}

bool InView(const ecs::Registry& registry, entt::entity creature)
{
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (transform == nullptr || !Locator::camera::has_value())
	{
		return false;
	}
	const auto& camera = Locator::camera::value();
	return help::LooksAt(camera.GetOrigin(), camera.GetFocus(), transform->position);
}

/// Fainted or asleep: the game's plans of dying and of sleeping, which hold back the help
bool OutOrAsleep(const ecs::Registry& registry, entt::entity creature)
{
	if (const auto* mind = registry.TryGet<const CreatureMindState>(creature); mind != nullptr)
	{
		const auto activity = mind->idle.activity;
		if (activity == creature_mind::Activity::Sleep || activity == creature_mind::Activity::Faint)
		{
			return true;
		}
	}
	if (const auto* needs = registry.TryGet<const CreatureNeeds>(creature); needs != nullptr)
	{
		return needs->rest == CreatureNeeds::Rest::Asleep || needs->rest == CreatureNeeds::Rest::Unconscious;
	}
	return false;
}

const GCreatureInfo* SpeciesInfo(const ecs::Registry& registry, entt::entity creature)
{
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr || !Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	const auto& creatures = Locator::infoConstants::value().creature;
	const auto row = creature::InfoRow(body->species);
	return row < creatures.size() ? &creatures.at(row) : nullptr;
}

help::BodyThresholds ThresholdsOf(const GCreatureInfo* info)
{
	if (info == nullptr)
	{
		return {};
	}
	return {
	    .seconds = info->helpBodyProblemSeconds,
	    .lowEnergy = info->helpLowEnergy,
	    .veryLowEnergy = info->helpVeryLowEnergy,
	    .lowLife = info->helpLowLife,
	    .veryLowLife = info->helpVeryLowLife,
	    .tired = info->helpTired,
	    .veryTired = info->helpVeryTired,
	    .thirsty = info->helpThirsty,
	    .veryThirsty = info->helpVeryThirsty,
	};
}

std::optional<help::Body> BodyOf(const ecs::Registry& registry, entt::entity creature)
{
	const auto* needs = registry.TryGet<const CreatureNeeds>(creature);
	if (needs == nullptr || !needs->started)
	{
		return std::nullopt;
	}
	return help::Body {
	    .energy = needs->needs.energy,
	    .life = needs->needs.life,
	    .exhaustion = needs->needs.exhaustion,
	    .dehydration = needs->needs.dehydration,
	};
}

/// A miracle's own text of the creature having learnt it, or nearly
std::optional<uint32_t> MiracleText(help::Kind kind, uint32_t miracle)
{
	if (!Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	const auto& effects = Locator::infoConstants::value().magicEffect;
	if (miracle >= effects.size())
	{
		return std::nullopt;
	}
	const auto& effect = effects.at(miracle);
	return static_cast<uint32_t>(kind == help::Kind::LessonMagicAction ? effect.creatureLearntEnum
	                                                                   : effect.creatureNearlyLearntEnum);
}

/// The help script that says a message's text, with the text's number as its argument
void Say(std::string_view script, uint32_t text)
{
	if (!Locator::vm::has_value())
	{
		return;
	}
	auto& vm = Locator::vm::value();
	vm.Pushf(static_cast<float>(text));
	vm.StartScript(std::string(script), k_SinglePlayerScripts);
}

help::Memory& MemoryOf(ecs::Registry& registry, entt::entity creature)
{
	if (auto* help = registry.TryGet<CreatureHelpMemory>(creature); help != nullptr)
	{
		return help->memory;
	}
	return registry.Assign<CreatureHelpMemory>(creature).memory;
}
} // namespace

void CreatureHelpSystem::Raise(entt::entity creature, const help::Message& message)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.AllOf<Creature>(creature))
	{
		return;
	}
	const auto turn = Turn();
	auto& memory = MemoryOf(registry, creature);
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	const help::Situation situation {
	    .helpOn = _helpOn,
	    .insideTemple = InsideTemple(),
	    .playersCreature = IsPlayersCreature(creature),
	    .stage = mind != nullptr ? mind->developmentPhase : help::k_HelpStage,
	    .outOrAsleep = OutOrAsleep(registry, creature),
	    .dialogueTaken = DialogueTaken(),
	    .handOnCreature =
	        Locator::creatureHandSystem::has_value() && Locator::creatureHandSystem::value().GetCreature() == creature,
	    .raisedThisTurn = _toldTurn == turn,
	    .scriptControlled = registry.AllOf<ScriptControlled>(creature),
	    .inDevelopmentScript = mind != nullptr && mind->inDevelopmentScript,
	    .inView = InView(registry, creature),
	};
	const auto toldLately = help::ToldLately(memory, message.kind, message.parameter, turn, k_TurnsPerSecond);
	switch (help::Decide(message, situation, toldLately))
	{
	case help::Outcome::Tell:
		help::MarkTold(memory, message.kind, message.parameter, turn);
		_toldTurn = turn;
		Tell(creature, message);
		break;
	case help::Outcome::Stack:
		help::Stack(memory, message, turn, k_TurnsPerSecond);
		break;
	case help::Outcome::Drop:
		break;
	}
}

void CreatureHelpSystem::Tell(entt::entity creature, const help::Message& message)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& info = help::Info(message.kind);
	std::optional<uint32_t> text;
	switch (info.telling)
	{
	case help::Telling::Nothing:
	case help::Telling::Unsaid:
		break;
	case help::Telling::StageScript:
	{
		const auto land = static_cast<uint32_t>(registry.Context().mapScriptGlobals.landNumber);
		if (const auto script = help::StageScript(message.kind, land); script.has_value() && Locator::vm::has_value())
		{
			Locator::vm::value().StartScript(std::string(*script), k_SinglePlayerScripts);
			if (auto* mind = registry.TryGet<CreatureMindState>(creature); mind != nullptr)
			{
				mind->inDevelopmentScript = true;
			}
		}
		break;
	}
	case help::Telling::PointAtCreature:
	case help::Telling::JustText:
	{
		const bool miracle =
		    message.kind == help::Kind::LessonMagicAction || message.kind == help::Kind::NearlyLearntMagicAction;
		text =
		    help::Text(message.kind, message.parameter, miracle ? MiracleText(message.kind, message.parameter) : std::nullopt);
		if (text.has_value())
		{
			Say(info.telling == help::Telling::PointAtCreature ? "HelpForCreaturePointAtCreature" : "HelpForCreatureJustText",
			    *text);
		}
		break;
	}
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature help: kind {} parameter {} text {}", static_cast<int>(message.kind),
	                    message.parameter, text.value_or(0));
	_told.push_back({.creature = creature, .kind = message.kind, .parameter = message.parameter, .text = text, .turn = Turn()});
	if (_told.size() > k_ToldKept)
	{
		_told.erase(_told.begin());
	}
}

void CreatureHelpSystem::ProcessTurn()
{
	if (!Locator::playerSystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	const auto& players = Locator::playerSystem::value();
	const auto creature = players.GetPrimaryCreature(players.GetLocalPlayer());
	if (creature.has_value() && Locator::entitiesRegistry::value().Valid(*creature))
	{
		UpdateCreature(*creature);
	}
}

void CreatureHelpSystem::UpdateCreature(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	// A creature a script controls thinks nothing of its own, help included
	if (mind == nullptr || !mind->desires.has_value() || registry.AllOf<ScriptControlled>(creature))
	{
		return;
	}
	const auto turn = Turn();

	// The first waiting message is raised again, unless a script holds the dialogue; whatever becomes of it, it is done
	// waiting
	if (!DialogueTaken())
	{
		if (const auto waiting = help::FirstWaiting(MemoryOf(registry, creature)); waiting.has_value())
		{
			const auto entry = MemoryOf(registry, creature).entries.at(static_cast<size_t>(waiting->kind)).at(waiting->index);
			Raise(creature,
			      {.kind = waiting->kind, .parameter = entry.parameter, .thing = entry.thing, .position = entry.position});
			help::Settle(MemoryOf(registry, creature), *waiting, turn);
		}
	}

	[&]() {
		const auto* info = SpeciesInfo(registry, creature);
		auto& memory = MemoryOf(registry, creature);
		// A problem with its body, at most every two minutes
		if (const auto body = BodyOf(registry, creature); body.has_value())
		{
			const auto since = help::WholeSeconds(turn, memory.physicalProblemTurn, k_TurnsPerSecond);
			if (const auto problem = help::BodyProblemNow(*body, ThresholdsOf(info), since); problem.has_value())
			{
				Raise(creature, {.kind = help::Kind::PhysicalProblem, .parameter = static_cast<uint32_t>(*problem)});
				MemoryOf(registry, creature).physicalProblemTurn = turn;
				return;
			}
		}
		// The hand kept over it, it is told what the creature wants and why
		const auto& desires = *mind->desires;
		if (memory.handOver >= help::k_HandOverToTell && mind->planner.current.has_value() && !DialogueTaken())
		{
			const auto desire = mind->planner.current->desire;
			const auto& state = desires[desire];
			const auto source = creature_learning::DominantSource(state);
			const auto type = source.has_value() ? state.sources.at(*source).type : creature_learning::k_NoTellingSource;
			if (type != creature_learning::k_NoTellingSource)
			{
				Raise(creature, {.kind = help::Kind::CurrentDesire, .parameter = static_cast<uint32_t>(desire)});
			}
			return;
		}
		// A prompt on how to look after it, at most every two minutes
		std::array<float, creature_desires::k_DesireCount> values {};
		std::array<bool, creature_desires::k_DesireCount> active {};
		for (size_t i = 0; i < creature_desires::k_DesireCount; ++i)
		{
			values.at(i) = desires.desires.at(i).value;
			active.at(i) = desires.desires.at(i).activated;
		}
		const auto dominant = static_cast<Desire>(help::DominantDesire(values, active));
		const auto applies = help::TutorialApplies({
		    .stage = mind->developmentPhase,
		    .wantsToBuildHome = dominant == Desire::BuildHome,
		    .wantsWater = dominant == Desire::Water,
		    .wantsToPoo = dominant == Desire::Poo,
		    .carrying = Locator::creatureObjectActionSystem::has_value() &&
		                Locator::creatureObjectActionSystem::value().GetHeld(creature).has_value(),
		});
		const auto seconds = info != nullptr ? info->helpTutorialSeconds : help::BodyThresholds {}.seconds;
		if (const auto prompt = help::TutorialNow(memory, applies, turn, k_TurnsPerSecond, seconds); prompt.has_value())
		{
			Raise(creature, {.kind = help::Kind::Tutorial, .parameter = static_cast<uint32_t>(*prompt)});
			auto& after = MemoryOf(registry, creature);
			after.tutorialTurn = turn;
			after.tutorialToldTurn.at(*prompt) = turn;
		}
	}();

	// How steadily the player's hand has been over it
	const bool over = Locator::pickingSystem::has_value() && Locator::pickingSystem::value().GetPick().object == creature;
	auto& memory = MemoryOf(registry, creature);
	memory.handOver = help::NextHandOver(memory.handOver, over);
}

void CreatureHelpSystem::PromptKnown(entt::entity creature, help::Prompt prompt)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(creature))
	{
		MemoryOf(registry, creature).tutorialDone.at(static_cast<size_t>(prompt)) = true;
	}
}

void CreatureHelpSystem::Fainting(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return;
	}
	if (const auto body = BodyOf(registry, creature); body.has_value())
	{
		if (const auto problem = help::FaintingProblem(*body); problem.has_value())
		{
			Raise(creature, {.kind = help::Kind::PhysicalProblem, .parameter = static_cast<uint32_t>(*problem)});
		}
	}
	// The game also says it was knocked out, but only once it lies dying, which holds that message back: never heard
}
