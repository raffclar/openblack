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

// The storm's test hooks (not in the original), documented in docs/bw1-notes/openblack-internals.md:
// OPENBLACK_TEST_STORM_SHOT="<turns>,<path>[;<turns>,<path>...]" asks for a screenshot that many game turns after the
// first storm spell of the land was cast; OPENBLACK_STORM_TRACE=1 logs, every 10 turns of every storm spell, its age,
// chants, the weather at its centre (rain, overcast, wind, temperature), the clouds, the
// lightning and the objects its tornado carries (and, from Particles/Rules/Storm.cpp, the storm's registration, every strike
// and every object taken).

namespace openblack::magic::storm_debug
{
/// Every turn of every storm spell
void OnTurn(entt::entity spell);
/// A land is loaded
void Reset();
} // namespace openblack::magic::storm_debug
