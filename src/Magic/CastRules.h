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
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack
{
struct GMagicInfo;
} // namespace openblack

// Where a miracle may be cast: the magic's cast rule (castRuleType) and its class's own check (one for a position, one
// for an object). Positions are map positions (x, z metres).

namespace openblack::magic::cast_rules
{
/// The 10 m cell is inside the map
[[nodiscard]] bool InBounds(const glm::vec3& position);
/// The position is on land
[[nodiscard]] bool IsLand(const glm::vec3& position);

/// The cast rule: in bounds, then ANYWHERE 1, ON_LAND IsLand, IN_INFLUENCE
/// CalculatePlayerInfluence(pos, player, 0, 0, 1) > 0, ON_LAND_IN_INFLUENCE both
[[nodiscard]] bool CanCastRule(const GMagicInfo& info, const glm::vec3& position, PlayerNames player);

/// The class's check at a position:
/// GMagicInfo = 1; GMagicHealInfo = FindTargets(pos, NULL) for HEAL / HEAL_PU_ONE;
/// GMagicResourceInfo = IsLand; GMagicCreatureSpellInfo = 0;
/// GMagicForestInfo = InBounds, IsLand, no Abode covers the point, ValidPlaceForTree (Spells/SpellForest);
/// GMagicTeleportInfo = no MultiMapFixed within 6 m (Magic/Objects/MagicTeleport)
[[nodiscard]] bool CanCastAt(MagicType type, const glm::vec3& position);

/// The class's check on an object: GMagicInfo = the check at the object's position;
/// object magic (resources) = IsLand there; GMagicForestInfo = 0; GMagicCreatureSpellInfo = a
/// Creature whose mind allows it (creatures are not ported yet: none can, 0)
[[nodiscard]] bool CanCastOn(MagicType type, entt::entity object);

/// FindTargets (pos, spell): the available Living effect receivers that the heal can heal
/// (CanBeHealedByHealSpell) within R = dummyVar (x the spell's tribal power) of the spiral's point that visits their
/// cell (not of the cast position: it measures from the MapCoords the spiral moves), searched in a spiral of
/// ceil(2R / 10)^2 cells, at most maxToHeal (x tribal power, rounded). With a spell each one becomes a target of its
/// PSys. Returns the number found.
/// Without a spell, `typeWithoutSpell` picks the row (the cast check passes the type it checks).
int FindHealTargets(const glm::vec3& position, entt::entity spell, MagicType typeWithoutSpell = MagicType::Heal);
} // namespace openblack::magic::cast_rules
