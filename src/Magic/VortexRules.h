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
#include <span>

#include <glm/vec2.hpp>

#include "Enums.h"

// The swirling vortex between the lands: when it opens and closes, and how it levels the ground it opens on. A vortex
// fades in (2 seconds of nothing, then 5 seconds easing open) or starts fully open, and when told to fade out eases
// shut over 5 seconds and is gone 2 seconds later. While it opens, the land vortices (not the volcano's mouth) pull an
// 11 by 11 cell square of the land towards the square's average height, wholly within 50 of the middle and fading to
// nothing at 56. The levelling only ever grows and is never undone. Pure rules, tested without the game.

namespace openblack::vortex
{

/// How far a vortex pulls things in, whatever its kind
inline constexpr float k_PullRadius = 50.0f;
/// A fading vortex waits this long before it starts to change
inline constexpr float k_FadeDelaySeconds = 2.0f;
/// Then it eases open or shut over this long
inline constexpr float k_FadeSeconds = 5.0f;
/// A fade is over, and the vortex fully open or gone, once more than this much time has passed
inline constexpr float k_FadeOverSeconds = k_FadeDelaySeconds + k_FadeSeconds;

/// The levelled ground's reach: the land is pulled fully to the average within the inner radius, less and less out to
/// the outer one, and not at all beyond it
inline constexpr float k_LevelReach = 58.0f;
inline constexpr float k_LevelFullRadius = 50.0f;
inline constexpr float k_LevelOuterRadius = k_LevelReach - 2.0f;
/// The levelled square: cells either side of the vortex's own cell, and along each side
inline constexpr int k_LevelHalfSide = 5;
inline constexpr int k_LevelSide = k_LevelHalfSide * 2 + 1;
inline constexpr size_t k_LevelCellCount = static_cast<size_t>(k_LevelSide) * k_LevelSide;

/// The hole a vortex opens in the land never takes in the most opaque part of its texture
inline constexpr uint8_t k_GroundHoleMaxThreshold = 235;
/// The hole's and the ring's textures span this many land units times the kind's base size, centred on the vortex
inline constexpr float k_GroundTextureSpan = 160.0f;

/// Seconds since a vortex's state began: whole game turns since then and the part of the current one, at the game's
/// turn length. Kept in double: the end of a fade is judged on it unrounded, the fades themselves on it as a float.
[[nodiscard]] double ElapsedSeconds(uint32_t turnsSince, float turnFraction, uint32_t millisecondsPerTurn);

/// How open a vortex is, 0 to 1, eased in and out
[[nodiscard]] float Openness(VortexStateType state, float seconds);

/// How far the levelling has gone, 0 to 1: the openness eased out (one minus the square of what is left), and complete
/// once the vortex is fading out
[[nodiscard]] float LevelAmount(VortexStateType state, float seconds);

/// What a vortex does at the end of a game turn
enum class Step
{
	Stay,
	/// Its fade-in is over: it is fully open
	Open,
	/// Its fade-out is over: it is gone
	Remove,
};
[[nodiscard]] Step Advance(VortexStateType state, double seconds);

/// The glow the vortex lays on the ground, 0 to 1: rising to full over the delay of a fade-in and settling to 0.6 as it
/// opens; rising back to full as it fades out, then dying away over the last 2 seconds
[[nodiscard]] float GlowBrightness(VortexStateType state, float seconds);

/// How opaque the hole texture must be, 0 to 255, for the land to be drawn there: the openness in 255ths, rounded down,
/// and never more than 235. The hole widens from the texture's clearest strokes as the vortex opens.
[[nodiscard]] uint8_t GroundHoleThreshold(float openness);

/// Where a point of the land falls on the hole's and the ring's textures: across them with the world's z, down them
/// with its x, the vortex in the middle, a whole texture spanning 160 units times the kind's base size
[[nodiscard]] glm::vec2 GroundTextureCoordinates(glm::vec2 point, glm::vec2 centre, float baseScale);

/// How far below the ground the swirl drawn under the land sits: 2.5 while the levelling hasn't begun, rising to 0.3
[[nodiscard]] float SwirlDepth(float levelAmount);

/// Whether a kind of vortex levels the ground: the land vortices do, the volcano's mouth does not
[[nodiscard]] constexpr bool LevelsGround(VortexType type)
{
	return type != VortexType::Volcano;
}

/// The map cell a vortex at a point on the land levels round: its position in cells, rounded towards zero
[[nodiscard]] glm::ivec2 CentreCell(glm::vec2 centre);

/// The cells of the levelled square, x major: the first cell's x and z, then along z for each x
[[nodiscard]] glm::ivec2 SquareCell(glm::ivec2 centreCell, size_t index);

/// The square's average height: every cell counts, cells off the land as 0
[[nodiscard]] float AverageAltitude(std::span<const uint8_t> altitudes);

/// How much of the way to the average a cell at a distance from the middle is pulled, before the levelling's amount
[[nodiscard]] float LevelWeight(float distance);

/// The levelled height of a cell that was at an altitude when the vortex first opened, at a distance from the middle
[[nodiscard]] uint8_t LevelledAltitude(uint8_t original, float average, float distance, float amount);

/// The square's levelled heights, x major as its cells: each cell from its height when the vortex first opened, the
/// square's average, its distance from the vortex's middle and the levelling's amount
[[nodiscard]] std::array<uint8_t, k_LevelCellCount> LevelSquare(glm::vec2 centre, std::span<const uint8_t> originals,
                                                                float average, float amount);

} // namespace openblack::vortex
