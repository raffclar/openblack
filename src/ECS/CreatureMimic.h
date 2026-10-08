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

#include <optional>

#include <entt/entity/fwd.hpp>

#include "Creature/CreatureDeeds.h"
#include "Enums.h"

// What the game tells the players' creatures about their players: the deeds a creature may copy, and the town needs
// it may come to share. Each is published as an event (ECS/Events/CreatureMimicEvents.h) and only read through the
// const registry, so with no creature in the world nothing changes. Docs: docs/bw1-notes/creature.md.

namespace openblack
{
class EventManager;
} // namespace openblack

namespace openblack::ecs::creature_mimic
{
/// A player did a deed to an object: the player's creature may copy it if it saw it. The point is the object's position
/// (the origin for an object with none).
void Consider(PlayerNames player, creature_watching::Deed deed, entt::entity object,
              std::optional<MagicType> magic = std::nullopt);

/// A villager has sunk while it could still be reached: the player whose hand last dropped it threw it in the sea
void ConsiderThrownInTheSea(entt::entity villager);

/// Whether a hit on a building counts as the player's deed of damaging it by throwing: the hit has a player, and the
/// building's own body is marked as thrown from the hand
[[nodiscard]] bool ShouldMimicBuildingHit(std::optional<PlayerNames> player, uint32_t buildingBodyFlags);

/// A villager of a player saw to one of the town's needs, with a weight: the player's creature may come to share the
/// need. Nothing without a player.
void EmpathiseWithTownDesire(std::optional<PlayerNames> player, TownDesireInfo desire, float weight, entt::entity villager);

/// The handler that passes the player's deeds on to the creatures' minds, when there are minds to pass them to
void AddMimicEventHandlers(EventManager& manager);
} // namespace openblack::ecs::creature_mimic
