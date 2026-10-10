/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "RandomNumberManagerProduction.h"

using namespace openblack;

RandomNumberManagerProduction::RandomNumberManagerProduction()
{
	SetRunSeed(std::random_device {}());
}

void RandomNumberManagerProduction::SetRunSeed(uint32_t seed)
{
	_runSeed = seed;
	_seeding.fetch_add(1);
	for (size_t i = 0; i < _streams.size(); ++i)
	{
		_streams.at(i).seed(run_seed::StreamSeed(seed, static_cast<RandomStream>(i)));
	}
}

std::mt19937& RandomNumberManagerProduction::Stream(RandomStream stream)
{
	return _streams.at(static_cast<size_t>(stream));
}

std::mt19937& RandomNumberManagerProduction::Generator()
{
	// Each thread draws from its own, started from the run's seed again whenever that changes
	struct Seeded
	{
		std::mt19937 generator;
		std::optional<uint32_t> seeding;
	};
	thread_local Seeded tGenerator;
	if (tGenerator.seeding != _seeding)
	{
		tGenerator.generator.seed(run_seed::StreamSeed(_runSeed, RandomStream::General));
		tGenerator.seeding = _seeding;
	}
	return tGenerator.generator;
}

std::optional<std::reference_wrapper<std::mutex>> RandomNumberManagerProduction::LockAccess()
{
	return std::nullopt;
}
