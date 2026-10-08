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

// The creature miracles (freeze, small and big, weak and strong, fat and thin, invisible, nice and nasty, itchy and the
// unfinished ones). Cast at a place, one plays as a plain spell. Cast on a creature, the creature takes it on: the
// spell then runs until the creature lets it go, and each turn its effect eases in, holds and wears off
// (creature_spells). Wiki: docs/bw1-notes/creature.md.

namespace openblack::magic::spell_creature
{
/// The creature takes the spell on, held for the spell's time made longer by the caster's tribal power, after which
/// the spell runs with no time limit until the creature lets it go. Another of its kind already on the creature is
/// cut short, and its spell closed down. Nothing for anything but a creature, or for a magic type that is not one of
/// the creature miracles.
void Receive(entt::entity creature, entt::entity spell);

/// Once a game turn: every creature's spells move on by a turn, their effects applied to it, and the spells that have
/// worn off closed down
void ProcessTurn();
} // namespace openblack::magic::spell_creature
