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

#include "Enums.h"

// The wonder's own part: its power and the per-tribe sum the player keeps. The rest of a wonder is a plain abode
// (MakeFunctional is the abode's). The scaffold side (SetPower from the town's wonder power when a planned building
// is built, the WonderScale) is ecs::scaffolds'.

namespace openblack::ecs::wonders
{
/// The entity has a components::Wonder
[[nodiscard]] bool IsWonder(entt::entity entity);
/// After the abode is made (AbodeArchetype): the wonder part zeroed, SetPower with the scale, the creation hooks,
/// then IsBuilt -> AddToPlayer. A script's whole wonder counts at once, a plan's from Built
void Create(entt::entity wonder, float scale);
/// Sets the wonder's power (the scaffold sets it from the town's wonder power when the planned building is built)
void SetPower(entt::entity wonder, float power);
/// The wonder's power (0 for anything that is not a wonder)
[[nodiscard]] float GetPower(entt::entity wonder);
/// The town's player and the info's tribe; a player, a tribe and IsBuilt -> the player's wonder power[tribe] = power
/// + it (a float)
void AddToPlayer(entt::entity wonder);
/// The same tests -> the player's wonder power[tribe] = it - power
void RemoveFromPlayer(entt::entity wonder);
/// The part after the abode's Built (abodes::Built calls it at its end): AddToPlayer
void Built(entt::entity wonder);
/// Run first when the wonder is deleted: RemoveFromPlayer, then the abode's DeleteDependants (the caller's)
void DeleteDependants(entt::entity wonder);
/// PlayerMagic::wonderPower[tribe]: the sum of the built wonders' power of that tribe the player owns. (pending) who
/// reads it (inferred: a wonder bonus per tribe) and who clears it (the player's creation and map clearing were not
/// checked for it)
[[nodiscard]] float PlayerWonderPower(PlayerNames player, Tribe tribe);
/// (openblack) the "wonder_power" part of the turn state hash (Debug/StateHash.h): every player's wonder power in
/// tribe order, through magic::players::MagicOf (also the players without an entity). Registered on every land load
/// (Game::LoadMap; a part of the same name is replaced)
void RegisterStateHash();
} // namespace openblack::ecs::wonders
