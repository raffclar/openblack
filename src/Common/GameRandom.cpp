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
#include <cstdio>
#include <cstdlib>

#include <bit>

#include <spdlog/spdlog.h>

#include "Locator.h"

namespace openblack::game_random
{
namespace
{
GameRandomInterface& Service() noexcept
{
	if (!Locator::gameRandom::has_value()) [[unlikely]]
	{
		// A clear stop instead of a null dereference: the engine emplaces the service at start-up, a test its fake
		std::fputs("game_random: no GameRandomInterface in the locator (tests: use game_random::testing::ScopedState)\n",
		           stderr);
		std::abort();
	}
	return Locator::gameRandom::value();
}

/// (u x scale) x x, the particle systems' order
float ParticleOrderFloat(uint32_t u, float x) noexcept
{
	float r = static_cast<float>(u);
	r = r * FloatRandScale();
	r = r * x;
	return r;
}
} // namespace

float FloatRandScale() noexcept
{
	return std::bit_cast<float>(k_FloatRandScaleBits);
}

float CrtRandomScale() noexcept
{
	return std::bit_cast<float>(k_CrtRandomScaleBits);
}

uint32_t SeededRandom(uint32_t n, uint32_t& seed) noexcept
{
	uint32_t s = seed * 9377u + 0x24DFu;
	s = std::rotr(s, 13);
	seed = s;
	return s % n;
}

float FloatRand(float x, uint32_t& seed) noexcept
{
	// +-0 and NaN both give 0 without a draw
	if (x == 0.0f || std::isnan(x))
	{
		return 0.0f;
	}
	const uint32_t u = SeededRandom(k_FloatRandRange, seed);
	float r = static_cast<float>(u);
	r = r * x;
	r = r * FloatRandScale();
	return r;
}

void Init() noexcept
{
	Service().Init();
}

void Reset() noexcept
{
	Service().Reset();
}

GameRandomSeeds Save() noexcept
{
	return Service().Current();
}

void Load(const GameRandomSeeds& seeds) noexcept
{
	Service().Load(seeds);
}

const GameRandomSeeds& Current() noexcept
{
	return Service().Current();
}

uint32_t GameRand(uint32_t n, std::source_location where) noexcept
{
	return Service().GameRand(n, where);
}

float GameFloatRand(float x, std::source_location where) noexcept
{
	return Service().GameFloatRand(x, where);
}

float GameFloatRange(float a, float b, std::source_location where) noexcept
{
	const float d = b - a;
	const float r = GameFloatRand(d, where);
	return r + a;
}

uint32_t LocalRand(int32_t n) noexcept
{
	return Service().LocalRand(n);
}

float LocalFloatRand(float x) noexcept
{
	return Service().LocalFloatRand(x);
}

// ---- psys ----------------------------------------------------------------------------------------------------------

psys::StepScope::StepScope(NetGameType type) noexcept
{
	Service().SetParticleStream(type == NetGameType::Synced ? ParticleRandomStream::Synced : ParticleRandomStream::Local);
}

psys::StepScope::~StepScope()
{
	Service().SetParticleStream(ParticleRandomStream::None);
}

ParticleRandomStream psys::Active() noexcept
{
	return Service().GetParticleStream();
}

float psys::FloatRand(float x, std::source_location where) noexcept
{
	switch (Active())
	{
	case ParticleRandomStream::Synced:
		return ParticleOrderFloat(GameRand(k_FloatRandRange, where), x);
	case ParticleRandomStream::Local:
		return ParticleOrderFloat(LocalRand(static_cast<int32_t>(k_FloatRandRange)), x);
	case ParticleRandomStream::None:
		break;
	}
	Service().CountOutsideStep(where);
	return 0.0f;
}

float psys::FloatRand(float a, float b, std::source_location where) noexcept
{
	const float d = b - a;
	const float r = FloatRand(d, where);
	return r + a;
}

int32_t psys::Rand(int32_t n, std::source_location where) noexcept
{
	switch (Active())
	{
	case ParticleRandomStream::Synced:
		return static_cast<int32_t>(GameRand(static_cast<uint32_t>(n), where));
	case ParticleRandomStream::Local:
		return static_cast<int32_t>(LocalRand(n));
	case ParticleRandomStream::None:
		break;
	}
	Service().CountOutsideStep(where);
	return 0;
}

glm::vec3 psys::RandR3(std::source_location where) noexcept
{
	if (Active() == ParticleRandomStream::None)
	{
		// (approximate) the original would loop forever on (-1, -1, -1); it never calls this outside a step
		Service().CountOutsideStep(where);
		static bool s_warned = false;
		if (!s_warned)
		{
			s_warned = true;
			if (auto logger = spdlog::get("game"); logger != nullptr)
			{
				logger->warn("game_random: psys::RandR3 outside an effect's step ({}:{}), (-1, -1, -1)", where.file_name(),
				             where.line());
			}
		}
		return {-1.0f, -1.0f, -1.0f};
	}
	glm::vec3 p;
	float sum = 0.0f;
	do
	{
		p.x = FloatRand(2.0f, where) - 1.0f;
		p.y = FloatRand(2.0f, where) - 1.0f;
		p.z = FloatRand(2.0f, where) - 1.0f;
		const float zz = p.z * p.z;
		const float yy = p.y * p.y;
		const float xx = p.x * p.x;
		sum = zz + yy;
		sum = sum + xx;
	} while (sum > 1.0f);
	return p;
}

// ---- crt -----------------------------------------------------------------------------------------------------------

int32_t crt::Rand() noexcept
{
	return Service().CrtRand();
}

void crt::Srand(uint32_t seed) noexcept
{
	Service().CrtSrand(seed);
}

uint32_t crt::Seed() noexcept
{
	return Service().CrtSeed();
}

float crt::Random(float a, float b) noexcept
{
	float r = static_cast<float>(Rand());
	r = r * CrtRandomScale();
	const float d = b - a;
	r = r * d;
	return r + a;
}
} // namespace openblack::game_random
