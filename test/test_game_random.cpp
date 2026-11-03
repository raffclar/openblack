/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <bit>
#include <limits>
#include <random>

#include <gtest/gtest.h>

#define LOCATOR_IMPLEMENTATIONS
#include <Common/GameRandomProduction.h>

using openblack::GameRandomProduction;
using openblack::GameRandomSeeds;
using openblack::ParticleRandomStep;
using openblack::ParticleRandomStream;
namespace game_random = openblack::game_random;

TEST(GameRandom, DrawsAsTheGameDoes)
{
	GameRandomProduction random;
	EXPECT_EQ(random.GameRand(100), 78);
	EXPECT_EQ(random.GameRand(100), 48);
	EXPECT_EQ(random.GameRand(7), 5);
	EXPECT_EQ(random.GetSeeds().synced, 0x47184E08u);
	// the local seed is its own
	EXPECT_EQ(random.GetSeeds().local, game_random::k_InitialSeed);
	EXPECT_EQ(random.LocalRand(100), 78);
}

TEST(GameRandom, ZeroDrawsNothing)
{
	GameRandomProduction random;
	EXPECT_EQ(random.GameRand(0), 0);
	EXPECT_EQ(random.LocalRand(0), 0);
	EXPECT_EQ(random.GameFloatRand(0.0f), 0.0f);
	EXPECT_EQ(random.GameFloatRand(std::numeric_limits<float>::quiet_NaN()), 0.0f);
	EXPECT_EQ(random.LocalFloatRand(-0.0f), 0.0f);
	EXPECT_EQ(random.GetSeeds(), GameRandomSeeds {});
}

TEST(GameRandom, FloatsScaleASixteenBitDraw)
{
	GameRandomProduction random;
	// u = 45003: (u * 10) * k
	EXPECT_EQ(random.GameFloatRand(10.0f), 6.8670177f);
	random.SetSeeds({game_random::k_InitialSeed, game_random::k_InitialSeed});
	EXPECT_EQ(random.GameFloatRand(-10.0f), -6.8670177f);
	EXPECT_EQ(random.LocalFloatRand(10.0f), 6.8670177f);
}

namespace
{
/// The rounding to 24 bits by the double's parts, as the FPU at 24-bit precision rounds: the fake to check against
double RoundTo24BitsByParts(double value)
{
	if (value == 0.0 || !std::isfinite(value))
	{
		return value;
	}
	int exponent = 0;
	const double mantissa = std::frexp(value, &exponent);
	return std::ldexp(std::nearbyint(std::ldexp(mantissa, 24)), exponent - 24);
}

void ExpectSameBits(double value)
{
	EXPECT_EQ(std::bit_cast<uint64_t>(game_random::RoundTo24Bits(value)), std::bit_cast<uint64_t>(RoundTo24BitsByParts(value)))
	    << std::hexfloat << value;
}
} // namespace

