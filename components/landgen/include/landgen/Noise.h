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

/// Gradient noise on a square lattice. It uses only additions, multiplications and std::floor, so that it gives the same
/// values on every platform.
namespace openblack::landgen::noise
{
/// The smooth step from 0 at 0 to 1 at 1, with no slope and no curve at either end
[[nodiscard]] constexpr float Fade(float t)
{
	return t * t * t * ((t * ((t * 6.0f) - 15.0f)) + 10.0f);
}

/// The smooth step from 0 at edge0 to 1 at edge1, flat outside them
[[nodiscard]] constexpr float SmoothStep(float edge0, float edge1, float value)
{
	float t = (value - edge0) / (edge1 - edge0);
	t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
	return t * t * (3.0f - (2.0f * t));
}

/// Gradient noise at (x, z), from about -1 to 1. It is 0 on each point of the lattice.
[[nodiscard]] float Gradient(uint64_t seed, float x, float z);

/// The sum of octaves of gradient noise, each at twice the frequency and half the amplitude of the one before. The
/// result is from about -1 to 1.
[[nodiscard]] float Fractal(uint64_t seed, float x, float z, int octaves);

/// Ridged noise from 0 to about 1: sharp crests where the gradient noise crosses 0, as on the ridges of mountains. Each
/// octave has more weight where the octaves before it make a crest.
[[nodiscard]] float Ridged(uint64_t seed, float x, float z, int octaves);
} // namespace openblack::landgen::noise
