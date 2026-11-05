/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed runner: the actions of the computer gods.

#include <algorithm>
#include <ranges>

#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "Creature/CreatureMindTables.h"
#include "Debug/ComputerActions.h"
#include "ECS/Archetypes/ScriptMarkerArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ComputerPlayerSystemInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/WorkshopSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "InfoConstants.h"
#include "LHScriptX/Script.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Sound/GameMusic.h"
#include "TestbedScenarioRunner.h"
#include "TestbedScenarioRunnerParts.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;
using namespace openblack::testbed_scenarios::runner_parts;
using openblack::ecs::components::Creature;

void Runner::UpdateComputerActions()
{
	if (_scenario == nullptr)
	{
		return;
	}
	const uint32_t turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
	for (size_t i = 0; i < _scenario->computerActions.size() && i < _computerActionsDone.size(); ++i)
	{
		const auto& action = _scenario->computerActions[i];
		if (_computerActionsDone[i] || turn - _startTurn < action.afterTurns)
		{
			continue;
		}
		_computerActionsDone[i] = true;
		if (action.templeHit)
		{
			MarkTempleHit(action.player, turn);
			continue;
		}
		// The tool makes a marker on the land when it gives the action
		const auto thingOf = [this, &action](const ComputerActionThing& thing) {
			if (thing.kind != ComputerActionThing::Kind::Marker)
			{
				return ComputerActionThingOf(thing, action.player);
			}
			const auto point = MapPoint(_middle, thing.point);
			return ecs::archetypes::ScriptMarkerArchetype::Create(
			    {point.x, Locator::terrainSystem::value().GetHeightAt(point), point.y});
		};
		const auto first = thingOf(action.first);
		const auto second = thingOf(action.second);
		if (action.forceAtOnce)
		{
			if (!Locator::computerPlayerSystem::value().ForceAction(action.player, action.node, first, second))
			{
				SPDLOG_LOGGER_WARN(spdlog::get("game"), "Testbed: the computer god {} did not start {}",
				                   static_cast<int>(action.player), action.node);
			}
			continue;
		}
		if (!debug::computer_actions::Force(action.player, action.node, first, second))
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "Testbed: the computer god {} has no node {}",
			                   static_cast<int>(action.player), action.node);
		}
	}
}

void Runner::MarkTempleHit(PlayerNames player, uint32_t turn) const
{
	const auto temple =
	    Locator::worshipSiteSystem::has_value() ? Locator::worshipSiteSystem::value().TempleOf(player) : entt::null;
	auto* record = temple != entt::null ? Locator::entitiesRegistry::value().TryGet<ecs::components::Temple>(temple) : nullptr;
	if (record == nullptr)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Testbed: the computer god {} has no temple to hit", static_cast<int>(player));
		return;
	}
	record->lastHitTurn = turn;
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Testbed: the temple of the computer god {} is hit at turn {}",
	                   static_cast<int>(player), turn);
}

entt::entity Runner::ComputerActionThingOf(const ComputerActionThing& thing, PlayerNames player) const
{
	auto& registry = Locator::entitiesRegistry::value();
	switch (thing.kind)
	{
	case ComputerActionThing::Kind::None:
	case ComputerActionThing::Kind::Marker:
		break;
	case ComputerActionThing::Kind::Town:
	{
		const auto& towns = registry.Context().towns;
		const auto found = towns.find(thing.townId);
		return found != towns.end() ? found->second : entt::null;
	}
	case ComputerActionThing::Kind::Icon:
	{
		if (!Locator::infoConstants::has_value())
		{
			break;
		}
		const auto& info = Locator::infoConstants::value();
		entt::entity found = entt::null;
		registry.Each<const ecs::components::WorshipIcon>(
		    [&registry, &info, &found, &thing, player](entt::entity entity, const ecs::components::WorshipIcon& icon) {
			    const auto* site =
			        registry.Valid(icon.site) ? registry.TryGet<const ecs::components::WorshipSite>(icon.site) : nullptr;
			    if (found != entt::null || site == nullptr || site->player != player || icon.seed == SpellSeedType::None)
			    {
				    return;
			    }
			    const auto plain =
			        magic::GetMagicTypeFromPowerUpLevel(magic::GetSpellSeedInfo(info, icon.seed), magic::k_BasePowerUpLevel);
			    if (plain == thing.type)
			    {
				    found = entity;
			    }
		    });
		return found;
	}
	case ComputerActionThing::Kind::Object:
		return thing.object < _objects.size() ? _objects[thing.object] : entt::null;
	case ComputerActionThing::Kind::Creature:
		return thing.object < _creatures.size() ? _creatures[thing.object] : entt::null;
	case ComputerActionThing::Kind::Workshop:
	{
		const auto& towns = registry.Context().towns;
		const auto found = towns.find(thing.townId);
		const auto* town = found != towns.end() ? registry.TryGet<const ecs::components::Town>(found->second) : nullptr;
		if (town == nullptr)
		{
			break;
		}
		const auto workshop = std::ranges::find_if(town->abodes, [&registry](entt::entity abode) {
			return registry.Valid(abode) && registry.AllOf<ecs::components::Workshop>(abode);
		});
		return workshop != town->abodes.end() ? *workshop : entt::null;
	}
	case ComputerActionThing::Kind::PlayerCreature:
		return Locator::playerSystem::has_value()
		           ? Locator::playerSystem::value().GetPrimaryCreature(thing.owner).value_or(entt::null)
		           : entt::null;
	}
	return entt::null;
}
