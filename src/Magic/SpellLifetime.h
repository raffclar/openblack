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

#include "Magic/SpellRules.h"

// How long a running miracle lives, apart from its particle effect. A miracle closes down when its time runs out, its
// strength falls to nothing, its caster goes or something stops it; it goes once whatever keeps it has gone. A miracle
// with a particle effect lives while the effect does, so it goes once its effect has died away after closing down. A
// miracle without one (the physical shield, teleport, the flocks) lives until it closes down, unless what its kind
// made (a shield's dome, a forest's trees, a flock) keeps it longer. Pure rules, tested without the game.

namespace openblack::magic
{

/// What a miracle's life depends on this turn
struct SpellLife
{
	/// Its magic type starts a particle effect
	bool hasParticleType {false};
	/// Its particle effect still runs
	bool effectRunning {false};
	bool closedDown {false};
	/// What its kind made still keeps it: a shield's dome fading out, a forest with trees left, a flock in the air
	bool keptByKind {false};
};

/// Whether a miracle carries on into the next turn or goes
enum class SpellFate : uint8_t
{
	Continue,
	Delete,
};

[[nodiscard]] SpellFate FateOf(const SpellLife& life);

/// Whether a cast is refused because the miracle's effect couldn't start: a miracle whose magic type names a particle
/// effect is nothing without it
[[nodiscard]] constexpr bool CastRefusedWithoutEffect(bool hasParticleType, bool effectStarted)
{
	return hasParticleType && !effectStarted;
}

/// What becomes of the seed in the hand once its miracle is cast
enum class SeedAfterCast : uint8_t
{
	/// It stays in the hand, linked to its miracle: lightning, water, food and wood, and the creature spells' phials
	StaysInHand,
	/// It leaves the hand, out of sight, and stays linked to its miracle until the miracle goes: the fireball, the heal,
	/// teleport, the flocks, the beam explosion
	LinkedToSpell,
	/// It leaves the hand bound to its miracle and goes with it: the storms and both shields, and the forest
	FollowsSpell,
};

/// From the seed's record: whether it is kept in the hand or follows its miracle. A kept seed stays in the hand, linked
/// to its miracle; a seed that is not kept leaves the hand linked to its miracle. The mark to delete a seed once cast
/// has no effect on the cast.
[[nodiscard]] SeedAfterCast SeedAfterCastOf(bool keptInHand, bool followsSpell);

/// A creature spell's phial stays in the hand, linked to its miracle, until the miracle is older than this many seconds
inline constexpr float k_CreatureSeedInHandSeconds = 4.0f;
/// Whether the seed in the hand leaves it, still linked to its miracle: only a creature spell's, once the miracle is
/// older than k_CreatureSeedInHandSeconds
[[nodiscard]] bool SeedLeavesHandWithSpell(SpellClass spellClass, float spellAge);

} // namespace openblack::magic
