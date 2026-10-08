/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// OPENBLACK_TEST_SHIELD_SHOT="<turns>,<path>[;<turns>,<path>...]": a screenshot that many game turns after the first
// MapShield was made (the frame count of --screenshot-frame drifts with the frame rate). Documented in
// docs/bw1-notes/openblack-internals.md.

namespace openblack::magic::shield_debug
{
/// MapShield::ProcessShields, each turn: `created` is the first shield's creation turn
void OnTurn(unsigned int created, unsigned int turn);
/// OPENBLACK_TEST_SHIELD_FRAMES="<turns>,<n>,<prefix>[@<slow>]": from that many turns after the first shield, a
/// screenshot on each of the next n frames (<prefix>_<i>.png, the turn and its fraction logged), to see the motion
/// inside a turn; with @<slow> the game speed multiplier becomes <slow> at the first shot (each shot stalls its frame).
/// @<slow> is no use for the PSys: Game.cpp hands psys::manager::ProcessTurn the plain k_TurnDuration, so its
/// interpolation fraction saturates at 1 after 0.1 s of real time however slow the turn is.
/// Called every frame after map_shield::DrawShields (MagicLoop.cpp)
void OnFrame();
} // namespace openblack::magic::shield_debug
