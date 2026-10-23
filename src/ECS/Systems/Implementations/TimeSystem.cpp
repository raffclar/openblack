/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "TimeSystem.h"

#include <algorithm>
#include <utility>

#include "3D/MapCoords.h"

using namespace openblack::ecs::systems;

namespace
{
constexpr auto k_TurnMs = static_cast<uint32_t>(TimeSystemInterface::k_TurnDuration.count());
/// Further behind than this, a game with one player gives up on the time it lost
constexpr int32_t k_MaxLagMs = 2000;
/// The turns that a frame can do: one alone, and up to 10 in a network game, so that a computer that is behind catches up
constexpr uint32_t k_MaxTurnsPerFrame = 1;
constexpr uint32_t k_MaxNetworkTurnsPerFrame = 10;
constexpr int32_t k_MaxRemainderMs = 99;
constexpr float k_FractionPerMs = 0.01f;
/// The timer is started at this speed so that taking its saved speed counts from now
constexpr float k_StartSpeed = 0.00001f;

uint32_t WallTicks()
{
	using namespace std::chrono;
	return static_cast<uint32_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}
} // namespace

int32_t GameTimer::Milliseconds(uint32_t now) const
{
	// The ticks since the base, scaled, plus what was counted by then, as one float sum truncated
	const auto ticks = static_cast<float>(now - base);
	return openblack::map_coords::FtoL(ticks * speed + static_cast<float>(elapsed));
}

void GameTimer::Stop(uint32_t now)
{
	if (speed != 0.0f)
	{
		savedSpeed = speed;
		elapsed = Milliseconds(now);
		base = now;
		speed = 0.0f;
	}
}

void GameTimer::SetSpeed(float factor, uint32_t now)
{
	if (speed != 0.0f)
	{
		elapsed = Milliseconds(now);
		base = now;
		speed = factor;
	}
	else
	{
		savedSpeed = factor;
	}
}

void GameTimer::Start(uint32_t now)
{
	speed = k_StartSpeed;
	SetSpeed(savedSpeed, now);
}

TimeSystem::TimeSystem()
    : TimeSystem(&WallTicks)
{
}

TimeSystem::TimeSystem(TickSource ticks)
    : _ticks(std::move(ticks))
{
}

void TimeSystem::Start()
{
	_elapsedTime = std::chrono::milliseconds(0);
	_lastFrameTicks = Ticks();
	_lastDrawTicks = _lastFrameTicks;
	_ticksEpoch = _lastFrameTicks;
}

