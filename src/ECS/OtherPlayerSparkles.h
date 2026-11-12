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

#include <glm/vec3.hpp>

/// The sparkles at the cameras of the other players of a network game and of the computer gods (rules in
/// player_sparkles)
namespace openblack::ecs::other_player_sparkles
{

/// Once a game turn, on a computer that draws the world: one sparkle at the reported camera of each other human player
/// that is in the game, and at the camera of each computer god whose mind plays, in the pool of that player
/// (player_sparkles::PlaceOf). None while this computer's camera is inside the temple, or while its hand directs a
/// fight. The sparkles take the local random numbers.
void AddAtCameras();

/// Once a frame, before the draw: the symbol of the god shows over the sparkles of each other player that shows it
/// (player_sparkles::ShowsSymbol), and turns by the milliseconds of the frame's game time. An observer of the god of the
/// observers shows it. No symbol shows while this computer's hand directs a fight. The symbols turn in the order that
/// they draw in for a camera at `camera`.
void TurnSymbols(int32_t milliseconds, const glm::vec3& camera);

} // namespace openblack::ecs::other_player_sparkles
