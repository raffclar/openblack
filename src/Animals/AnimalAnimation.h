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

#include <span>

#include <glm/mat4x4.hpp>

// How an animal plays its clip: each animal keeps its own place in its clip, in milliseconds. Standing, the clip plays
// by the clock; moving, by the ground the animal covers, a play of the clip for each stride of its scaled model. The
// pose is the clip's keyframes, spread evenly over its play time, blended between the two around that place. Pure
// functions, tested on their own.

namespace openblack::animals
{

/// What a clip says about how it plays
struct ClipTiming
{
	/// Milliseconds one play lasts
	uint32_t playTime {0};
	size_t frameCount {0};
	/// It plays round and round, else it holds its last pose
	bool looping {false};
	/// It plays by the clock even while its animal moves
	bool playedByTime {false};
	/// How far one play carries the animal, in the model's units
	float stride {0.0f};
};

/// The place in a clip after it has played on: past the end a looping clip comes round again, any other holds its end
[[nodiscard]] uint32_t AdvanceClip(const ClipTiming& clip, uint32_t place, int32_t played);

/// How far into its clip a moving animal gets in a frame of so many milliseconds: the metres its speed takes it,
/// shrunk by its scale, as a share of the clip's stride, of the clip's play time. The speed is the tables' speed state
/// (65536ths of a metre each tenth of a second, over ten).
[[nodiscard]] int32_t MovingPlay(const ClipTiming& clip, uint16_t speedState, uint32_t frameMilliseconds, float scale);

/// A moving play in whole milliseconds with the part of a millisecond the frame left over, which the next frame takes on
struct CarriedPlay
{
	int32_t played {0};
	/// The part of a millisecond of the clip the ground covered but the whole milliseconds didn't take, under one
	float carry {0.0f};
};

/// The same, with the part of a millisecond left over from the frame before added in and the new leftover kept, so that
/// the clip keeps pace with the ground however many frames a second are drawn
[[nodiscard]] CarriedPlay MovingPlayCarried(const ClipTiming& clip, uint16_t speedState, uint32_t frameMilliseconds,
                                            float scale, float carry);

/// How far an animal's clip plays in a frame: while the animal moves and its clip plays by the ground, the ground it
/// covers with the part of a millisecond left over from the frame before (so its legs keep pace with the ground at any
/// frame rate); else the frame's milliseconds by the clock, the leftover kept for when it next moves
[[nodiscard]] CarriedPlay FramePlay(const ClipTiming& clip, bool moving, uint16_t speedState, uint32_t frameMilliseconds,
                                    float scale, float carry);

/// The two keyframes a place in a clip falls between and how far between them: a looping clip's keyframes are spread
/// over its whole play time and its last blends into its first, any other's run from the first at its start to the
/// last at its end
struct KeyframeSpan
{
	size_t from {0};
	size_t to {0};
	float t {0.0f};
};
[[nodiscard]] KeyframeSpan SpanAt(const ClipTiming& clip, uint32_t place);

/// A model posed between two keyframes: each bone's matrix blended element by element between its two keyframes, then
/// placed by its parent's, which comes before it (a root bone's parent is the largest index). The bones written are
/// as many as there are parents; both keyframes have at least that many.
void PoseBetween(std::span<const glm::mat4> from, std::span<const glm::mat4> to, float t, std::span<const uint32_t> parents,
                 std::span<glm::mat4> bones);

/// The game's clock for drawing things between turns: the turn's start plus the milliseconds into it, at most 99
[[nodiscard]] uint32_t DrawTime(uint32_t turn, float turnFraction);

} // namespace openblack::animals