int64_t TimeSystem::GetUnixTime() const
{
	if (_pinnedDate.has_value())
	{
		return *_pinnedDate + static_cast<int64_t>(GetTicks() / 1000u);
	}
	return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

void TimeSystem::RestartClock(std::optional<int64_t> unixTime)
{
	_ticksEpoch = Ticks();
	_pinnedDate = unixTime;
}

uint32_t TimeSystem::Ticks() const
{
	return _fixedFrameTime.has_value() ? _fixedTicks : _ticks() + _tickOffset;
}

void TimeSystem::SetFixedFrameTime(std::optional<std::chrono::microseconds> frameTime)
{
	if (frameTime.has_value() && !_fixedFrameTime.has_value())
	{
		_fixedTicks = Ticks();
		_fixedRemainderUs = 0;
	}
	else if (!frameTime.has_value() && _fixedFrameTime.has_value())
	{
		// The tick source's count carries on from the fixed one, the wraps of both alike
		_tickOffset = _fixedTicks - _ticks();
	}
	_fixedFrameTime = frameTime;
}

void TimeSystem::Update()
{
	if (_fixedFrameTime.has_value())
	{
		// Each whole millisecond goes on the tick count; the rest waits for the next frames
		constexpr uint32_t k_MicrosecondsPerMs = 1000;
		_fixedRemainderUs += static_cast<uint32_t>(_fixedFrameTime->count());
		_fixedTicks += _fixedRemainderUs / k_MicrosecondsPerMs;
		_fixedRemainderUs %= k_MicrosecondsPerMs;
	}
	// The real time since the start, by the tick count, so that it follows a fixed frame time
	const auto ticks = Ticks();
	const auto step = static_cast<int32_t>(ticks - _lastFrameTicks);
	_lastFrameTicks = ticks;
	_elapsedTime += std::chrono::milliseconds(step > 0 ? step : 0);
}

void TimeSystem::StartDraw()
{
	// The draw's real time in whole milliseconds since the last draw, never none
	const auto ticks = Ticks();
	const auto step = static_cast<int32_t>(ticks - _lastDrawTicks);
	_frameRealMs = step > 0 ? static_cast<uint32_t>(step) : 1u;
	_lastDrawTicks = ticks;
}

std::chrono::milliseconds TimeSystem::GetElapsedTime() const
{
	return _elapsedTime;
}

void TimeSystem::StartGameClock(bool paused)
{
	const auto now = Ticks();
	_turn = 0;
	_timer.base = now;
	_timer.elapsed = 0;
	_timer.Stop(now);
	_timer.Start(now);
	_paused = paused;
	if (Stopped())
	{
		_timer.Stop(now);
	}
	_turnsThisFrame = 0;
	_lastFrameSample = 0;
	_lastFrameTurn = 0;
	_turnRemainderMs = 0;
	_visualMs = 0;
	_frameGameMs = 0;
	_turnFraction = 0.0f;
	ResetTimerToTurn();
}

void TimeSystem::RestartTurns()
{
	_turn = 0;
}

void TimeSystem::ResumeAtTurn(uint32_t turn, int32_t remainderMs)
{
	_turn = turn;
	_turnsThisFrame = 0;
	_lastFrameTurn = turn;
	_turnRemainderMs = std::clamp(remainderMs, 0, k_MaxRemainderMs);
	_visualMs = turn * k_TurnMs + static_cast<uint32_t>(_turnRemainderMs);
	_frameGameMs = 0;
	_turnFraction = static_cast<float>(_turnRemainderMs) * k_FractionPerMs;
	// The timer stands at the start of the last turn played plus the time past it, so the next turn comes when the rest of
	// its time has gone. The frame clock counts on from there.
	const auto now = Ticks();
	const auto running = _timer.speed != 0.0f;
	_timer.Stop(now);
	_timer.base = now;
	_timer.elapsed = std::max(static_cast<int32_t>(_turn * k_TurnMs) - static_cast<int32_t>(k_TurnMs), 0) + _turnRemainderMs;
	if (running)
	{
		_timer.Start(now);
	}
	_lastFrameSample = _timer.Milliseconds(now);
}

bool TimeSystem::IsTurnDue()
{
	if (Stopped() || _turnsThisFrame >= (_catchesUp ? k_MaxNetworkTurnsPerFrame : k_MaxTurnsPerFrame))
	{
		return false;
	}
	const auto timer = _timer.Milliseconds(Ticks());
	const auto due = static_cast<int32_t>(_turn * k_TurnMs);
	// A player's computer in a network game keeps the time it lost, and catches up
	if (!_catchesUp && timer - due > k_MaxLagMs)
	{
		ResetTimerToTurn();
	}
	// From the time taken before any reset
	return timer >= due;
}

void TimeSystem::StartTurn()
{
	++_turnsThisFrame;
	if (!Stopped())
	{
		++_turn;
	}
}

void TimeSystem::ResetTimerToTurn()
{
	const auto now = Ticks();
	const auto running = _timer.speed != 0.0f;
	_timer.Stop(now);
	_timer.base = now;
	_timer.elapsed = static_cast<int32_t>(_turn * k_TurnMs);
	if (running)
	{
		_timer.Start(now);
	}
}

void TimeSystem::UpdateFrame()
{
	_turnsThisFrame = 0;
	CatchUpFrame();
}

void TimeSystem::CatchUpFrame()
{
	if (Stopped())
	{
		// The fraction stays as it was
		_frameGameMs = 0;
		return;
	}
	const auto sample = _timer.Milliseconds(Ticks());
	const auto delta = sample - _lastFrameSample;
	_lastFrameSample = sample;
	// The time past the last turn played: a new turn takes a turn's length off it
	if (_lastFrameTurn == _turn)
	{
		_turnRemainderMs += delta;
	}
	else
	{
		_turnRemainderMs += delta - static_cast<int32_t>(k_TurnMs);
		_lastFrameTurn = _turn;
	}
	_turnRemainderMs = std::clamp(_turnRemainderMs, 0, k_MaxRemainderMs);
	// The visual clock can go back, and then the frame takes no time
	const auto visual = _turn * k_TurnMs + static_cast<uint32_t>(_turnRemainderMs);
	if (visual < _visualMs)
	{
		_visualMs = visual;
		_turnRemainderMs = 0;
	}
	_frameGameMs = visual - _visualMs;
	_visualMs = visual;
	_turnFraction = static_cast<float>(_turnRemainderMs) * k_FractionPerMs;
}

void TimeSystem::SetPaused(bool paused)
{
	const bool wasStopped = Stopped();
	_paused = paused;
	FollowStop(wasStopped);
}

void TimeSystem::SetPlayerPause(bool paused)
{
	if (PlayerCanStopClock(_networkGame))
	{
		SetPaused(paused);
	}
}

void TimeSystem::SetNetworkGame(bool networkGame)
{
	_networkGame = networkGame;
	// A hold that started before the game became a network game ends
	if (networkGame && _held)
	{
		SetHeld(false);
	}
}

void TimeSystem::SetHeld(bool held)
{
	if (held && !PlayerCanStopClock(_networkGame))
	{
		return;
	}
	const bool wasStopped = Stopped();
	_held = held;
	// The things that the frame steps after the start of the hold are in the stopped world, so they get no game time
	if (held)
	{
		_frameGameMs = 0;
	}
	FollowStop(wasStopped);
}

void TimeSystem::FollowStop(bool wasStopped)
{
	if (Stopped() == wasStopped)
	{
		return;
	}
	const auto now = Ticks();
	if (Stopped())
	{
		_timer.Stop(now);
	}
	else
	{
		_timer.Start(now);
	}
}

void TimeSystem::SetSpeed(float speed)
{
	_speed = speed;
	_timer.SetSpeed(speed, Ticks());
}
