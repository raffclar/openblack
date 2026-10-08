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

// The town centre's spell icons: one TownCentreSpellIcon per spell seed the town holds, at most 6, on the town
// centre mesh's special points 0..5.
// Tapping one taps the worship site's icon of the same seed.

namespace openblack::worship::town_centre
{
/// Nothing if an icon of the seed exists; else in the first free slot whose special point exists, a town spell icon
/// at the point with its angle and the town centre's scale. It goes in the town's list and so at the town's worship
/// site. True when made.
bool AddSpell(entt::entity townCentre, SpellSeedType seed);
/// The seed's icons leave the town centre and go (out of the town's list, and the site's icon if no town holds the
/// seed any more)
void RemoveSpell(entt::entity townCentre, SpellSeedType seed);
/// The power-up level set / cleared on the seed's icon
void AddPowerUp(entt::entity townCentre, SpellSeedType seed, int powerUp);
void ClearPowerUp(entt::entity townCentre, SpellSeedType seed, int powerUp);
/// The town centre's icon of that seed, or entt::null
[[nodiscard]] entt::entity FindSpellIcon(entt::entity townCentre, SpellSeedType seed);
/// How many icons (the town centre can hold more while < 6)
[[nodiscard]] int SpellCount(entt::entity townCentre);

/// The icons part of a town centre's removal: each of the 6 slots is emptied, the icon forgets its town centre and is
/// deleted unless a flag of it is set ((pending) the flag is not identified; openblack's icons go at once, so a slot
/// never holds a deleted one)
void DeleteDependants(entt::entity townCentre);

/// A town centre is made: an icon for every magic the town already holds
/// (AddSpell + AddPowerUp), then site::AddTownSpells
void MakeFunctional(entt::entity townCentre);
} // namespace openblack::worship::town_centre
