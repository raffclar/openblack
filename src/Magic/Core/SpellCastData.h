/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// The cast arguments of a spell cast at a position (PSysProcessInfo and SpellEventInfo are in Particles/SpellLink.h).

namespace openblack::magic
{

/// SpellCastData (16 bytes)
struct SpellCastData
{
	/// the gesture's size for hand casts (1 for a FIRE seed, PrepareCast), the script's radius
	float magnitude {0.0f};
	float chants {0.0f};         ///< effect.initialChants x the seed's multiplier
	float duration {-1.0f};      ///< seconds: timerWhenPlayerCasting x the multiplier, or the script's time
	int maxObjectsToCreate {-1}; ///< the seed's stored count, or -1
};

} // namespace openblack::magic
