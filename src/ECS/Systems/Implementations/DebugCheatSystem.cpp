/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "DebugCheatSystem.h"

#include <utility>
#include <vector>

#include "ECS/Components/DebugCheats.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PrayerPower.h"
#include "ECS/Components/WorshipChants.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "Locator.h"

namespace openblack::ecs::systems
{

using components::DebugCheats;
using components::DebugCheatSite;
using components::Player;
using components::PrayerPower;
using components::WorshipChants;
using components::WorshipSite;

DebugCheatSystem::DebugCheatSystem()
    : DebugCheatSystem(
          []() -> Registry* { return Locator::entitiesRegistry::has_value() ? &Locator::entitiesRegistry::value() : nullptr; })
{
}

DebugCheatSystem::DebugCheatSystem(RegistryOf registry)
    : _registry(std::move(registry))
{
}

bool DebugCheatSystem::Set(PlayerNames player, debug_cheats::Cheat cheat, bool on)
{
	auto* registry = _registry ? _registry() : nullptr;
	if (registry == nullptr || cheat >= debug_cheats::Cheat::Count)
	{
		return false;
	}
	entt::entity found = entt::null;
	registry->Each<const Player>([&found, player](entt::entity entity, const Player& component) {
		if (component.name == player && found == entt::null)
		{
			found = entity;
		}
	});
	if (found == entt::null)
	{
		return false;
	}
	auto* cheats = registry->TryGet<DebugCheats>(found);
	const auto before = cheats != nullptr ? cheats->on : uint32_t {0};
	const auto after = debug_cheats::With(before, cheat, on);
	if (before == after)
	{
		return false;
	}
	if (cheats == nullptr)
	{
		cheats = &registry->Assign<DebugCheats>(found);
	}
	cheats->on = after;
	// The rules of the turn see the change at once
	ProcessTurn();
	return true;
}

uint32_t DebugCheatSystem::CheatsOf(PlayerNames player) const
{
	auto* registry = _registry ? _registry() : nullptr;
	if (registry == nullptr)
	{
		return 0;
	}
	uint32_t cheats = 0;
	// Few entities have cheats, so the walk starts from the cheats and is short
	registry->Each<const DebugCheats, const Player>(
	    [&cheats, player](entt::entity, const DebugCheats& component, const Player& owner) {
		    if (owner.name == player)
		    {
			    cheats = component.on;
		    }
	    });
	return cheats;
}

void DebugCheatSystem::ProcessTurn()
{
	auto* registry = _registry ? _registry() : nullptr;
	if (registry == nullptr)
	{
		return;
	}
	// With no cheat and no forced site there is nothing to do, and the turn costs nothing
	bool any = false;
	registry->Each<const DebugCheats>([&any](entt::entity, const DebugCheats&) { any = true; });
	registry->Each<const DebugCheatSite>([&any](entt::entity, const DebugCheatSite&) { any = true; });
	if (!any)
	{
		return;
	}
	// The prayer power of each player with cheats
	registry->Each<DebugCheats, PrayerPower>([](entt::entity, DebugCheats& cheats, PrayerPower& prayer) {
		const bool wanted = debug_cheats::Has(cheats.on, debug_cheats::Cheat::InfiniteMiracleCharge);
		if (wanted && !cheats.prayerForced)
		{
			cheats.prayerWasInfinite = prayer.infinite;
			cheats.prayerForced = true;
		}
		else if (!wanted && cheats.prayerForced)
		{
			prayer.infinite = cheats.prayerWasInfinite;
			cheats.prayerForced = false;
		}
		if (cheats.prayerForced)
		{
			prayer.infinite = true;
		}
	});
	// The worship sites: a site of a player with the cheat keeps its own flags aside and gets the forced flags. A site
	// whose player has no cheat now gets its own flags back.
	std::vector<entt::entity> restored;
	registry->Each<const WorshipSite, WorshipChants>(
	    [this, registry, &restored](entt::entity site, const WorshipSite& component, WorshipChants& chants) {
		    const auto cheats = CheatsOf(component.player);
		    auto* kept = registry->TryGet<DebugCheatSite>(site);
		    const bool forced = debug_cheats::Has(cheats, debug_cheats::Cheat::InfiniteMiracleCharge);
		    if (forced)
		    {
			    if (kept == nullptr)
			    {
				    registry->Assign<DebugCheatSite>(site, chants.infinite, chants.freeMaintenance);
			    }
			    const auto flags = debug_cheats::SiteFlagsWith(cheats, {});
			    chants.infinite = flags.infinite;
			    chants.freeMaintenance = flags.freeMaintenance;
		    }
		    else if (kept != nullptr)
		    {
			    chants.infinite = kept->infinite;
			    chants.freeMaintenance = kept->freeMaintenance;
			    restored.push_back(site);
		    }
	    });
	for (const auto site : restored)
	{
		registry->Remove<DebugCheatSite>(site);
	}
	// A player with no cheat left keeps no record of cheats
	std::vector<entt::entity> empty;
	registry->Each<const DebugCheats>([&empty](entt::entity entity, const DebugCheats& cheats) {
		if (cheats.on == 0 && !cheats.prayerForced)
		{
			empty.push_back(entity);
		}
	});
	for (const auto entity : empty)
	{
		registry->Remove<DebugCheats>(entity);
	}
}

} // namespace openblack::ecs::systems