TEST(GameRandom, RoundingTo24BitsIsTheRoundingByParts)
{
	constexpr double k_Max = std::numeric_limits<double>::max();
	constexpr double k_Denormal = std::numeric_limits<double>::denorm_min();
	for (const double value :
	     {0.0, -0.0, 1.0, -1.0, k_Max, -k_Max, k_Denormal, -k_Denormal, 1e-310, -3e-312, std::numeric_limits<double>::min(),
	      std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
	{
		ExpectSameBits(value);
	}
	// The dropped bits at a tie, a bit under it and a bit over it, with an even and an odd last kept bit, and all the
	// dropped bits set, which carries into the exponent
	for (const uint64_t kept : {0x3FF0000000000000ull, 0x3FF0000020000000ull, 0x3FFFFFFFE0000000ull, 0xC00FFFFFE0000000ull,
	                            0x7FEFFFFFE0000000ull, 0x0010000000000000ull})
	{
		for (const uint64_t dropped : {0x0ull, 0x1ull, 0xFFFFFFFull, 0x10000000ull, 0x10000001ull, 0x1FFFFFFFull})
		{
			ExpectSameBits(std::bit_cast<double>(kept | dropped));
		}
	}
	std::mt19937_64 bits(0x5EED);
	for (int i = 0; i < 200000; ++i)
	{
		const auto value = std::bit_cast<double>(bits());
		if (!std::isnan(value))
		{
			ExpectSameBits(value);
		}
	}
	EXPECT_TRUE(std::isnan(game_random::RoundTo24Bits(std::numeric_limits<double>::quiet_NaN())));
}

TEST(GameRandom, FloatsOfEveryDrawRoundByParts)
{
	for (const float x : {1.0f, -1.0f, 10.0f, 0.3f, 255.0f, 1e-30f, 3e38f, -7.25e5f, 1e-44f})
	{
		for (uint32_t u = 0; u <= game_random::k_FloatRandRange; ++u)
		{
			double r = RoundTo24BitsByParts(static_cast<double>(u) * static_cast<double>(x));
			r = RoundTo24BitsByParts(r * static_cast<double>(game_random::k_FloatRandScale));
			ASSERT_EQ(std::bit_cast<uint32_t>(game_random::FloatOf(u, x)), std::bit_cast<uint32_t>(static_cast<float>(r)))
			    << u << " " << x;
		}
	}
}

TEST(GameRandom, CrtRandIsMsvcs)
{
	GameRandomProduction random;
	EXPECT_EQ(random.CrtRand(), 41);
	EXPECT_EQ(random.CrtRand(), 18467);
	EXPECT_EQ(random.CrtRand(), 6334);
	random.CrtSrand(1);
	EXPECT_EQ(random.CrtRand(), 41);
}

TEST(GameRandom, ParticlesDrawOnlyInAStep)
{
	GameRandomProduction random;
	EXPECT_EQ(random.ParticleFloatRand(10.0f), 0.0f);
	EXPECT_EQ(random.ParticleRand(100), 0);
	EXPECT_EQ(random.GetSeeds(), GameRandomSeeds {});
	{
		const ParticleRandomStep step(random, true);
		EXPECT_EQ(random.GetParticleStream(), ParticleRandomStream::Synced);
		EXPECT_EQ(random.ParticleFloatRand(10.0f), 6.8670177f);
		EXPECT_EQ(random.GetSeeds().local, game_random::k_InitialSeed);
	}
	EXPECT_EQ(random.GetParticleStream(), ParticleRandomStream::None);
	{
		const ParticleRandomStep step(random, false);
		EXPECT_EQ(random.ParticleRand(100), 78);
		EXPECT_NE(random.GetSeeds().local, game_random::k_InitialSeed);
	}
}

TEST(GameRandom, ParticlePointsAreInTheUnitBall)
{
	GameRandomProduction random;
	EXPECT_EQ(random.ParticleRandR3(), glm::vec3(-1.0f));
	const ParticleRandomStep step(random, true);
	for (int i = 0; i < 100; ++i)
	{
		const auto p = random.ParticleRandR3();
		EXPECT_LE((p.z * p.z + p.y * p.y) + p.x * p.x, 1.0f);
	}
}

TEST(GameRandom, ARestartIsTheStateOfANewProgram)
{
	GameRandomProduction fresh;
	GameRandomProduction used;
	used.SetSeeds({.synced = 5, .local = 6});
	used.SetParticleStream(ParticleRandomStream::Local);
	static_cast<void>(used.GameRand(10));
	used.CrtSrand(77);
	static_cast<void>(used.CrtRand());
	used.Restart();
	EXPECT_EQ(used.GetSeeds(), fresh.GetSeeds());
	EXPECT_EQ(used.GetParticleStream(), fresh.GetParticleStream());
	EXPECT_EQ(used.GetCrtSeed(), fresh.GetCrtSeed());
	EXPECT_EQ(used.GetSyncedDraws(), 0u);
	EXPECT_EQ(used.CrtRand(), fresh.CrtRand());
	EXPECT_EQ(used.GameRand(1000), fresh.GameRand(1000));
}
