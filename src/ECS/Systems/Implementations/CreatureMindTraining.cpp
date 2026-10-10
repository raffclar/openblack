/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <chrono>
#include <vector>

#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureSpellMind.h"
#include "Creature/CreatureTraining.h"
#include "CreatureMindSystem.h"
#include "CreatureMindSystemDetail.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Transform.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHighlightRules.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Enums.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using creature_desires::Desire;

namespace
{
constexpr float k_TurnsPerSecond = 1.0f / std::chrono::duration<float>(TimeSystemInterface::k_TurnDuration).count();
} // namespace

float CreatureMindSystem::GetInteractionMagnitude(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* mind = registry.Valid(creature) ? registry.TryGet<const CreatureMindState>(creature) : nullptr;
	return mind != nullptr ? mind->interactionMagnitude : 0.0f;
}

void CreatureMindSystem::ClearInteractionMagnitude(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr)
	{
		mind->interactionMagnitude = 0.0f;
	}
}

uint32_t CreatureMindSystem::GetActionCount(entt::entity creature, uint32_t action) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* mind = registry.Valid(creature) ? registry.TryGet<const CreatureMindState>(creature) : nullptr;
	return mind != nullptr ? creature_training::TimesCarriedOut(mind->actionCounts, action) : 0;
}

void CreatureMindSystem::SetOnlyDesire(entt::entity creature, Desire desire, float seconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	const auto* body = mind != nullptr ? registry.TryGet<const Creature>(creature) : nullptr;
	if (body == nullptr)
	{
		return;
	}
	// A creature told before it has first thought has its desires set up for it
	SetUpMind(creature, *body, *mind);
	if (!mind->desires.has_value())
	{
		return;
	}
	// The creature keeps one desire made dominant at a time, which the mood spells share
	auto* spells = registry.TryGet<CreatureSpells>(creature);
	if (spells == nullptr)
	{
		spells = &registry.Assign<CreatureSpells>(creature);
	}
	spells->cheat = creature_spell_mind::SetCheatDominant(
	    *mind->desires, desire, {.all = true, .seconds = seconds, .floor = mind_detail::DesireFloorOf(creature)},
	    k_TurnsPerSecond);
}

void CreatureMindSystem::ClearOnlyDesire(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	auto* spells = mind != nullptr ? registry.TryGet<CreatureSpells>(creature) : nullptr;
	if (spells == nullptr || !mind->desires.has_value())
	{
		return;
	}
	// The desires held down are let go only for a creature of no player, or one on the learning leash
	const auto* body = registry.TryGet<const Creature>(creature);
	const bool letGo = body == nullptr || body->owner == PlayerNames::NEUTRAL ||
	                   (Locator::leashSystem::has_value() && Locator::leashSystem::value().TypeOf(creature) == LeashType::Rope);
	creature_spell_mind::ClearOnlyDesire(*mind->desires, spells->cheat, letGo);
}

bool CreatureMindSystem::PointOutHighlight(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.Valid(creature) ? registry.TryGet<const Transform>(creature) : nullptr;
	const auto* tables = GetTables();
	if (transform == nullptr || tables == nullptr || !Locator::entitiesMap::has_value())
	{
		return false;
	}
	const auto& map = Locator::entitiesMap::value();
	const auto highlight = creature_training::HighlightToPointOut(
	    map_coords::FromMetres({transform->position.x, transform->position.z}), [&registry, &map](glm::ivec2 cell) {
		    std::vector<creature_training::HighlightInCell> highlights;
		    for (const auto entity : map.GetAllInCell(cell))
		    {
			    if (const auto* found = registry.TryGet<const ScriptHighlight>(entity))
			    {
				    highlights.push_back({.entity = entity, .tipSign = ecs::script_highlights::IsTipSign(found->kind)});
			    }
		    }
		    return highlights;
	    });
	if (!highlight.has_value())
	{
		return false;
	}
	// It obeys its player by pointing the highlight out, forced over anything it wants
	const auto action = creature_mind_tables::FindAction(*tables, "PointOutHighlight");
	if (!action.has_value() ||
	    !ForcePlan(creature, {.desire = Desire::ObeyPlayer, .action = "PointOutHighlight", .object = *highlight}))
	{
		// TODO(creature-do-action-2): openblack's creature can't carry out pointing a highlight out yet
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} can't point out highlight {} yet", entt::to_integral(creature),
		                    entt::to_integral(*highlight));
	}
	return true;
}
