/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// Casting from the hand, the per-frame and per-turn parts MagicLoop.cpp calls: the gesture sampling and
// ProcessPowerUpSystem, the in-hand effect, the hand effects and the utility effects. Wiki: docs/bw1-notes/magic.md.

namespace openblack::magic::hand_casting
{
/// A land is loaded
void OnLoadMap();
/// At the start of the game turn (the interface's turn: ProcessPowerUpSystem again, with the last frame's game time)
void ProcessTurn();
/// Every frame after the hand's update (the interface's frame updates, the hand's draw): seconds of game time (0 while
/// paused)
void Update(float seconds);
} // namespace openblack::magic::hand_casting
