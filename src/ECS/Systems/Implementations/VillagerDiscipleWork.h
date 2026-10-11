/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <entt/entity/entity.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
}

/// The work of the disciples who have no ordinary job to fall back on: the trader carrying food or wood from its own
/// town's store to another town's, the craftsman keeping its town's workshops in wood, and the missionary, who moves
/// into the nearest town and wins its villagers' belief for the player who sent it. The state functions are the living
/// action system's.
namespace openblack::ecs::villager_disciple_work
{

/// A trader deciding: it fetches or delivers what the town it trades with wants more, or waits by that town's centre
/// when it wants neither. 1 when it set about either.
uint32_t CheckTrader(components::LivingAction& action);
/// At its own town's store to fetch a load
uint32_t TraderFetch(components::LivingAction& action);
/// At the other town's store to deliver its load
uint32_t TraderDeliver(components::LivingAction& action);

/// A craftsman deciding: it takes the wood it carries to the workshop that wants it most, or fetches wood from its
/// town's store for it. 1 when it set about either.
uint32_t CheckSupplyWorkshop(components::LivingAction& action);
/// At the workshop with its wood
uint32_t ArrivesAtWorkshopForDropOff(components::LivingAction& action);
/// At the store for the workshop's wood
uint32_t ArrivesAtStoragePitForWorkshopMaterials(components::LivingAction& action);

/// A missionary put down: it moves into the nearest building of any town, starts preaching there for its player, and
/// goes to its new home
uint32_t MissionaryDisciple(components::LivingAction& action);
/// Once a game turn: each missionary's preaching follows it about, and stops once it no longer preaches (gone, dead,
/// held or set to other work), freeing it
void ProcessMissionaries();
/// A town changed hands or went: the missionaries preaching in it stop and are freed
void StopMissionariesIn(entt::entity town);

} // namespace openblack::ecs::villager_disciple_work
