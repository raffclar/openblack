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

/// The rules of a puzzle totem that rises out of the land and sinks back into it, such as the two totems of the undead
/// village. Its height is a whole number from 0 to its greatest height. At its greatest height it stands on the land.
/// At height 0 it is under the land by its sink depth, a part of its model's height.
///
/// The hand takes hold of the totem and slides it up and down.
namespace openblack::ecs::puzzle_totem
{

/// The part of the model's height that stays above the land when the totem is fully down
inline constexpr float k_ShownWhenDown = 0.35f;
/// A slide of the hand moves the totem by the slide divided by this
inline constexpr float k_SlideDivisor = 1.5f;
/// The greatest height of a totem that only takes taps, as a bell of the lion maze
inline constexpr int32_t k_TapOnly = -1;

/// How far the totem goes under the land at height 0: the part of its model's height that is not shown when down. The
/// half height is half of the model's height before its scale.
[[nodiscard]] float SinkDepth(float halfHeight, float scale);

/// The part of its greatest height that a totem stands at. A totem that only takes taps always stands fully up.
[[nodiscard]] float ShareOf(int32_t height, int32_t highest);

/// How far below its place on the land the totem stands for a share of its greatest height: none at all of it, the
/// whole sink depth at none of it. The value is zero or less.
[[nodiscard]] float OffsetFor(float share, float depth);

/// The height for a share of the greatest height: the whole part, towards zero, at most the greatest height
[[nodiscard]] int32_t HeightForShare(float share, int32_t highest);

/// The share that a totem stands at, from how far it is below its place: between none and all
[[nodiscard]] float ShareForOffset(float offset, float depth);

/// Where the hand holds the totem, as an offset below its place, after a slide up (more than zero) or down: kept
/// between the whole sink depth and none
[[nodiscard]] float HoldAfterSlide(float hold, float slide, float depth);

/// How far below its place the totem stands while the hand holds it at a height: the sink depth times the part of the
/// greatest height that it is short of. The value is zero or less.
[[nodiscard]] float HoldOffset(int32_t height, int32_t highest, float depth);

/// The height that the hand holds the totem at, for its hold: the whole part, towards zero, at most the greatest height
[[nodiscard]] int32_t HeightForHold(float hold, float depth, int32_t highest);

} // namespace openblack::ecs::puzzle_totem
