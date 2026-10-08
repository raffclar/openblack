/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/fwd.hpp>

namespace openblack
{
class DayNightClock;
}

namespace openblack::ecs
{

/// Once per game turn: in the evening up to 50 fireflies appear at random trees and rocks and one per turn flies to an
/// abode or street lantern found by a 300-unit spiral search (each cell is searched only on GameRand(2) != 0 and a
/// closer match ends the cell on GameRand(3) == 0: not strictly the nearest); in the morning one per turn flies back to
/// a tree or rock found the same way and goes to sleep.
void ProcessFireFliesTurn(const DayNightClock& clock);

/// Every frame: the orbit around the firefly's point, the distance fade and
/// the sprite (S_SpriteSheet3 frame 37, additive). `seconds` is game time (0 while paused).
void UpdateFireFlies(float seconds, const glm::vec3& camera);

/// Removes every firefly (new map)
void ClearFireFlies();

/// When an object is placed in the magic hand: a firefly whose map coordinates equal the point's (x and z; a sleeping
/// one sits exactly on its tree or rock) goes. True when there was one; the spell reward is
/// Worship/FireFlyReward.cpp's.
bool TakeFireFlyAt(const glm::vec3& position);

} // namespace openblack::ecs
