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

#include <functional>

#include <entt/locator/locator.hpp>

#include "GameRandom.h"
#include "GameRandomProduction.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack
{
/// The game's streams for tests: the real arithmetic, plus hooks that replace GameRand / GameFloatRand
class GameRandomTesting final: public GameRandomInterface
{
public:
	using RandHook = std::function<uint32_t(uint32_t)>;
	using FloatRandHook = std::function<float(float)>;

	explicit GameRandomTesting(GameRandomSeeds seeds = {}, uint32_t crtSeed = game_random::k_CrtInitialSeed) noexcept;
	GameRandomTesting(const GameRandomTesting&) = delete;
	GameRandomTesting& operator=(const GameRandomTesting&) = delete;

	/// When set, GameRand / GameFloatRand return them, called before the 0-for-0 test, the seed untouched. Particle
	/// draws on the synced stream go through GameRand too
	void SetGameRand(RandHook rand, FloatRandHook floatRand);

	void Init() noexcept override;
	void Reset() noexcept override;
	void Load(const GameRandomSeeds& seeds) noexcept override;
	[[nodiscard]] const GameRandomSeeds& Current() const noexcept override;

	uint32_t GameRand(uint32_t n, std::source_location where) noexcept override;
	float GameFloatRand(float x, std::source_location where) noexcept override;
	uint32_t LocalRand(int32_t n) noexcept override;
	float LocalFloatRand(float x) noexcept override;

	[[nodiscard]] ParticleRandomStream GetParticleStream() const noexcept override;
	void SetParticleStream(ParticleRandomStream stream) noexcept override;
	void CountOutsideStep(const std::source_location& where) noexcept override;
	[[nodiscard]] uint32_t OutsideStepDraws() const noexcept override;

	int32_t CrtRand() noexcept override;
	void CrtSrand(uint32_t seed) noexcept override;
	[[nodiscard]] uint32_t CrtSeed() const noexcept override;

private:
	GameRandomProduction _real;
	RandHook _rand;
	FloatRandHook _floatRand;
};
} // namespace openblack

namespace openblack::game_random::testing
{
/// Sets the hooks on the service, first putting a GameRandomTesting in the locator if it holds another one (or none)
void SetGameRand(GameRandomTesting::RandHook rand, GameRandomTesting::FloatRandHook floatRand);
/// How many particle draws were made outside an effect's step
[[nodiscard]] uint32_t OutsideStepDraws() noexcept;

/// A GameRandomTesting with these seeds in the locator for one test; the previous service comes back on destruction
class ScopedState
{
public:
	explicit ScopedState(GameRandomSeeds seeds = {k_InitialSeed, k_InitialSeed}, uint32_t crtSeed = k_CrtInitialSeed);
	~ScopedState();
	ScopedState(const ScopedState&) = delete;
	ScopedState& operator=(const ScopedState&) = delete;
	ScopedState(ScopedState&&) = delete;
	ScopedState& operator=(ScopedState&&) = delete;

private:
	entt::locator<GameRandomInterface>::node_type _previous;
};
} // namespace openblack::game_random::testing
