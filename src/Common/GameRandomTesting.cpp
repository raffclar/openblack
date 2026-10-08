/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "GameRandomTesting.h"

#include <utility>

#include "Locator.h"

using namespace openblack;
using namespace openblack::game_random;

GameRandomTesting::GameRandomTesting(GameRandomSeeds seeds, uint32_t crtSeed) noexcept
    : _real(seeds, crtSeed)
{
}

void GameRandomTesting::SetGameRand(RandHook rand, FloatRandHook floatRand)
{
	_rand = std::move(rand);
	_floatRand = std::move(floatRand);
}

void GameRandomTesting::Init() noexcept
{
	_real.Init();
}

void GameRandomTesting::Reset() noexcept
{
	_real.Reset();
}

void GameRandomTesting::Load(const GameRandomSeeds& seeds) noexcept
{
	_real.Load(seeds);
}

const GameRandomSeeds& GameRandomTesting::Current() const noexcept
{
	return _real.Current();
}

uint32_t GameRandomTesting::GameRand(uint32_t n, std::source_location where) noexcept
{
	if (_rand)
	{
		return _rand(n);
	}
	return _real.GameRand(n, where);
}

float GameRandomTesting::GameFloatRand(float x, std::source_location where) noexcept
{
	if (_floatRand)
	{
		return _floatRand(x);
	}
	return _real.GameFloatRand(x, where);
}

uint32_t GameRandomTesting::LocalRand(int32_t n) noexcept
{
	return _real.LocalRand(n);
}

float GameRandomTesting::LocalFloatRand(float x) noexcept
{
	return _real.LocalFloatRand(x);
}

ParticleRandomStream GameRandomTesting::GetParticleStream() const noexcept
{
	return _real.GetParticleStream();
}

void GameRandomTesting::SetParticleStream(ParticleRandomStream stream) noexcept
{
	_real.SetParticleStream(stream);
}

void GameRandomTesting::CountOutsideStep(const std::source_location& where) noexcept
{
	_real.CountOutsideStep(where);
}

uint32_t GameRandomTesting::OutsideStepDraws() const noexcept
{
	return _real.OutsideStepDraws();
}

int32_t GameRandomTesting::CrtRand() noexcept
{
	return _real.CrtRand();
}

void GameRandomTesting::CrtSrand(uint32_t seed) noexcept
{
	_real.CrtSrand(seed);
}

uint32_t GameRandomTesting::CrtSeed() const noexcept
{
	return _real.CrtSeed();
}

void testing::SetGameRand(GameRandomTesting::RandHook rand, GameRandomTesting::FloatRandHook floatRand)
{
	auto* service =
	    Locator::gameRandom::has_value() ? dynamic_cast<GameRandomTesting*>(&Locator::gameRandom::value()) : nullptr;
	if (service == nullptr)
	{
		service = &static_cast<GameRandomTesting&>(Locator::gameRandom::emplace<GameRandomTesting>());
	}
	service->SetGameRand(std::move(rand), std::move(floatRand));
}

uint32_t testing::OutsideStepDraws() noexcept
{
	return Locator::gameRandom::value().OutsideStepDraws();
}

testing::ScopedState::ScopedState(GameRandomSeeds seeds, uint32_t crtSeed)
    : _previous(Locator::gameRandom::handle())
{
	Locator::gameRandom::emplace<GameRandomTesting>(seeds, crtSeed);
}

testing::ScopedState::~ScopedState()
{
	Locator::gameRandom::reset(_previous);
}
