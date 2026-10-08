/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "GameRandomProduction.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <spdlog/spdlog.h>

#include "GameClock.h"

using namespace openblack;
using namespace openblack::game_random;

namespace
{
bool EnvOn(const char* name) noexcept
{
	const char* value = std::getenv(name);
	return value != nullptr && value[0] == '1';
}

/// The original tests C3 after comparing with 0, which is set for +-0 and for NaN (unordered)
bool ZeroOrNaN(float x) noexcept
{
	return x == 0.0f || std::isnan(x);
}
} // namespace

GameRandomProduction::GameRandomProduction(GameRandomSeeds seeds, uint32_t crtSeed) noexcept
    : _seeds(seeds)
    , _crtSeed(crtSeed)
    , _traceOn(EnvOn("OPENBLACK_TRACE_GAME_RAND"))
    , _outsideReportOn(EnvOn("OPENBLACK_TEST_PSYS_RAND_OUTSIDE"))
{
}

GameRandomProduction::~GameRandomProduction()
{
	for (const auto& [site, count] : _outsideSites)
	{
		std::fprintf(stderr, "game_random: %u psys draws outside a step at %s\n", count, site.c_str());
	}
}

void GameRandomProduction::CheckThread() const noexcept
{
#ifndef NDEBUG
	if (_owner != std::thread::id {})
	{
		assert(_owner == std::this_thread::get_id() && "game_random: drawn from another thread than Init / Reset's");
	}
#endif
}

void GameRandomProduction::TakeThread() noexcept
{
#ifndef NDEBUG
	_owner = std::this_thread::get_id();
#endif
}

void GameRandomProduction::Trace(const char* what, uint32_t seed, const std::source_location& where) const
{
	if (!_traceOn)
	{
		return;
	}
	if (auto logger = spdlog::get("game"); logger != nullptr)
	{
		// The original's trace format
		logger->trace("{}(). Seed {}, GameTurn {}, ({}:{})", what, static_cast<int32_t>(seed), game_clock::Turn(),
		              where.file_name(), where.line());
	}
}

void GameRandomProduction::Init() noexcept
{
	TakeThread();
	_seeds = {k_InitialSeed, k_InitialSeed};
}

void GameRandomProduction::Reset() noexcept
{
	TakeThread();
	_seeds = {0, 0};
}

void GameRandomProduction::Load(const GameRandomSeeds& seeds) noexcept
{
	_seeds = seeds;
}

const GameRandomSeeds& GameRandomProduction::Current() const noexcept
{
	return _seeds;
}

uint32_t GameRandomProduction::GameRand(uint32_t n, std::source_location where) noexcept
{
	CheckThread();
	Trace("Rand", _seeds.synced, where);
	if (n == 0)
	{
		return 0;
	}
	return SeededRandom(n, _seeds.synced);
}

float GameRandomProduction::GameFloatRand(float x, std::source_location where) noexcept
{
	CheckThread();
	// Tested before the trace, the draw tests it again
	if (ZeroOrNaN(x))
	{
		return 0.0f;
	}
	Trace("FloatRand", _seeds.synced, where);
	return FloatRand(x, _seeds.synced);
}

uint32_t GameRandomProduction::LocalRand(int32_t n) noexcept
{
	CheckThread();
	if (n == 0)
	{
		return 0;
	}
	return SeededRandom(static_cast<uint32_t>(n), _seeds.local);
}

float GameRandomProduction::LocalFloatRand(float x) noexcept
{
	CheckThread();
	return FloatRand(x, _seeds.local);
}

ParticleRandomStream GameRandomProduction::GetParticleStream() const noexcept
{
	return _stream;
}

void GameRandomProduction::SetParticleStream(ParticleRandomStream stream) noexcept
{
	_stream = stream;
}

void GameRandomProduction::CountOutsideStep(const std::source_location& where) noexcept
{
	++_outsideStepDraws;
	if (!_outsideReportOn)
	{
		return;
	}
	const auto site = std::string(where.file_name()) + ":" + std::to_string(where.line());
	if (++_outsideSites[site] == 1)
	{
		if (auto logger = spdlog::get("game"); logger != nullptr)
		{
			logger->warn("game_random: psys draw outside an effect's step at {}", site);
		}
	}
}

uint32_t GameRandomProduction::OutsideStepDraws() const noexcept
{
	return _outsideStepDraws;
}

int32_t GameRandomProduction::CrtRand() noexcept
{
	CheckThread();
	_crtSeed = _crtSeed * 0x343FDu + 0x269EC3u;
	return static_cast<int32_t>((_crtSeed >> 16u) & 0x7FFFu);
}

void GameRandomProduction::CrtSrand(uint32_t seed) noexcept
{
	_crtSeed = seed;
}

uint32_t GameRandomProduction::CrtSeed() const noexcept
{
	return _crtSeed;
}
