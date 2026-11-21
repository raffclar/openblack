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

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The soft glow of the whole screen under a good sky, with the weather option. After each frame of the land, the game
/// takes 16 by 16 points of the finished screen, the interface and the last glow too. Each channel of each point eases
/// an eighth of the way to the screen's colour in whole steps. The next frame adds these points over the whole screen,
/// filtered between them and times the darkness of the good sky. Thus the glow follows the screen slowly, and adds to
/// itself.
///
/// The renderer does these steps on the GPU (shaders screen_glow.sh, fs_screen_glow.sc and fs_screen_glow_step.sc). The
/// functions here are the same rules on the processor, for the tests.
namespace openblack::graphics::screen_glow
{

/// The points across and down the screen
constexpr int k_Side = 16;
/// A channel of the screen as a point keeps it: the 8 bits of the channel moved up by this many bits
constexpr int k_ChannelShift = 12;
/// The highest value of a channel of a point, a little under the top level of 4 bits. The game keeps the value at most
/// this, thus the top level never overflows into the next channel.
constexpr int k_ChannelMost = 0xF7000;
/// A channel of the glow has 4 bits: levels 0 to 15
constexpr int k_TopLevel = 15;

/// The pixel of the screen, from the top left, that the point at column and row of the 16 by 16 points reads
[[nodiscard]] glm::ivec2 SamplePoint(glm::ivec2 screen, glm::ivec2 point);

/// A channel of a point after one step towards the screen's channel of 8 bits: an eighth of the difference, cut towards
/// zero, then kept from 0 to k_ChannelMost
[[nodiscard]] int Step(int value, uint8_t screenChannel);

/// The levels of 4 bits of the red, green and blue channels of a point, as the game writes them into its texture of the
/// glow. Blue goes to the nearest level. Red and green keep their top bits after a much smaller addition, thus they
/// stay at the lower level nearly always (vanilla-bugs row 2430, kept).
[[nodiscard]] glm::ivec3 Levels(glm::ivec3 value);

/// Where a pixel of the screen, from the top left, falls among the 16 by 16 points, in points: 0 at the first pixel
/// and 15 at the edge after the last pixel. The game spreads the points from the first pixel to the far edge.
[[nodiscard]] glm::vec2 PointsAt(glm::vec2 pixel, glm::vec2 screen);

/// A channel of the screen, of 8 bits, with the glow added: the frame's channel plus the filtered level of the points,
/// of 15, times the strength, of 255. The screen keeps at most 255.
[[nodiscard]] uint8_t Shown(uint8_t frameChannel, float level, uint8_t strength);

/// The strength of the glow, of 255: the good sky's darkness, or none without the weather option. The game draws the
/// glow and takes its points only with a strength above 0, and only in a frame that draws the land.
[[nodiscard]] uint8_t Strength(uint8_t darkness, bool weather);

} // namespace openblack::graphics::screen_glow
