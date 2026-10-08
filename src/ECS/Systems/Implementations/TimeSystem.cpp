/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ECS/Systems/Implementations/TimeSystem.h"

using namespace openblack;
using namespace openblack::game_clock;

TimeSystem::TimeSystem() noexcept
{
	Reset();
}

uint32_t TimeSystem::TickCount() const noexcept
{
	if (_fixedFrameMs != 0)
	{
		return _fixedTicks.load(std::memory_order_relaxed);
	}
	return _tickSource.load(std::memory_order_relaxed)();
}

void TimeSystem::SetTickSource(TickSource source) noexcept
{
	_tickSource.store(source != nullptr ? source : &WallTicks, std::memory_order_relaxed);
}

void TimeSystem::SetFixedFrameMs(uint32_t frameMs) noexcept
{
	_fixedFrameMs = frameMs;
	_fixedTicks.store(0, std::memory_order_relaxed);
}

uint32_t TimeSystem::FixedFrameMs() const noexcept
{
	return _fixedFrameMs;
}

void TimeSystem::AdvanceFixedFrame() noexcept
{
	_fixedTicks.fetch_add(_fixedFrameMs, std::memory_order_relaxed);
}

void TimeSystem::Reset() noexcept
{
	_state = {};
	// The engine timer starts reset and stopped, with its saved speed at 1
	auto& engine = _state.engineTimer;
	const uint32_t now = TickCount();
	engine = {now, 0, 1.0f, 0.0f};
	engine.Stop(now);
}

void TimeSystem::StartEngineTimer() noexcept
{
	auto& engine = _state.engineTimer;
	// Speed 1e-5, elapsed = ElapsedMs, base = now, then the saved speed (read before)
	const float saved = engine.savedFactor;
	engine.speed = k_StartSpeed;
	const uint32_t now = TickCount();
	engine.elapsedTime = engine.ElapsedMs(now);
	engine.tickCount = now;
	engine.speed = saved;
}

uint32_t TimeSystem::MsPerTurn() const noexcept
{
	return _state.msPerTurn;
}

void TimeSystem::SetMsPerTurn(uint32_t ms) noexcept
{
	_state.msPerTurn = ms;
}

uint32_t TimeSystem::Turn() const noexcept
{
	return _state.turn;
}

void TimeSystem::SetTurn(uint32_t turn) noexcept
{
	_state.turn = turn;
}

bool TimeSystem::IsTurnScheduled() noexcept
{
	auto& s = _state;
	// The sample comes first, then the target turn * 100
	const int32_t local = s.timer.ElapsedMs(TickCount());
	const auto target = static_cast<int32_t>(s.turn * k_SchedulerMsPerTurn);
	if (s.paused)
	{
		return false;
	}
	// More than 2 s behind: the timer goes back to the turn
	if (local - target > k_MaxLagMs)
	{
		ResetLocalTimer();
	}
	// The sample taken before the reset
	return local >= target;
}

uint32_t TimeSystem::TurnsThisFrame() const noexcept
{
	return _state.turnsThisFrame;
}

void TimeSystem::StartTurn() noexcept
{
	auto& s = _state;
	++s.turnsThisFrame;
	// The turn number does not go up while paused
	if (!s.paused)
	{
		++s.turn;
	}
}

void TimeSystem::ResetLocalTimer() noexcept
{
	auto& timer = _state.timer;
	const uint32_t now = TickCount();
	timer.Stop(now);
	// The elapsed time = turn * 100, from now
	timer.tickCount = now;
	timer.elapsedTime = static_cast<int32_t>(_state.turn * k_SchedulerMsPerTurn);
	timer.Stop(now);
	timer.Start(now);
}

void TimeSystem::Start(bool paused) noexcept
{
	auto& s = _state;
	const uint32_t now = TickCount();
	s.timer.tickCount = now;
	s.timer.elapsedTime = 0;
	s.timer.Stop(now);
	s.timer.Start(now);
	s.frameGameMs = 0;
	s.fraction = 0.0f;
	s.paused = paused;
	s.turnsThisFrame = 0;
	// Before the single player loop
	ResetLocalTimer();
}

