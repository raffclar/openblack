/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// The heal miracle's test hook (docs/bw1-notes/openblack-internals.md):
//   OPENBLACK_TEST_HURT_VILLAGERS="x,z,radius,life[,poisoned]"  the villagers within radius of (x, z) get that life
//   (0..1), and poisoned with a 1; then every change of their life is logged ("Heal test: ...").

namespace openblack::magic::heal_debug
{
/// Every turn, before the other magic hooks (so that a heal cast the same turn finds them hurt)
void RunDebugHooks();
/// A new land: the hook runs again
void ResetDebugHooks();
} // namespace openblack::magic::heal_debug
