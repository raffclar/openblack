/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"
#include "Enums.h"

// The town's resource stores outside its storage pit: the temporary food and wood pots. Villagers drop their resources
// there while the town has no working storage pit.

namespace openblack::ecs::town_stores
{
/// What GetTemporaryResourceStorePotOrPos returns: the pot and the point to go to
struct TemporaryStore
{
	entt::entity pot {entt::null};
	map_coords::MapCoords pos {};
};

/// How far from the congregation point a new wood pot goes
constexpr float k_WoodPotOffset = 5.0f;
/// The clear area searched around a new pot: 45.0, 1.5 and the radius 2.0
constexpr float k_ClearAreaA = 45.0f;
constexpr float k_ClearAreaB = 1.5f;
constexpr float k_ClearAreaRadius = 2.0f;

/// The town's temporary pot of `type` (FOOD 0, WOOD 1) when it is available. Otherwise a new one: the
/// congregation point + GetPosFromAngle(0, WOOD ? 5 : 0), moved by FindClearArea(45, 1.5, 2) away from the MultiMapFixed
/// objects (its result is not tested), and a pot with the pot info FOOD ? 10 MagicFood : 9 MagicWood and amount 0; kept
/// as the town's pot of that type. Either way the point is the pot's nearest edge to `from`. There is always a pot: the
/// original never returns none (the creation fails only when its allocation does). Without a town (or another type)
/// {entt::null, from}
[[nodiscard]] TemporaryStore GetTemporaryResourceStorePotOrPos(entt::entity town, const map_coords::MapCoords& from,
                                                               ResourceType type);

/// Player >= 8 or type not FOOD / WOOD -> 0; never taken -> 1; else r = min((turn - last) /
/// maxGameturnsForBeliefAfterRemovingFromStoragePit (1000), 1) and r^3. How much a player giving back what it has just
/// taken counts
[[nodiscard]] float GetGameTurnResourceLastRemovedModifier(entt::entity town, PlayerNames player, ResourceType type);
/// The same bounds, then the turn the player last took the resource = now. Only the abode's resource
/// removal calls it
void SetGameTurnResourceLastRemoved(entt::entity town, PlayerNames player, ResourceType type);
/// Adds f to player P's belief in the town: pending[n] += f (folded each turn by
/// ecs::town_belief::Fold), recent[n] += f (decays), f != 0 -> lastAddedTurn[n] = the turn. With a thing: draw ->
/// town_belief::DrawBelief (the belief-sprite queue) and the guidance's belief sound (pending: it needs the interface's
/// position for the distance; it plays only for a player below the strongest belief)
void AddToBelief(entt::entity town, PlayerNames player, float f, entt::entity thing, bool draw, int guidanceAlignment);
/// When a storage pit becomes functional: the town's pit = `pit`; each temporary pot: available and holding its
/// resource -> its reaction (animal_ai::SetupPotReaction), else ToBeDeleted; the slot = 0
void SetStoragePit(entt::entity town, entt::entity pit);
/// A step of the town's turn: each temporary pot (FOOD, WOOD) available, empty and with the town's storage pit
/// functional -> ToBeDeleted, the slot = 0; not available -> the slot = 0; else it stays
void ProcessTemporaryPots(entt::entity town);
} // namespace openblack::ecs::town_stores
