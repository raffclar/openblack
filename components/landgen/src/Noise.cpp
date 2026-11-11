/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "landgen/Noise.h"

#include <cmath>

#include <array>

#include "landgen/Random.h"

using namespace openblack::landgen;

namespace
{
struct Direction
{
	float x;
	float z;
};
constexpr float k_Diagonal = 0.70710678f;
/// The gradients of the lattice: the axes and the diagonals
constexpr std::array<Direction, 8> k_Gradients {{{1.0f, 0.0f},
                                                 {-1.0f, 0.0f},
                                                 {0.0f, 1.0f},
                                                 {0.0f, -1.0f},
                                                 {k_Diagonal, k_Diagonal},
                                                 {-k_Diagonal, k_Diagonal},
                                                 {k_Diagonal, -k_Diagonal},
                                                 {-k_Diagonal, -k_Diagonal}}};

float Corner(uint64_t seed, int32_t ix, int32_t iz, float fx, float fz)
{
	const auto& gradient = k_Gradients[Hash(seed, ix, iz) & 7u];
	return (gradient.x * fx) + (gradient.z * fz);
}

float Lerp(float a, float b, float t)
{
	return a + ((b - a) * t);
}
} // namespace

float noise::Gradient(uint64_t seed, float x, float z)
{
	const float floorX = std::floor(x);
	const float floorZ = std::floor(z);
	const auto ix = static_cast<int32_t>(floorX);
	const auto iz = static_cast<int32_t>(floorZ);
	const float fx = x - floorX;
	const float fz = z - floorZ;

	const float n00 = Corner(seed, ix, iz, fx, fz);
	const float n10 = Corner(seed, ix + 1, iz, fx - 1.0f, fz);
	const float n01 = Corner(seed, ix, iz + 1, fx, fz - 1.0f);
	const float n11 = Corner(seed, ix + 1, iz + 1, fx - 1.0f, fz - 1.0f);

	const float u = Fade(fx);
	const float v = Fade(fz);
	// The largest value of this noise is about 0.7: scale it to about 1
	return Lerp(Lerp(n00, n10, u), Lerp(n01, n11, u), v) * 1.4142135f;
}

float noise::Fractal(uint64_t seed, float x, float z, int octaves)
{
	float sum = 0.0f;
	float amplitude = 1.0f;
	float total = 0.0f;
	float frequency = 1.0f;
	for (int octave = 0; octave < octaves; ++octave)
	{
		sum += Gradient(StageSeed(seed, static_cast<uint64_t>(octave)), x * frequency, z * frequency) * amplitude;
		total += amplitude;
		amplitude *= 0.5f;
		frequency *= 2.0f;
	}
	return sum / total;
}

float noise::Ridged(uint64_t seed, float x, float z, int octaves)
{
	float sum = 0.0f;
	float amplitude = 1.0f;
	float total = 0.0f;
	float frequency = 1.0f;
	float weight = 1.0f;
	for (int octave = 0; octave < octaves; ++octave)
	{
		float crest = 1.0f - std::fabs(Gradient(StageSeed(seed, static_cast<uint64_t>(octave)), x * frequency, z * frequency));
		crest = crest * crest * weight;
		weight = crest < 1.0f ? crest : 1.0f;
		sum += crest * amplitude;
		total += amplitude;
		amplitude *= 0.5f;
		frequency *= 2.0f;
	}
	return sum / total;
}
