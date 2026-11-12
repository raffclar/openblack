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

namespace openblack::landgen
{
/// Mixes the bits of a 64 bit value, so that near values give values that look unrelated (the SplitMix64 finaliser)
[[nodiscard]] constexpr uint64_t Mix(uint64_t value)
{
	value ^= value >> 30;
	value *= 0xBF58476D1CE4E5B9ull;
	value ^= value >> 27;
	value *= 0x94D049BB133111EBull;
	value ^= value >> 31;
	return value;
}

/// A value that only the seed and the two whole numbers give, for the lattice of the noise
[[nodiscard]] constexpr uint32_t Hash(uint64_t seed, int32_t x, int32_t z)
{
	const auto ux = static_cast<uint64_t>(static_cast<uint32_t>(x));
	const auto uz = static_cast<uint64_t>(static_cast<uint32_t>(z));
	return static_cast<uint32_t>(Mix(seed ^ Mix((ux << 32) | uz)) >> 32);
}

/// The seed of one stage of the generator, from the land's seed and the stage's own number
[[nodiscard]] constexpr uint64_t StageSeed(uint64_t seed, uint64_t stage)
{
	return Mix(seed + (stage * 0x9E3779B97F4A7C15ull));
}

/// A generator of random numbers (PCG32). It gives the same numbers on every platform for the same seed, unlike the
/// distributions of the standard library.
class Random
{
public:
	explicit constexpr Random(uint64_t seed)
	    : _state(0)
	    , _increment((Mix(seed ^ 0xDA3E39CB94B95BDBull) << 1u) | 1u)
	{
		Next();
		_state += Mix(seed);
		Next();
	}

	/// The next 32 random bits
	constexpr uint32_t Next()
	{
		const uint64_t old = _state;
		_state = (old * 6364136223846793005ull) + _increment;
		const auto shifted = static_cast<uint32_t>(((old >> 18u) ^ old) >> 27u);
		const auto rotation = static_cast<uint32_t>(old >> 59u);
		return (shifted >> rotation) | (shifted << ((32u - rotation) & 31u));
	}

	/// A number from 0 to below 1, in steps of 2^-24, which a float holds exactly
	constexpr float NextUnit() { return static_cast<float>(Next() >> 8u) * (1.0f / 16777216.0f); }

	/// A number from low to below high
	constexpr float NextRange(float low, float high) { return low + ((high - low) * NextUnit()); }

	/// A whole number from 0 to below count, which must not be 0
	constexpr uint32_t NextBelow(uint32_t count)
	{
		return static_cast<uint32_t>((static_cast<uint64_t>(Next()) * static_cast<uint64_t>(count)) >> 32u);
	}

private:
	uint64_t _state;
	uint64_t _increment;
};
} // namespace openblack::landgen
