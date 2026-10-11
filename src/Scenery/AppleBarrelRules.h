/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>
#include <span>

#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

// The apple barrel with its worm: a little ornament the land draws beside the local player's storage pit, the pit of
// their towns nearest their temple. The barrel stands on the land at the pit's edge, on the side the pit faces, never
// turned and at its own size; the worm sits on the barrel's top, wriggling on its clip by the game's clock. Neither is a
// thing of the world: the hand, the creature and the villagers never meet them. Pure functions, tested on their own.

namespace openblack::apple_barrel
{

/// No storage pit further than this many metres from the temple carries the barrel
constexpr float k_FarthestStore = 99990.0f;
/// With no temple the pits are measured from this place on the land instead
constexpr glm::vec2 k_NoTemplePlace {256.0f, 256.0f};

/// Where the pits are measured from: the player's temple, or a fixed place on the land when they have none
[[nodiscard]] glm::vec2 MeasuredFrom(std::optional<glm::vec2> temple);

/// Which of the pits carries the barrel: the nearest the place along the ground, nearer than the farthest allowed, the
/// first of equals; none when there is none
[[nodiscard]] std::optional<size_t> NearestStore(std::span<const glm::vec2> stores, glm::vec2 from);

/// Where the barrel stands on the ground: out from the pit's middle by the pit's radius on the ground, the way the pit
/// faces (the way its turn takes the model's own x axis)
[[nodiscard]] glm::vec2 BarrelSpot(glm::vec2 store, const glm::mat3& storeRotation, float storeRadius);

/// Where the worm sits: the barrel's model's point for it, the barrel standing unturned at its own size
[[nodiscard]] glm::vec3 WormSpot(const glm::vec3& barrel, const glm::vec3& modelPoint);

/// The worm's place in its clip after so many milliseconds of the game's clock: a looping clip comes round again, any
/// other stops a millisecond short of its end
[[nodiscard]] uint32_t StepWorm(uint32_t place, uint32_t elapsed, uint32_t playTime, bool looping);

} // namespace openblack::apple_barrel
