/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

/// The sparkles at the cameras of the other players of a network game and of the computer gods (rules in
/// player_sparkles)
namespace openblack::ecs::other_player_sparkles
{

/// Once a game turn, on a computer that draws the world: one sparkle at the reported camera of each other player that
/// plays, and at the camera of each computer god whose mind plays, in the pool of that player. None while this computer's
/// camera is inside the temple, or while its hand directs a fight. The sparkles take the local random numbers.
void AddAtCameras();

} // namespace openblack::ecs::other_player_sparkles
