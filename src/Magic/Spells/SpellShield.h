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

namespace openblack
{
struct GMagicShieldInfo;
} // namespace openblack

// SpellShield (a spell with objects): MAGIC_TYPE SHIELD (19) and PHYSICAL_SHIELD (20). It makes one MapShield
// (Magic/Objects/MapShield) and an anti-influence ring for
// every other active player; the magic shield's dome is its PSys (SF_DefenseSphere). Upkeep = costPerGameTurn x
// (radius / radiusForNormalCost)^2. Wiki: docs/bw1-notes/magic.md ("Shields").

namespace openblack::magic
{

/// The shield spell's own state
struct SpellShieldData
{
	uint32_t struckReaction {0};     ///< REACTION_REACT_TO_MAGIC_SHIELD_STRUCK (35), made on the first hit
	uint32_t shieldReaction {0};     ///< REACTION_REACT_TO_MAGIC_SHIELD (13)
	entt::entity town {entt::null};  ///< The nearest town within 250 m at the cast
	std::vector<entt::entity> rings; ///< The anti-influence rings, newest first
};

/// The cast's radius clamp: max first (r >= max -> max), then min (r <= min -> min)
[[nodiscard]] float ClampShieldRadius(const GMagicShieldInfo& info, float radius);
/// Upkeep: base x (magnitude / radiusForNormalCost)^2
[[nodiscard]] float ShieldCostToMaintain(float baseCost, float magnitude, float radiusForNormalCost);

namespace spell_shield
{
/// The struck reaction (35) the first time, else its turn stamp refreshed
void UpdateStruckReaction(entt::entity spell);
/// The REACTION 13s it started go, then REACTION 36 (destroyed)
void SetUpDestroyedReaction(entt::entity spell);
/// dist(p, castPos) < the spell's radius - margin (MapCoords)
[[nodiscard]] bool IsUnder(entt::entity spell, const glm::vec3& point, float margin);
/// For the script's spell-at-point query (mask 3): the first available shield spell whose
/// 2D radius around originalCastPos is strictly greater than its distance to the point. The type bit
/// (19 -> 2, 20 -> 1) is tested as `(bit | mask) != 0`, so no mask filters.
[[nodiscard]] entt::entity FindShieldAt(const glm::vec3& point, uint32_t mask);
/// The town of the spell
[[nodiscard]] entt::entity TownOf(entt::entity spell);
/// The shield spells, newest first
[[nodiscard]] const std::vector<entt::entity>& Spells();
/// A land is loaded
void Clear();
} // namespace spell_shield
} // namespace openblack::magic
