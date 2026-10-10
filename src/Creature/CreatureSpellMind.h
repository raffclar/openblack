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

#include <array>
#include <optional>

#include "CreatureDesires.h"

// What the mood and need spells do to a creature's desires. As one starts, its desire is made fully dominant and most
// of the others are held down for a long while, so the creature wants nothing else: the hunger, poo, tiredness and
// thirst only when all are to be, and never its wish to idle or play with the player, to restore its health, make
// friends, show its state, rest, hang around at home or look around. Compassion also makes it want to make friends
// above all. The spell's desire is let go each turn while the cheat lasts, and when it runs out every desire is let go.
// As the spell wears off its desire drops below the weakest of the others. Scripts make a desire the creature's only one
// the same way, for as long as they say. Pure, tested on their own.

namespace openblack::creature_spell_mind
{

/// The cheat lasts this many seconds, and holds the other desires down as long
inline constexpr float k_CheatSeconds = 20000.0f;
/// As a spell wears off its desire drops this far below the weakest of the others
inline constexpr float k_LeastDominantFactor = 1.3f;

/// A desire made dominant over the others for a while
struct Cheat
{
	creature_desires::Desire desire;
	/// Turns it has lasted
	uint32_t turns {0};
	/// Whole seconds it lasts
	uint32_t seconds {static_cast<uint32_t>(k_CheatSeconds)};
};

/// How a desire is made dominant: whether the body's needs are held down too, for how many seconds, and the least any
/// other desire is then wanted (the species' floor)
struct CheatSetup
{
	bool all {true};
	float seconds {k_CheatSeconds};
	float floor {0.0f};
};

/// Sources of these kinds are left as they are when a desire is made dominant: anger from being hurt, and wanting to
/// restore its health from its life
inline constexpr std::array<uint32_t, 2> k_SourcesNotMaximised {creature_desires::sources::k_AngerFromDamage,
                                                                creature_desires::sources::k_RestoreHealthFromLife};

/// Whether making a desire dominant holds another down: always, never, or only when all are to be
[[nodiscard]] bool HeldDownByCheat(creature_desires::Desire other, bool all);

/// The desire is wanted above all and the others held down for the cheat's time (in turns at a rate a second): it is
/// active, wanted as much as it can be with every other desire at the floor, and each of its sources (but those above)
/// is full. The cheat to keep.
[[nodiscard]] Cheat SetCheatDominant(creature_desires::Desires& desires, creature_desires::Desire desire,
                                     const CheatSetup& setup, float turnsPerSecond);
/// A script takes the creature's only desire away: the cheat ends, and the desire it was is no longer wanted at all.
/// The held-down desires are let go only when told to (see the mind's rule for when).
void ClearOnlyDesire(creature_desires::Desires& desires, std::optional<Cheat>& cheat, bool letGo);
/// A turn of a cheat: its desire is let go of again, and once its time is up every desire is; whether it still lasts
[[nodiscard]] bool StepCheat(creature_desires::Desires& desires, Cheat& cheat, float turnsPerSecond);
/// Every desire held down is let go
void ClearCheatDominance(creature_desires::Desires& desires);
/// The desire, if active, is wanted as much as it can be and every other desire as little as any can be, the species'
/// floor; it is let go of first, active or not
void MakeFullyDominantOverOthers(creature_desires::Desires& desires, creature_desires::Desire desire, float floor);
/// The desire is wanted less than any other: the weakest of the others' value over the factor, no more than its maximum
void MakeLeastDominant(creature_desires::Desires& desires, creature_desires::Desire desire,
                       float factor = k_LeastDominantFactor);

} // namespace openblack::creature_spell_mind
