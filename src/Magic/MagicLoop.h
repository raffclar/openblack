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

// The miracles' hooks into the game loop (Game.cpp calls each at its step of the game turn; the sub-systems add their
// call here, never in Game.cpp). Wiki: docs/bw1-notes/magic.md.

namespace openblack::magic
{
/// The land is loaded (after psys::manager::Clear and before the script runs): the spells, reactions and the players
/// without an entity are cleared
void OnLoadMap();
/// The game inputs, before the game code (StartTurn): the hand's casting (the power-up system with the last frame's
/// time)
void ProcessGameInputs();
/// The start of the game turn, before the global game lists (the puzzle games) and the villagers: the atmosphere, the
/// influence rings, the players, the dances
void ProcessTurnStart(uint32_t turn);
/// Slot 5, the forests: after the global game lists, before the living things
void ProcessForests(uint32_t turn);
/// Where the original's Living step ends (after livingActionSystem.Update): slots 6..8
void ProcessTurn(uint32_t turn);
/// The PSys game loop end, after the physics' turn update and before the scripts: the EXPLODE_OBJECT queue and the
/// PSys sounds of this turn's atoms
void ProcessSpellParticlesEndOfLoop();
/// The hand's turn update, after the belief's once-per-turn step: the grain's raise and the held object's
/// ProcessInHand
void ProcessHandTurn();
/// Every frame, after the hand's update: seconds of game time (0 while paused)
void Update(float seconds);
/// The environment-variable test hooks (MagicDebugHooks.cpp), every turn; each runs once the land exists
void RunDebugHooks();
/// (OnLoadMap) the hooks run again on the next land
void ResetDebugHooks();
} // namespace openblack::magic
