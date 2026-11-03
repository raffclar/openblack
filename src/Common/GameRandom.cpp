/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameRandom.h"

#include <cmath>
#include <cstdint>

#include <bit>

using namespace openblack;

namespace
{
/// The game compares with 0 in a way that a NaN also passes
bool ZeroOrNaN(float x) noexcept
{
	return x == 0.0f || std::isnan(x);
}

/// Particles' (u * k) * x, the other way round from the game's (u * x) * k
float ParticleFloat(uint32_t u, float x) noexcept
{
	float r = static_cast<float>(u);
	r = r * game_random::k_FloatRandScale;
	r = r * x;
	return r;
}
} // namespace

double game_random::RoundTo24Bits(double value) noexcept
{
	// The 52 stored bits of a normal double keep their top 23 bits. The low 29 bits round to the nearest, to even at a tie.
	// A carry out of the stored bits goes into the exponent, which gives the next power of 2.
	constexpr uint64_t k_DroppedBits = 29;
	constexpr uint64_t k_DroppedMask = (uint64_t {1} << k_DroppedBits) - 1;
	constexpr uint64_t k_ExponentMask = uint64_t {0x7FF} << 52;
	const auto bits = std::bit_cast<uint64_t>(value);
	const auto exponent = bits & k_ExponentMask;
	if (exponent == k_ExponentMask)
	{
		// Infinite or NaN
		return value;
	}
	if (exponent == 0)
	{
		// Zero, or a subnormal double, whose precision is less than its stored bits show
		if (value == 0.0)
		{
			return value;
		}
		int power = 0;
		const double mantissa = std::frexp(value, &power);
		return std::ldexp(std::nearbyint(std::ldexp(mantissa, 24)), power - 24);
	}
	const uint64_t lowestKept = (bits >> k_DroppedBits) & 1;
	return std::bit_cast<double>((bits + (k_DroppedMask >> 1) + lowestKept) & ~k_DroppedMask);
}

float game_random::FloatOf(uint32_t u, float x) noexcept
{
	// Each product of a 16-bit draw and two floats is exact in a double. Each step then rounds to the 24 bits of the
	// FPU, with the FPU's wide exponent.
	double r = game_random::RoundTo24Bits(static_cast<double>(u) * static_cast<double>(x));
	r = game_random::RoundTo24Bits(r * static_cast<double>(k_FloatRandScale));
	return static_cast<float>(r);
}

float game_random::FloatRand(float x, uint32_t& seed) noexcept
{
	if (ZeroOrNaN(x))
	{
		return 0.0f;
	}
	return FloatOf(LHRand(k_FloatRandRange, seed), x);
}

float GameRandomInterface::GameFloatRange(float a, float b, DrawSite site)
{
	const float range = b - a;
	const float r = GameFloatRand(range, site);
	return r + a;
}

float GameRandomInterface::CrtRandom(float a, float b)
{
	float r = static_cast<float>(CrtRand());
	r = r * game_random::k_CrtRandomScale;
	const float range = b - a;
	r = r * range;
	return r + a;
}

float GameRandomInterface::ParticleFloatRand(float x, DrawSite site)
{
	switch (GetParticleStream())
	{
	case ParticleRandomStream::Synced:
		return ParticleFloat(GameRand(game_random::k_FloatRandRange, site), x);
	case ParticleRandomStream::Local:
		return ParticleFloat(LocalRand(static_cast<int32_t>(game_random::k_FloatRandRange)), x);
	case ParticleRandomStream::None:
		break;
	}
	return 0.0f;
}

float GameRandomInterface::ParticleFloatRand(float a, float b, DrawSite site)
{
	const float range = b - a;
	const float r = ParticleFloatRand(range, site);
	return r + a;
}

int32_t GameRandomInterface::ParticleRand(int32_t n, DrawSite site)
{
	switch (GetParticleStream())
	{
	case ParticleRandomStream::Synced:
		return static_cast<int32_t>(GameRand(static_cast<uint32_t>(n), site));
	case ParticleRandomStream::Local:
		return static_cast<int32_t>(LocalRand(n));
	case ParticleRandomStream::None:
		break;
	}
	return 0;
}

glm::vec3 GameRandomInterface::ParticleRandR3(DrawSite site)
{
	if (GetParticleStream() == ParticleRandomStream::None)
	{
		return glm::vec3(-1.0f);
	}
	glm::vec3 p;
	float sum = 0.0f;
	do
	{
		p.x = ParticleFloatRand(2.0f, site) - 1.0f;
		p.y = ParticleFloatRand(2.0f, site) - 1.0f;
		p.z = ParticleFloatRand(2.0f, site) - 1.0f;
		// (z z + y y) + x x
		const float zz = p.z * p.z;
		const float yy = p.y * p.y;
		const float xx = p.x * p.x;
		sum = zz + yy;
		sum = sum + xx;
	} while (sum > 1.0f);
	return p;
}

ParticleRandomStep::ParticleRandomStep(GameRandomInterface& random, bool synced) noexcept
    : _random(random)
{
	_random.SetParticleStream(synced ? ParticleRandomStream::Synced : ParticleRandomStream::Local);
}

ParticleRandomStep::~ParticleRandomStep()
{
	_random.SetParticleStream(ParticleRandomStream::None);
}
