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

#include "ECS/Components/Spell.h"
#include "Enums.h"

namespace openblack::ecs::components
{

/// The miracle in the hand, made from a worship icon or from a seed info. The entity also has a Transform and the
/// seed's mesh
/// (GSpellSeedInfo.mesh, scale = info.scale).
struct SpellSeed
{
	/// bit 0 cleared on cast / set in hand; bit 1 = don't keep it when it comes into the hand
	uint8_t flags {0};
	SpellSeedType seedType {SpellSeedType::None}; ///< its GSpellSeedInfo
	entt::entity icon {entt::null};               ///< the WorshipSpellIcon that charged it
	entt::entity spell {entt::null};              ///< the spell cast from it
	bool inInterface {false};                     ///< in the local player's hand
	SpellCreator creator {};                      ///< the spell's creator: the icon, or the interface's player
	int powerUp {-1};                             ///< POWER_UP_TYPE (-1 = the base)
	bool hasCast {false};
	bool fromOneShot {false}; ///< made by a one-off spell seed into the hand
	float chantStore {0.0f};  ///< the charge
	float chantStoreCopy {0.0f};
	float storedChants {0.0f}; ///< the last spell's chants (-1 = none)
	int storedMaxObjects {-1};
	float storedAge {0.0f};
	float castMultiplier {1.0f}; ///< scales initialChants and the duration
	float psysPower {1.0f};      ///< multiplies the spell's strength
	bool ready {false};          ///< active: held long enough (or at once)
	int turnsInHand {0};
	MagicType lastMagic {MagicType::None}; ///< the magic of the last cast
};

} // namespace openblack::ecs::components
