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

#include <array>
#include <vector>

#include <glm/vec3.hpp>

// The rain the engine draws where the atmosphere grid says it rains: one set of 128 streaks (lines from 50 m under the
// ground up to the cloud height, a dashed row of atmos.raw scrolling along them) repeated over 160 m tiles, one per
// land block near the camera, as dense and opaque as the rain of the block's four middle cells. This file keeps the
// streaks and picks the tiles; Graphics/RendererRain.cpp draws them.

namespace openblack::weather::rain
{
constexpr int32_t k_Drops = 128; ///< 128 streaks

/// One streak
struct Drop
{
	float phase {0.0f}; ///< 0..1 at 2.4 per second: fades in over the first 5 %, out over the last; a new place when it wraps
	float x {0.0f};     ///< -80..80 from the tile centre
	float z {0.0f};
	float dx {0.0f}; ///< -15..15: the top end's offset (the slant)
	float dz {0.0f};
	float scroll {0.0f}; ///< 0..1: the texture's u at the bottom (u + 1 at the top)
	float speed {0.0f};  ///< 0.1..0.2 texture turns per second (x the fall speed)
};

/// One tile to draw (picked, then Z-sorted, then drawn)
struct Tile
{
	glm::vec3 origin {0.0f}; ///< the block's centre on its 80 m grid, at the land height there
	int32_t drops {0};       ///< 128, fewer from 100 m to 400 m away
	int32_t alpha {0};       ///< the bottom end's alpha: max rain of the four cells x 88 / 100, faded with the distance
	int32_t alphaTop {0};    ///< alpha / ((2 d / 400 + 1) x 5)
};

/// The atmosphere's rain object: 128 streaks at random places
void Reset();
/// With the frame's seconds: the nearest storm to the camera sets where the streaks start (its elevation, 160 without
/// a storm) and how fast they scroll (its fall speed, 1), each moving 0.3 of the way per frame (40..640 m, 0.3..5);
/// then the streaks step, if the rain was drawn last frame
void Update(float seconds, const glm::vec3& camera);
/// The rain half of the atmosphere's draw: the tiles of the land blocks within 560 m whose four middle cells rain
/// more than 5, and the tile's distance fade (nothing from 400 m)
[[nodiscard]] std::vector<Tile> CollectTiles(const glm::vec3& camera);
/// The renderer drew the streaks this frame: the next Update steps them
void MarkDrawn();

[[nodiscard]] const std::array<Drop, k_Drops>& Drops();
/// The streaks' top above the tile's ground (160 at start)
[[nodiscard]] float Elevation();
/// The scroll speed factor (1 at start)
[[nodiscard]] float FallSpeed();
/// The per-streak fade: phase < 0.05 -> phase x 20, > 0.95 -> (1 - phase) x 20, else 1
[[nodiscard]] float PhaseFade(float phase);
} // namespace openblack::weather::rain