void TimeSystem::OnLoad() noexcept
{
	auto& s = _state;
	s.frameGameMs = 0;
	s.visualMs = s.turn * k_SchedulerMsPerTurn;
	s.fraction = 0.0f;
}

void TimeSystem::Pause(bool paused) noexcept
{
	auto& s = _state;
	// Only when the flag changes
	if (paused == s.paused)
	{
		return;
	}
	s.paused = paused;
	const uint32_t now = TickCount();
	if (paused)
	{
		s.timer.Stop(now);
	}
	else
	{
		s.timer.Start(now);
	}
}

bool TimeSystem::IsPaused() const noexcept
{
	return _state.paused;
}

int32_t TimeSystem::SequenceMode() const noexcept
{
	return _state.sequenceMode;
}

void TimeSystem::SetSequenceMode(int32_t mode) noexcept
{
	_state.sequenceMode = mode;
}

void TimeSystem::SetSpeed(float speed) noexcept
{
	_state.speed = speed;
	_state.timer.SetSpeed(speed, TickCount());
	// The original also clears a field here that nothing reads (not ported)
}

float TimeSystem::Speed() const noexcept
{
	return _state.speed;
}

void TimeSystem::UpdateFrameClock() noexcept
{
	auto& s = _state;
	if (!s.paused)
	{
		const int32_t sample = s.timer.ElapsedMs(TickCount());
		const int32_t delta = sample - s.previousLoopTimerSample;
		s.previousLoopTimerSample = sample;
		const uint32_t turn = s.turn;
		if (s.previousLoopGameTurn == turn)
		{
			s.loopTimeRemainder += delta;
		}
		else
		{
			// A new turn: its 100 ms come off the remainder
			s.loopTimeRemainder += delta - static_cast<int32_t>(k_SchedulerMsPerTurn);
			s.previousLoopGameTurn = turn;
		}
		if (s.loopTimeRemainder > 0)
		{
			if (s.loopTimeRemainder >= k_MaxRemainderMs)
			{
				s.loopTimeRemainder = k_MaxRemainderMs;
			}
		}
		else
		{
			s.loopTimeRemainder = 0;
		}
		// turn * 100 + remainder, compared unsigned: a smaller value is stored as it is (the clock goes back) with the
		// remainder at 0, and the frame's dt is 0
		const uint32_t visual = turn * k_SchedulerMsPerTurn + static_cast<uint32_t>(s.loopTimeRemainder);
		if (visual < s.visualMs)
		{
			s.visualMs = visual;
			s.loopTimeRemainder = 0;
		}
		s.frameGameMs = visual - s.visualMs;
		s.visualMs = visual;
		s.fraction = static_cast<float>(s.loopTimeRemainder) * k_FractionPerMs;
	}
	else
	{
		// Paused: no game ms, the fraction is not touched
		s.frameGameMs = 0;
	}
	s.turnsThisFrame = 0;
}

void TimeSystem::UpdateRealClock() noexcept
{
	auto& s = _state;
	const auto now = static_cast<uint32_t>(s.engineTimer.ElapsedMs(TickCount()));
	// Signed: <= 0 gives 1
	const auto delta = static_cast<int32_t>(now - s.previousEngineSample);
	s.frameRealMs = delta > 0 ? static_cast<uint32_t>(delta) : 1u;
	s.previousEngineSample = now;
}

int32_t TimeSystem::EngineMs() const noexcept
{
	return _state.engineTimer.ElapsedMs(TickCount());
}

uint32_t TimeSystem::FrameGameMs() const noexcept
{
	return _state.frameGameMs;
}

float TimeSystem::TurnFraction() const noexcept
{
	return _state.fraction;
}

uint32_t TimeSystem::VisualMs() const noexcept
{
	return _state.visualMs;
}

uint32_t TimeSystem::FrameRealMs() const noexcept
{
	return _state.frameRealMs;
}

uint32_t TimeSystem::EngineFrameSampleMs() const noexcept
{
	// UpdateRealClock keeps this frame's sample
	return _state.previousEngineSample;
}
