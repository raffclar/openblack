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

#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

// The storm and tornado spell: MAGIC_TYPE 16 STORM (wind and rain), 17 STORM_PU1 (with lightning) and 18 STORM_PU2
// (the tornado). The three cast the same SF_LightningStormPush, whose rules read the power-up level (Rules/Storm.cpp),
// plus an SF_StormCast swirl at the hand. Wiki: docs/bw1-notes/miracles.md, "Storm, electric storm and tornado".

namespace openblack::magic
{
/// The storm spell's own state (both zeroed at creation)
struct SpellStormData
{
	uint32_t castEffect {0};    ///< The SF_StormCast effect (psys::manager id), stepped by the spell
	uint32_t waterReaction {0}; ///< REACTION_REACT_TO_MAGIC_WATER_PUTTING_OUT_FIRE (34), made by ReactToRainOnFire
};

/// The cast radius clamp: r = min(r, maxRadius), then max(r, minRadius)
[[nodiscard]] float ClampStormRadius(float minRadius, float maxRadius, float radius);
/// The storm's upkeep: base x (magnitude / radiusForNormalCost)^2
[[nodiscard]] float StormCostToMaintain(float baseCost, float magnitude, float radiusForNormalCost);

namespace spell_storm
{
/// From the fire's rain cooling: the first storm spell of the list (the newest first) with no water reaction whose 2D
/// radius (its magnitude) reaches the burning object (distance in x, z to the spell's position) gets
/// REACT_TO_MAGIC_WATER_PUTTING_OUT_FIRE by its player, stamped
void ReactToRainOnFire(const glm::vec3& objectPosition);
/// The storm spells, the newest first
[[nodiscard]] const std::vector<entt::entity>& Spells();
/// The SF_StormCast effect of a storm spell (0 none)
[[nodiscard]] uint32_t CastEffectOf(entt::entity spell);
/// A land is loaded
void Clear();
} // namespace spell_storm
} // namespace openblack::magic
