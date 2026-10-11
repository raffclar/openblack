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

#include <optional>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include "3D/WaterRings.h"

/// How a shark is drawn between its turns and how its wake spreads
namespace openblack::ecs::shark_rules
{

/// A wake ring comes once more than this many milliseconds have gone since the last
inline constexpr int32_t k_WakeInterval = 50;
/// A shark's colour under the sea, 0x303070, which the game's light shades as it does every model's
inline constexpr glm::vec3 k_Colour {48.0f, 48.0f, 112.0f};
/// A shark is drawn in two parts cut at the sea's level
inline constexpr float k_SeaLevel = 0.0f;

/// How a shark is drawn in a frame. What is under the sea is drawn in its own dark blue beneath the sea's surface, so
/// the sea is blended over it and it shows through as the sea lets it. What breaks the surface (the fins and back) is
/// drawn over the sea in the land's brightest light, as other models are, but only while a script walks it along a
/// path; once there, only its dark shape under the sea is left.
struct Look
{
	bool aboveSea {false};
	glm::vec3 underSea {k_Colour};
};
[[nodiscard]] Look LookOf(bool walkingAPath);

/// The way a shark faces, radians from +x towards +z: the way it moved this turn, or the way it faced if it didn't move
/// across the land
[[nodiscard]] float Heading(const glm::vec3& turnStart, const glm::vec3& position, float facing);

/// How a shark's model is turned for its heading: its model's +x lies along the heading, so its nose leads the way it
/// swims
[[nodiscard]] glm::mat3 Facing(float heading);

/// Where a shark is drawn `turnFraction` of the way through its turn, each end at the given height
[[nodiscard]] glm::vec3 Drawn(const glm::vec3& turnStart, float startHeight, const glm::vec3& position, float endHeight,
                              float turnFraction);

/// The wake's ring for a frame of `frameMilliseconds`, when one is due: on the water under `point`, turned with the
/// shark. Every shark's frames count towards the one timer, so two sharks share the rings between them.
[[nodiscard]] std::optional<water_rings::Ring> WakeRing(int32_t& timer, const glm::vec3& point, float heading,
                                                        int32_t frameMilliseconds);

} // namespace openblack::ecs::shark_rules
