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

#include <entt/entity/fwd.hpp>

/// Towns' graveyards in the world: which graveyard a town counts its dead in, the burials counted there and the graves
/// they show. The rules are graveyard::'s (GraveyardRules.h).
namespace openblack::graveyards
{

/// A graveyard building is made: it keeps its burials, and starts working at once when it stands built
void Made(entt::entity graveyard);

/// A graveyard starts working (built, finished by its builders, or repaired whole): the town takes it as its own when it has
/// none, and it counts a burial straight away, so a new graveyard shows its first grave
void StartsWorking(entt::entity graveyard);

/// One of a town's people has died: its graveyard, if it has one, counts a burial
void TownPersonDied(entt::entity town);

/// A graveyard goes: when it was its town's own, the first other working civic building of the town (any but a wonder)
/// takes over as its graveyard, as in the original game. Another building standing in as the town's graveyard going
/// leaves the town with none.
void Lost(entt::entity graveyard);

/// Whether the town has a graveyard of its own and it works (a civic building standing in as one counts)
[[nodiscard]] bool HasWorkingGraveyard(entt::entity town);

/// The town's own graveyard (or the civic building standing in as one), none when it has none
[[nodiscard]] entt::entity GraveyardOf(entt::entity town);

/// How many of a graveyard model's graves are shown: none for anything else
[[nodiscard]] uint32_t GravesShown(entt::entity graveyard);

} // namespace openblack::graveyards
