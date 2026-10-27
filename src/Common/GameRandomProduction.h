/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "GameRandom.h"

#if defined(OPENBLACK_DEVELOPER_TOOLS)
#include "DrawTrace.h"
#endif

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack
{

/// The game's random state (its two seeds, the particles' stream and the game thread's C runtime seed). It has no lock, as
/// in the game: only the game thread draws
class GameRandomProduction final: public GameRandomInterface
{
public:
	uint32_t LocalRand(int32_t n) override;
	float LocalFloatRand(float x) override;
	int32_t CrtRand() override;
	void CrtSrand(uint32_t seed) override;

	[[nodiscard]] std::optional<uint32_t> GetCrtSeed() const override { return _crtSeed; }
	[[nodiscard]] uint64_t GetSyncedDraws() const override { return _syncedDraws; }
	void SetSyncedDraws(uint64_t draws) override;
	[[nodiscard]] GameRandomSeeds GetSeeds() const override { return _seeds; }
	void SetSeeds(GameRandomSeeds seeds) override { _seeds = seeds; }
	void Restart() override;

	[[nodiscard]] ParticleRandomStream GetParticleStream() const override { return _particleStream; }
	void SetParticleStream(ParticleRandomStream stream) override { _particleStream = stream; }

#if defined(OPENBLACK_DEVELOPER_TOOLS)
	void SetDrawTrace(std::shared_ptr<draw_trace::DrawTrace> trace) override { _trace = std::move(trace); }
	[[nodiscard]] draw_trace::DrawTrace* GetDrawTrace() const override { return _trace.get(); }
#endif

protected:
	uint32_t SyncedRand(uint32_t n, DrawSite site) override;
	float SyncedFloatRand(float x, DrawSite site) override;

private:
	/// Counts a draw on the synced seed, and gives it to the trace when there is one
	void CountSyncedDraw(const DrawSite& site);

	GameRandomSeeds _seeds;
	ParticleRandomStream _particleStream {ParticleRandomStream::None};
	uint32_t _crtSeed {1};
	uint64_t _syncedDraws {0};
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	std::shared_ptr<draw_trace::DrawTrace> _trace;
#endif
};

} // namespace openblack
