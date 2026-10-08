/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// The fire's crackle: only the 2 burning objects nearest the camera play it (two slots of {fire, distance}, and the
// farthest slot's distance), a looped G_Fire sample at the object.

namespace openblack::ecs::fire
{
struct FireEffect;

namespace sound
{
/// First in the fire list's pass: the slots' camera distances again
void RefreshDistances();
/// At the end of a fire's process: a fire with a fraction above 0.1 takes the first free or farthest slot when it is
/// nearer than that one (or no slot is taken); otherwise it gives its slot back
void Consider(FireEffect& fire, bool loud);
/// At the end of the fire list's pass: each slot's fire plays (looped, bank 2)
void StartSlots();
/// ToBeDeleted: the fire's slot is freed (its sound stops)
void Free(FireEffect& fire);
void Clear();
} // namespace sound
} // namespace openblack::ecs::fire
