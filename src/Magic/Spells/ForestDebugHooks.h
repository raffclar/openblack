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

// OPENBLACK_TEST_FOREST_SHOT="<turns>,<path>[;<turns>,<path>...]": a screenshot that many game turns after the forest
// miracle's seed landed (the frame count of --screenshot-frame drifts with the frame rate). Documented in
// docs/bw1-notes/openblack-internals.md.

namespace openblack::magic::forest_debug
{
/// SpellForest::SpellEvent made the forest
void OnLanded(entt::entity spell, unsigned int turn);
/// SpellForest::Process, each turn
void OnTurn(entt::entity spell, unsigned int turn);
} // namespace openblack::magic::forest_debug
