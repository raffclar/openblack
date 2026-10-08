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

#include <atomic>

#include "GameClock.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack
{
class TimeSystem final: public TimeSystemInterface
{
public:
	/// The state at program start (Reset)
	TimeSystem() noexcept;
	TimeSystem(const TimeSystem&) = delete;
	TimeSystem& operator=(const TimeSystem&) = delete;

	[[nodiscard]] uint32_t TickCount() const noexcept override;
	void SetTickSource(game_clock::TickSource source) noexcept override;
	void SetFixedFrameMs(uint32_t frameMs) noexcept override;
	[[nodiscard]] uint32_t FixedFrameMs() const noexcept override;
	void AdvanceFixedFrame() noexcept override;

	void Reset() noexcept override;
	void StartEngineTimer() noexcept override;
	[[nodiscard]] uint32_t MsPerTurn() const noexcept override;
	void SetMsPerTurn(uint32_t ms) noexcept override;
	[[nodiscard]] uint32_t Turn() const noexcept override;
	void SetTurn(uint32_t turn) noexcept override;
	[[nodiscard]] bool IsTurnScheduled() noexcept override;
	[[nodiscard]] uint32_t TurnsThisFrame() const noexcept override;
	void StartTurn() noexcept override;
	void ResetLocalTimer() noexcept override;
	void Start(bool paused) noexcept override;
	void OnLoad() noexcept override;
	void Pause(bool paused) noexcept override;
	[[nodiscard]] bool IsPaused() const noexcept override;
	[[nodiscard]] int32_t SequenceMode() const noexcept override;
	void SetSequenceMode(int32_t mode) noexcept override;
	void SetSpeed(float speed) noexcept override;
	[[nodiscard]] float Speed() const noexcept override;
	void UpdateFrameClock() noexcept override;
	void UpdateRealClock() noexcept override;
	[[nodiscard]] int32_t EngineMs() const noexcept override;
	[[nodiscard]] uint32_t FrameGameMs() const noexcept override;
	[[nodiscard]] float TurnFraction() const noexcept override;
	[[nodiscard]] uint32_t VisualMs() const noexcept override;
	[[nodiscard]] uint32_t FrameRealMs() const noexcept override;
	[[nodiscard]] uint32_t EngineFrameSampleMs() const noexcept override;

private:
	/// The game's fields and the game loop's own samples the clock is made of
	struct State
	{
		game_clock::Timer timer;       ///< The game timer
		game_clock::Timer engineTimer; ///< The engine's wall clock timer
		uint32_t msPerTurn {game_clock::k_MsPerTurn};
		uint32_t turn {0};
		bool paused {false};
		float speed {1.0f};
		uint32_t turnsThisFrame {0};
		int32_t previousLoopTimerSample {0};
		uint32_t previousLoopGameTurn {0};
		int32_t loopTimeRemainder {0};
		uint32_t visualMs {0};
		uint32_t frameGameMs {0};
		float fraction {0.0f};
		uint32_t previousEngineSample {0};
		uint32_t frameRealMs {1};
		int32_t sequenceMode {game_clock::k_SequenceModeNone};
	};

	State _state;
	/// Atomic: the music thread reads the ticks too
	std::atomic<game_clock::TickSource> _tickSource {&game_clock::WallTicks};
	/// The fixed step: its ticks (atomic for the music thread) and its ms a frame, 0 = off
	std::atomic<uint32_t> _fixedTicks {0};
	uint32_t _fixedFrameMs {0};
};
} // namespace openblack
