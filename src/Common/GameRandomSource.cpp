/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameRandom.h"
#include "Locator.h"

using namespace openblack;

uint32_t GameRandomSource::GameRand(uint32_t n)
{
	if (Locator::gameRandom::has_value())
	{
		return Locator::gameRandom::value().GameRand(n);
	}
	return n == 0 ? 0 : game_random::LHRand(n, _own.synced);
}

float GameRandomSource::GameFloatRand(float x)
{
	if (Locator::gameRandom::has_value())
	{
		return Locator::gameRandom::value().GameFloatRand(x);
	}
	return game_random::FloatRand(x, _own.synced);
}

uint32_t GameRandomSource::LocalRand(int32_t n)
{
	if (Locator::gameRandom::has_value())
	{
		return Locator::gameRandom::value().LocalRand(n);
	}
	return n == 0 ? 0 : game_random::LHRand(static_cast<uint32_t>(n), _own.local);
}

float GameRandomSource::LocalFloatRand(float x)
{
	if (Locator::gameRandom::has_value())
	{
		return Locator::gameRandom::value().LocalFloatRand(x);
	}
	return game_random::FloatRand(x, _own.local);
}
