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

#include <functional>

#include <glm/vec2.hpp>

/// Random positions shared by the villagers and the animals: one copy of CalcRandomPos.
namespace openblack::ecs::living
{

/// A per-class test of a candidate point (valid for the turn angle, valid for the map cell): a villager's are the
/// defaults, both true; an animal's are its own
using PosTest = std::function<bool(glm::vec2)>;

/// The cell's unsigned high words inside the map
[[nodiscard]] bool InBounds(glm::vec2 position);
/// Whether the point collides with `collideType`: off the map collides with every type
[[nodiscard]] bool Collides(glm::vec2 position, uint32_t collideType);

/// Two tries of a = GameFloatRand(2 pi), r = GameFloatRand(rMax - rMin) + rMin, the point at angle a and distance r from
/// the centre's map coordinates and a 25-cell spiral from it: the first in bounds, not colliding with `collideType` (from
/// the info) and passing both tests; else the centre when it passes the turn test, else `own` (the walker's position:
/// the original's fallback step is 0)
[[nodiscard]] glm::vec2 CalcRandomPos(glm::vec2 centre, float rMin, float rMax, uint32_t collideType, glm::vec2 own,
                                      const PosTest& validForTurnAngle, const PosTest& validForMapCell);

} // namespace openblack::ecs::living
