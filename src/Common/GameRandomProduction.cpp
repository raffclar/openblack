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

#include <cmath>

using namespace openblack;

void GameRandomProduction::CountSyncedDraw([[maybe_unused]] const DrawSite& site)
{
	++_syncedDraws;
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	if (_trace != nullptr)
	{
		_trace->Draw(_syncedDraws, site);
	}
#endif
}

uint32_t GameRandomProduction::SyncedRand(uint32_t n, DrawSite site)
{
	if (n == 0)
	{
		return 0;
	}
	CountSyncedDraw(site);
	return game_random::LHRand(n, _seeds.synced);
}

float GameRandomProduction::SyncedFloatRand(float x, DrawSite site)
{
	// A float of 0 or not a number gives 0 without a draw
	if (x != 0.0f && !std::isnan(x))
	{
		CountSyncedDraw(site);
	}
	return game_random::FloatRand(x, _seeds.synced);
}

void GameRandomProduction::SetSyncedDraws(uint64_t draws)
{
	_syncedDraws = draws;
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	if (_trace != nullptr)
	{
		_trace->SetDraws(draws);
	}
#endif
}

void GameRandomProduction::Restart()
{
	_seeds = {};
	_particleStream = ParticleRandomStream::None;
	_crtSeed = 1;
	_syncedDraws = 0;
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	if (_trace != nullptr)
	{
		_trace->Restart();
	}
#endif
}

uint32_t GameRandomProduction::LocalRand(int32_t n)
{
	return n == 0 ? 0 : game_random::LHRand(static_cast<uint32_t>(n), _seeds.local);
}

float GameRandomProduction::LocalFloatRand(float x)
{
	return game_random::FloatRand(x, _seeds.local);
}

int32_t GameRandomProduction::CrtRand()
{
	return game_random::CrtRand(_crtSeed);
}

void GameRandomProduction::CrtSrand(uint32_t seed)
{
	_crtSeed = seed;
}
