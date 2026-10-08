/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameClock.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>

#include <chrono>
#include <mutex>

#include <spdlog/spdlog.h>

#include "Locator.h"

namespace openblack::game_clock
{
namespace
{
TimeSystemInterface& Service() noexcept
{
	if (!Locator::time::has_value()) [[unlikely]]
	{
		// A clear stop instead of a null dereference: the game emplaces the clock first, the tests' listener its own
		std::fputs("game_clock: no TimeSystemInterface in the locator\n", stderr);
		std::abort();
	}
	return Locator::time::value();
}
} // namespace

int32_t Timer::ElapsedMs(uint32_t now) const
{
	// The tick difference is unsigned (it wraps), then float as the original's FPU
	const auto ticks = static_cast<float>(now - tickCount);
	return static_cast<int32_t>(ticks * speed + static_cast<float>(elapsedTime));
}

void Timer::Stop(uint32_t now)
{
	if (speed != 0.0f)
	{
		savedFactor = speed;
		elapsedTime = ElapsedMs(now);
		tickCount = now;
		speed = 0.0f;
	}
}

void Timer::SetSpeed(float factor, uint32_t now)
{
	if (speed != 0.0f)
	{
		elapsedTime = ElapsedMs(now);
		tickCount = now;
		speed = factor;
	}
	else
	{
		savedFactor = factor;
	}
}

void Timer::Start(uint32_t now)
{
	speed = k_StartSpeed;
	SetSpeed(savedFactor, now);
}

uint32_t WallTicks()
{
	using namespace std::chrono;
	return static_cast<uint32_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

uint32_t TickCount()
{
	if (!Locator::time::has_value()) [[unlikely]]
	{
		// The music thread must stop before the clock goes (ShutDownServices closes the audio device first). The wall
		// clock is only a last resort while shutting down; it is logged once so it is noticed if it happens in a game
		assert(false && "game_clock: ticks read without a clock");
		static std::once_flag s_warned;
		std::call_once(s_warned, [] {
			if (auto logger = spdlog::get("game"); logger != nullptr)
			{
				logger->warn("game_clock: ticks read without a clock in the locator, using the wall clock");
			}
		});
		return WallTicks();
	}
	return Locator::time::value().TickCount();
}

void SetTickSource(TickSource source)
{
	Service().SetTickSource(source);
}

void Reset()
{
	Service().Reset();
}

void StartEngineTimer()
{
	Service().StartEngineTimer();
}

uint32_t MsPerTurn()
{
	return Service().MsPerTurn();
}

void SetMsPerTurn(uint32_t ms)
{
	Service().SetMsPerTurn(ms);
}

int32_t TicksForSeconds(float seconds)
{
	// Unsigned integer division, then float. (inferred) the original does not check for 0 ms per turn
	// (SET_GAME_TICK_TIME 0 would fault on the division); openblack gives 0 turns instead of crashing
	const uint32_t msPerTurn = MsPerTurn();
	const uint32_t perSecond = msPerTurn != 0 ? 1000u / msPerTurn : 0u;
	return static_cast<int32_t>(static_cast<float>(perSecond) * seconds);
}

uint32_t Turn()
{
	return Service().Turn();
}

void SetTurn(uint32_t turn)
{
	Service().SetTurn(turn);
}

bool IsTurnScheduled()
{
	return Service().IsTurnScheduled();
}

bool TurnDue()
{
	// The timer first, then the turns of this frame
	return IsTurnScheduled() && Service().TurnsThisFrame() < k_MaxTurnsPerFrame;
}

void StartTurn()
{
	Service().StartTurn();
}

void ResetLocalTimer()
{
	Service().ResetLocalTimer();
}

void Start(bool paused)
{
	Service().Start(paused);
}

void OnLoad()
{
	Service().OnLoad();
}

void Pause(bool paused)
{
	Service().Pause(paused);
}

bool IsPaused()
{
	return Service().IsPaused();
}

int32_t SequenceMode()
{
	return Service().SequenceMode();
}

void SetSequenceMode(int32_t mode)
{
	Service().SetSequenceMode(mode);
}

bool IsInsideCitadel()
{
	return SequenceMode() == k_SequenceModeCitadel;
}

void SetSpeed(float speed)
{
	Service().SetSpeed(speed);
}

float Speed()
{
	return Service().Speed();
}

void UpdateFrameClock()
{
	Service().UpdateFrameClock();
}

void UpdateRealClock()
{
	Service().UpdateRealClock();
}

int32_t EngineMs()
{
	return Service().EngineMs();
}

uint32_t FrameGameMs()
{
	return Service().FrameGameMs();
}

float FrameGameSeconds()
{
	return static_cast<float>(FrameGameMs()) * k_SecondsPerMs;
}

float TurnFraction()
{
	return Service().TurnFraction();
}

uint32_t VisualMs()
{
	return Service().VisualMs();
}

uint32_t FrameRealMs()
{
	return Service().FrameRealMs();
}

uint32_t EngineFrameSampleMs()
{
	return Service().EngineFrameSampleMs();
}

uint32_t CameraFrameMs(bool playingBack)
{
	return playingBack ? FrameGameMs() : FrameRealMs();
}

uint32_t ClampedFrameMs(bool inTemple)
{
	// A signed test: <= 0 gives 0, then at most 500
	const auto ms = static_cast<int32_t>(inTemple ? FrameRealMs() : FrameGameMs());
	if (ms <= 0)
	{
		return 0;
	}
	return ms >= static_cast<int32_t>(k_ClampedFrameMaxMs) ? k_ClampedFrameMaxMs : static_cast<uint32_t>(ms);
}

bool PausedTurnTimer::Due(uint32_t now)
{
	if (!last.has_value())
	{
		last = now;
	}
	if (now - *last <= k_PausedTurnMs)
	{
		return false;
	}
	*last += k_PausedTurnMs;
	if (now - *last > k_PausedTurnMaxLagMs)
	{
		last = now;
	}
	return true;
}

} // namespace openblack::game_clock
