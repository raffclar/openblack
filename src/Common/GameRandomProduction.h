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

#include <map>
#include <string>
#include <thread>

#include "GameRandom.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack
{
class GameRandomProduction final: public GameRandomInterface
{
public:
	explicit GameRandomProduction(GameRandomSeeds seeds = {}, uint32_t crtSeed = game_random::k_CrtInitialSeed) noexcept;
	GameRandomProduction(const GameRandomProduction&) = delete;
	GameRandomProduction& operator=(const GameRandomProduction&) = delete;
	~GameRandomProduction() override;

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
	/// Debug builds check nobody draws from another thread than Init / Reset's (the original has no lock either)
	void CheckThread() const noexcept;
	void TakeThread() noexcept;
	/// OPENBLACK_TRACE_GAME_RAND=1 logs every synced draw: the seed before it, the game turn and the caller
	void Trace(const char* what, uint32_t seed, const std::source_location& where) const;

	GameRandomSeeds _seeds;
	ParticleRandomStream _stream {ParticleRandomStream::None};
	uint32_t _crtSeed;
	uint32_t _outsideStepDraws {0};
	bool _traceOn;
	/// OPENBLACK_TEST_PSYS_RAND_OUTSIDE=1: each site drawing outside a step, logged the first time, totals at exit
	bool _outsideReportOn;
	std::map<std::string, uint32_t> _outsideSites;
#ifndef NDEBUG
	std::thread::id _owner;
#endif
};
} // namespace openblack
