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

#include <functional>
#include <optional>

#include <glm/vec3.hpp>

#include "3D/AllMeshes.h"
#include "3D/WaterRings.h"

/// How the game draws a villager that a script draws in high detail while it swims, such as the boy in the opening.
/// The game draws the villager as usual. It then draws the part of the villager under the sea again, in the dark blue
/// of the sharks, beneath the sea's surface, so that the sea blends over it. Each second of the frames that draw such a
/// swimmer, one ring comes on the water.
namespace openblack::ecs::swimmer_rules
{

/// The clip that makes a villager in high detail a swimmer
inline constexpr AnimId k_SwimClip = AnimId::PSwim2;
/// The colour of a swimmer under the sea, 0x303070, which the light of the game shades as it does every model
inline constexpr glm::vec3 k_UnderSeaColour {48.0f, 48.0f, 112.0f};
/// A ring comes when the frames since the last ring have more than this many milliseconds
inline constexpr int32_t k_RingInterval = 1000;
/// The ring takes its place across the land from this bone of the pose. The game reads the place from the pose before
/// it puts the pose in the world, so the ring comes near the corner of the map and not at the swimmer.
inline constexpr size_t k_RingBone = 5;

/// Whether the game draws a villager as a swimmer: it is in high detail and plays the swim clip
[[nodiscard]] bool Swims(bool highDetail, AnimId clip);

/// The ring of a frame that draws swimmers, when one is due. All the swimmers share the one timer: it counts the
/// milliseconds of each frame, and when it passes the interval it starts again at 0. `bonePlace` is the place of the
/// ring bone in the pose, before the pose goes into the world. `angle` is the turn of the ring, which the game draws
/// from its random numbers only when a ring is due.
[[nodiscard]] std::optional<water_rings::Ring> Ring(int32_t& timer, int32_t frameMilliseconds, const glm::vec3& bonePlace,
                                                    const std::function<float()>& angle);

} // namespace openblack::ecs::swimmer_rules
