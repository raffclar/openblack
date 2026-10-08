/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptTimer.h"

#include "ECS/Registry.h"
#include "ECS/Systems/ScriptStateInterface.h"
#include "GameClock.h"
#include "Locator.h"

namespace openblack::ecs
{
using components::ScriptTimer;

namespace
{
/// 0.001f, ms -> s
constexpr float k_SecondsPerMs = game_clock::k_SecondsPerMs;

/// turns x ms per turn x 0.001 with the FPU at 24 bits (game_clock.h): each step rounded to a float
float TurnsToSeconds(int32_t turns, uint32_t msPerTurn)
{
	const float ms = static_cast<float>(turns) * static_cast<float>(static_cast<int32_t>(msPerTurn));
	return ms * k_SecondsPerMs;
}

ScriptTimer* TimerOf(entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	return thing != entt::null && registry.Valid(thing) ? registry.TryGet<ScriptTimer>(thing) : nullptr;
}

/// What this module keeps between calls (Locator::scriptState)
struct ScriptTimerState
{
	script_countdown::State countdown {};
};

ScriptTimerState& ScriptTimerData()
{
	return openblack::Locator::scriptState::value().Get<ScriptTimerState>();
}
} // namespace

void script_timer::SetTurns(ScriptTimer& timer, int32_t turns, uint32_t now)
{
	timer.startTurn = now;
	timer.durationTurns = turns;
}

void script_timer::SetSeconds(ScriptTimer& timer, float seconds, uint32_t now)
{
	SetTurns(timer, game_clock::TicksForSeconds(seconds), now);
}

uint32_t script_timer::ElapsedTurns(const ScriptTimer& timer, uint32_t now)
{
	return now - timer.startTurn;
}

float script_timer::RemainingSeconds(const ScriptTimer& timer, uint32_t now, uint32_t msPerTurn)
{
	// A timer past its end gives 0 (never negative)
	const int32_t left = timer.durationTurns - static_cast<int32_t>(ElapsedTurns(timer, now));
	return TurnsToSeconds(left < 0 ? 0 : left, msPerTurn);
}

float script_timer::SecondsSinceSet(const ScriptTimer& timer, uint32_t now, uint32_t msPerTurn)
{
	return TurnsToSeconds(static_cast<int32_t>(ElapsedTurns(timer, now)), msPerTurn);
}

entt::entity script_timer::Create(float seconds)
{
	// A new thing, set to 0 turns, then SetSeconds(seconds). (not ported) "out of memory" gives 0 there ("Thing not
	// created")
	auto& registry = Locator::entitiesRegistry::value();
	const auto thing = registry.Create();
	auto& timer = registry.Assign<ScriptTimer>(thing);
	SetSeconds(timer, seconds, game_clock::Turn());
	return thing;
}

bool script_timer::IsTimer(entt::entity thing)
{
	return TimerOf(thing) != nullptr;
}

bool script_timer::SetTime(entt::entity thing, float seconds)
{
	auto* timer = TimerOf(thing);
	if (timer == nullptr)
	{
		return false;
	}
	SetSeconds(*timer, seconds, game_clock::Turn()); // restarts it from this turn
	return true;
}

std::optional<float> script_timer::Remaining(entt::entity thing)
{
	const auto* timer = TimerOf(thing);
	if (timer == nullptr)
	{
		return std::nullopt;
	}
	return RemainingSeconds(*timer, game_clock::Turn(), game_clock::MsPerTurn());
}

std::optional<float> script_timer::SinceSet(entt::entity thing)
{
	const auto* timer = TimerOf(thing);
	if (timer == nullptr)
	{
		return std::nullopt;
	}
	return SecondsSinceSet(*timer, game_clock::Turn(), game_clock::MsPerTurn());
}

script_timer::SaveRecord script_timer::ToSave(const ScriptTimer& timer)
{
	return {timer.startTurn, timer.durationTurns};
}

ScriptTimer script_timer::FromSave(const SaveRecord& record)
{
	return {record.startTurn, record.durationTurns};
}

bool script_countdown::Start(float seconds)
{
	auto& state = ScriptTimerData();
	// On and shown first; only above 0 is valid
	state.countdown.on = true;
	state.countdown.shown = true;
	const bool valid = seconds > 0.0f; // else the script error "Invalid time for timer", not stopping
	if (seconds < 0.0f)
	{
		seconds = 0.0f;
	}
	state.countdown.turnsLeft = game_clock::TicksForSeconds(seconds);
	return valid;
}

void script_countdown::Remove()
{
	ScriptTimerData().countdown.on = false; // only that flag (the turns and shown stay)
}

float script_countdown::RemainingSeconds()
{
	// 1000 / ms per turn, then turns left / that (both unsigned). (inferred) more than 1000 ms per turn would divide
	// by 0 in the original; openblack gives 0
	const uint32_t ms = game_clock::MsPerTurn();
	const uint32_t perSecond = ms != 0 ? 1000u / ms : 0u;
	if (perSecond == 0)
	{
		return 0.0f;
	}
	return static_cast<float>(static_cast<uint32_t>(ScriptTimerData().countdown.turnsLeft) / perSecond);
}

bool script_countdown::Exists()
{
	return ScriptTimerData().countdown.on;
}

void script_countdown::SetShown(bool shown)
{
	ScriptTimerData().countdown.shown = shown;
}

void script_countdown::ProcessTurn()
{
	auto& state = ScriptTimerData();
	// Off when it reaches 0 (a 0-turn countdown goes on below 0, as the original)
	if (state.countdown.on && --state.countdown.turnsLeft == 0)
	{
		state.countdown.on = false;
	}
}

script_countdown::Display script_countdown::GetDisplay()
{
	auto& state = ScriptTimerData();
	const float seconds = RemainingSeconds();
	// Visible when on and shown; red below 10
	return {state.countdown.on && state.countdown.shown, seconds, seconds < 10.0f};
}

const script_countdown::State& script_countdown::Get()
{
	return ScriptTimerData().countdown;
}

void script_countdown::Reset()
{
	ScriptTimerData().countdown = {};
}

} // namespace openblack::ecs
