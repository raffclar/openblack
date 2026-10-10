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

#include "Creature/CreatureCastMoves.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureMindTables.h"
#include "CreatureMindSystem.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using creature_desires::Desire;

namespace
{
constexpr std::string_view k_FightAction = "Fight";
/// Lying dead, for now or for good, a creature fights nobody
constexpr std::string_view k_DieAction = "Die";
constexpr std::string_view k_DeadForeverAction = "DeadForever";

/// Whether a creature is lying dead: knocked out, or doing one of the actions of lying dead
bool LyingDead(const ecs::Registry& registry, entt::entity creature, const creature_mind_tables::Tables& tables)
{
	if (registry.AllOf<CreatureKnockedOut>(creature))
	{
		return true;
	}
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	if (mind == nullptr || !mind->planActive || !mind->planner.current.has_value() ||
	    mind->planner.current->action >= tables.actions.size())
	{
		return false;
	}
	const auto& name = tables.actions[mind->planner.current->action].name;
	return name == k_DieAction || name == k_DeadForeverAction;
}
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
	// The script takes control of it first, so the action's agenda is made for a creature under a script's control
	registry.AssignOrReplace<ScriptControlled>(creature);
	const auto& name = tables->actions[action].name;
	if (name == k_FightAction)
	{
		return FightForScript(creature, target);
	}
	return ForcePlan(creature, {.desire = Desire::ObeyPlayer, .action = name, .object = target, .instrument = with});
}

bool CreatureMindSystem::FightForScript(entt::entity creature, entt::entity opponent)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& mind = registry.Get<CreatureMindState>(creature);
	const auto* tables = GetTables();
	// What it was doing is given up as a failure, whatever comes of the fight
	Abandon(mind);
	creature_mind::Plan(mind.idle, creature_mind::Activity::None, {});
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* other = registry.Valid(opponent) ? registry.TryGet<const Creature>(opponent) : nullptr;
	if (body == nullptr || other == nullptr || tables == nullptr || !Locator::creatureFightSystem::has_value())
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} has no other creature to fight", entt::to_integral(creature));
		return true;
	}
	const bool agrees = creature_fight::AgreesToFight(
	    creature_cast_moves::k_HeightOfSizeOne * ShownSize(*other), creature_cast_moves::k_HeightOfSizeOne * ShownSize(*body),
	    LyingDead(registry, opponent, *tables), registry.AllOf<ScriptControlled>(opponent), true);
	if (!agrees)
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} won't fight creature {}", entt::to_integral(opponent),
		                    entt::to_integral(creature));
		return true;
	}
	const auto started = Locator::creatureFightSystem::value().StartFight(creature, opponent);
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} picks a fight with creature {}: {}", entt::to_integral(creature),
	                    entt::to_integral(opponent), static_cast<int>(started));
	return true;
}
