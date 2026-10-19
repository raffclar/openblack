/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <optional>
#include <string_view>

#include <spdlog/spdlog.h>

#include "Creature/CreatureLayers.h"
#include "Creature/CreatureMindTables.h"
#include "CreatureMindSystem.h"
#include "CreatureMindSystemDetail.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using creature_desires::Desire;

namespace
{
constexpr std::string_view k_FightAction = "Fight";
/// The action of lying dead for good
constexpr std::string_view k_DeadForeverAction = "DeadForever";
} // namespace

bool CreatureMindSystem::ScriptDoAction(entt::entity creature, uint32_t action, entt::entity target,
                                        std::optional<entt::entity> with)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	if (tables == nullptr || action >= tables->actions.size() || !registry.Valid(creature) ||
	    !registry.AllOf<CreatureMindState>(creature))
	{
		return false;
	}
	// From now on, every free creature agrees when another creature asks it to play, to be friends or to be shown
	// something, until the land changes
	registry.Context().partnersAlwaysAgree = true;
	// The script takes control of it first, so the action's agenda is made for a creature under a script's control. It
	// takes it from a fight, from lying knocked out and from what its hands do too.
	registry.AssignOrReplace<ScriptControlled>(creature);
	StopForScript(creature);
	// It comes across the thing to act on, and the thing to use
	auto& mind = registry.Get<CreatureMindState>(creature);
	ComeAcross(creature, mind, target);
	if (with.has_value())
	{
		ComeAcross(creature, mind, *with);
	}
	const auto& name = tables->actions[action].name;
	if (name == k_FightAction)
	{
		return FightForScript(creature, target);
	}
	// The order of the script is not a forced plan. When the script lets the creature go, it can take up reactions while
	// it does the order
	if (!ForcePlan(creature, {.desire = Desire::ObeyPlayer,
	                          .action = name,
	                          .object = target,
	                          .instrument = with,
	                          .ordered = true,
	                          .forced = false}))
	{
		return false;
	}
	// The order of a script counts at the end of the plan, as the creature's own plans do
	mind.forcedPlanCounts = true;
	// Falling dead, those about it react to the death
	if (name == k_DeadForeverAction && Locator::reactionSystem::has_value())
	{
		if (const auto* transform = registry.TryGet<const Transform>(creature))
		{
			Locator::reactionSystem::value().Create(
			    {.initiator = creature, .type = Reaction::ReactToDeath, .position = transform->position});
		}
	}
	return true;
}

void CreatureMindSystem::StopForScript(entt::entity creature)
{
	if (Locator::creatureFightSystem::has_value())
	{
		Locator::creatureFightSystem::value().TakeForScript(creature);
	}
	// The step that the creature does stops as a failure. Thus a walk to a thing that the hands act on also stops, and
	// does not start again on the next turn.
	if (Locator::creatureObjectActionSystem::has_value() &&
	    Locator::creatureObjectActionSystem::value().GetState(creature) == CreatureObjectActionSystemInterface::State::Busy)
	{
		Locator::creatureObjectActionSystem::value().Cancel(creature);
	}
}

bool CreatureMindSystem::IsLyingDead(entt::entity creature)
{
	const auto* tables = GetTables();
	const auto& registry = Locator::entitiesRegistry::value();
	return tables != nullptr && registry.Valid(creature) && mind_detail::IsDead(registry, creature, *tables);
}

creature_mind::HandsCheck CreatureMindSystem::CheckHandsBeforeFaint(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	const auto* animation = mind != nullptr ? registry.TryGet<const CreatureAnimation>(creature) : nullptr;
	if (animation == nullptr || !Locator::creatureObjectActionSystem::has_value())
	{
		return creature_mind::HandsCheck::Free;
	}
	auto& hands = Locator::creatureObjectActionSystem::value();
	// The body is busy while it plays an action, moves or catches, and the hands while they do a thing
	const bool moving =
	    Locator::creatureLocomotionSystem::has_value() && Locator::creatureLocomotionSystem::value().IsMoving(creature);
	const bool bodyBusy = creature_layers::IsPlaying(animation->body) || moving || hands.IsCatching(creature);
	const bool handsBusy = hands.GetState(creature) == CreatureObjectActionSystemInterface::State::Busy;
	const auto check = creature_mind::CheckHands(hands.GetHeld(creature).has_value(), bodyBusy, handsBusy);
	if (check != creature_mind::HandsCheck::LetGo)
	{
		return check;
	}
	// It puts the thing down or tosses it away, as before a step that needs empty hands
	return LetGoOfHeld(creature, *mind, creature_mind::LetGo::AsItChooses) ? creature_mind::HandsCheck::LetGo
	                                                                       : creature_mind::HandsCheck::Wait;
}

bool CreatureMindSystem::FightForScript(entt::entity creature, entt::entity opponent)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& mind = registry.Get<CreatureMindState>(creature);
	// What it was doing is given up as a failure, whatever comes of the fight
	Abandon(mind);
	creature_mind::Plan(mind.idle, creature_mind::Activity::None, {});
	if (!Locator::creatureFightSystem::has_value())
	{
		return true;
	}
	// The other creature is asked first, and may refuse
	[[maybe_unused]] const auto started = Locator::creatureFightSystem::value().StartFight(creature, opponent);
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} picks a fight with creature {}: {}", entt::to_integral(creature),
	                    entt::to_integral(opponent), static_cast<int>(started));
	return true;
}

void CreatureMindSystem::ForceFinish(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	if (mind == nullptr)
	{
		return;
	}
	// The action that its body plays ends at two times the usual speed
	if (auto* body = registry.TryGet<CreatureAnimation>(creature))
	{
		body->body = creature_layers::Hurry(body->body);
	}
	// What it was doing stops as a failure: it stops as the game stops a walk, drops what it does with a thing, and
	// forgets its agenda
	if (Locator::creatureObjectActionSystem::has_value() &&
	    Locator::creatureObjectActionSystem::value().GetState(creature) == CreatureObjectActionSystemInterface::State::Busy)
	{
		Locator::creatureObjectActionSystem::value().Cancel(creature);
	}
	if (Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().Stop(creature);
	}
	Abandon(*mind);
	if (Locator::magicSystem::has_value())
	{
		Locator::magicSystem::value().ReleaseCreatureCast(creature);
	}
	creature_mind::Plan(mind->idle, creature_mind::Activity::None, {});
}
