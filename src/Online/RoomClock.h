/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cmath>
#include <cstdint>

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

#include <GameSocket.h>
#include <fmt/format.h>

/// The clock of the room of a network game: the editor's pause, step and speed for every player of the room. The game
/// server keeps the state and sends the turns by it. Each computer follows the state that the server sends.
namespace openblack::online::room_clock
{

/// Who can change the clock of the room
enum class Right : uint8_t
{
	/// Each player whose join ticket has the editor claim, a god or an observer
	EditorClaim,
	/// No player
	Nobody,
};

/// The rule of the room: who can change the clock, and whether every player sees a line for each change. The user can
/// change this rule (question ED-1).
struct Rule
{
	Right right {Right::EditorClaim};
	bool tellsEveryPlayer {true};
};

/// The rule that the game uses
inline constexpr Rule k_Rule {.right = Right::EditorClaim, .tellsEveryPlayer = true};

/// The fastest and the slowest time of a turn, as a multiple of a tenth of a second, as the speed of the editor gives it
inline constexpr float k_FastestSpeed = 0.1f;
inline constexpr float k_SlowestSpeed = 10.0f;
/// The normal time of a turn
inline constexpr float k_NormalSpeed = 1.0f;
/// The most steps that can wait for their turns
inline constexpr uint32_t k_MostWaitingSteps = 10;

/// The clock of the room, as the server keeps it
struct State
{
	bool paused {false};
	float speed {k_NormalSpeed};
	/// The turns of steps that the server did not send yet
	uint32_t steps {0};
	/// The time between two turns in milliseconds, when the last change of the speed came from the speed of the game
	/// (see PeriodOf). None after a change of the speed of the editor.
	std::optional<uint32_t> period;

	[[nodiscard]] bool operator==(const State&) const = default;
};

/// Why the server does not take a request
enum class Refusal : uint8_t
{
	/// The rule does not let the player change the clock
	NoRight,
	/// The speed is not a number from the fastest to the slowest
	SpeedOutOfRange,
	/// A step comes only while the clock is paused
	StepWhilePlaying,
	/// Too many steps wait for their turns
	TooManySteps,
};

[[nodiscard]] constexpr std::string_view RefusalText(Refusal refusal) noexcept
{
	switch (refusal)
	{
	case Refusal::NoRight:
		return "the player has no editor claim, or a debugger claim for a change of the speed";
	case Refusal::SpeedOutOfRange:
		return "the speed is not from 0.1 to 10";
	case Refusal::StepWhilePlaying:
		return "a step comes only while the game is paused";
	case Refusal::TooManySteps:
		return "too many steps wait for their turns";
	}
	return "";
}

/// The claims of the join ticket of a player that the rule reads
struct Claims
{
	bool editor {false};
	/// The debugger claim lets the player change only the speed, as the Game Speed menu of the debug menu bar does
	bool debugger {false};
};

/// Whether the rule lets a player change the clock
[[nodiscard]] constexpr bool MayChange(const Rule& rule, bool editorClaim) noexcept
{
	switch (rule.right)
	{
	case Right::EditorClaim:
		return editorClaim;
	case Right::Nobody:
		return false;
	}
	return false;
}

/// Whether the rule lets a player with the claims do the act. The editor claim gives each act, and the debugger claim
/// gives only a change of the speed.
[[nodiscard]] constexpr bool MayDo(const Rule& rule, const Claims& claims, game_socket::TimeAct act) noexcept
{
	return MayChange(rule, claims.editor || (claims.debugger && act == game_socket::TimeAct::Speed));
}

/// Why the server does not take the request, none when it takes it
[[nodiscard]] inline std::optional<Refusal> RefusalOf(const Rule& rule, const Claims& claims, const State& state,
                                                      const game_socket::EditorTime& request) noexcept
{
	if (!MayDo(rule, claims, request.act))
	{
		return Refusal::NoRight;
	}
	switch (request.act)
	{
	case game_socket::TimeAct::Speed:
		if (!std::isfinite(request.speed) || request.speed < k_FastestSpeed || request.speed > k_SlowestSpeed)
		{
			return Refusal::SpeedOutOfRange;
		}
		break;
	case game_socket::TimeAct::Step:
		if (!state.paused)
		{
			return Refusal::StepWhilePlaying;
		}
		if (state.steps >= k_MostWaitingSteps)
		{
			return Refusal::TooManySteps;
		}
		break;
	case game_socket::TimeAct::Pause:
	case game_socket::TimeAct::Play:
		break;
	}
	return std::nullopt;
}

/// Why the server does not take the request of a player with only the editor claim or no claim
[[nodiscard]] inline std::optional<Refusal> RefusalOf(const Rule& rule, bool editorClaim, const State& state,
                                                      const game_socket::EditorTime& request) noexcept
{
	return RefusalOf(rule, Claims {.editor = editorClaim}, state, request);
}

/// The clock after a request that the server took. A pause or a play also drops the steps that wait.
[[nodiscard]] constexpr State Apply(State state, const game_socket::EditorTime& request) noexcept
{
	switch (request.act)
	{
	case game_socket::TimeAct::Pause:
		state.paused = true;
		state.steps = 0;
		break;
	case game_socket::TimeAct::Play:
		state.paused = false;
		state.steps = 0;
		break;
	case game_socket::TimeAct::Step:
		++state.steps;
		break;
	case game_socket::TimeAct::Speed:
		state.speed = request.speed;
		state.period.reset();
		break;
	}
	return state;
}

/// Whether the server sends the next turn by the clock of the room
[[nodiscard]] constexpr bool SendsTurn(const State& state) noexcept
{
	return !state.paused || state.steps > 0;
}

/// The clock after the server sent a turn: a turn of a step uses that step
[[nodiscard]] constexpr State TurnSent(State state) noexcept
{
	if (state.paused && state.steps > 0)
	{
		--state.steps;
	}
	return state;
}

/// The speed of the game clock for a time of a turn: a turn of twice the time is half the speed
[[nodiscard]] constexpr float ClockSpeedOf(float speed) noexcept
{
	return 1.0f / speed;
}

/// The time between two turns at the speed of the game 1, in milliseconds
inline constexpr float k_NormalPeriod = 100.0f;
/// The shortest and the longest time between two turns that the speed of the game can ask for, in milliseconds
inline constexpr uint32_t k_ShortestPeriod = 1;
inline constexpr uint32_t k_LongestPeriod = 100'000;

/// The time between two turns in milliseconds that a speed of the game asks for in a network game: 100 divided by the
/// speed, with the fraction cut off, as the game calculates it. None when the time is not from k_ShortestPeriod to
/// k_LongestPeriod: then the game asks for no change.
[[nodiscard]] inline std::optional<uint32_t> PeriodOf(float speed) noexcept
{
	const float period = k_NormalPeriod / speed;
	// NOLINTNEXTLINE(readability-simplify-boolean-expr): The negated test also takes a NaN value.
	if (!(period >= static_cast<float>(k_ShortestPeriod) && period < static_cast<float>(k_LongestPeriod + 1)))
	{
		return std::nullopt;
	}
	return static_cast<uint32_t>(period);
}

/// The time of a turn of the room, as a multiple of a tenth of a second, for a time between two turns in milliseconds
[[nodiscard]] constexpr float SpeedOfPeriod(uint32_t period) noexcept
{
	return static_cast<float>(period) / k_NormalPeriod;
}

/// The time between two turns that this computer has from the server: the period of the last change of the clock of
/// the room, or the normal period before the server sent a change. None after a change of the speed of the editor.
[[nodiscard]] constexpr std::optional<uint32_t> PeriodOfRoom(const std::optional<game_socket::GameTime>& room) noexcept
{
	if (!room.has_value() || (!room->period.has_value() && room->speed == k_NormalSpeed))
	{
		return static_cast<uint32_t>(k_NormalPeriod);
	}
	return room->period;
}

/// Whether a speed of the game asks the server for a new time between two turns. The request does not go when the
/// time does not change.
[[nodiscard]] constexpr bool SendsPeriod(std::optional<uint32_t> roomPeriod, uint32_t period) noexcept
{
	return roomPeriod != period;
}

/// The clock after a request of the speed of the game that the server took
[[nodiscard]] constexpr State ApplyPeriod(State state, uint32_t period) noexcept
{
	state.speed = SpeedOfPeriod(period);
	state.period = period;
	return state;
}

/// The speed of the game clock for a time between two turns in milliseconds: 100 divided by the time
[[nodiscard]] constexpr float ClockSpeedOfPeriod(uint32_t period) noexcept
{
	return k_NormalPeriod / static_cast<float>(period);
}

/// The speed of the game clock for a clock of the room: from its period when the speed of the game set it, else from
/// the time of a turn
[[nodiscard]] constexpr float ClockSpeedOf(float speed, std::optional<uint32_t> period) noexcept
{
	return period.has_value() ? ClockSpeedOfPeriod(*period) : ClockSpeedOf(speed);
}

/// The time of a turn at the normal speed of the room
inline constexpr std::chrono::microseconds k_NormalTurnTime {100'000};
/// The most turns that the game server plays in one frame. A player's computer that catches up plays as many.
inline constexpr uint32_t k_MostServerTurnsInFrame = 10;

/// The most turns that the game server plays in one frame of this time, at this time of a turn of the room. It is the
/// number of turns that the clock of the room gives in the frame, rounded up, from 1 to k_MostServerTurnsInFrame. At
/// the normal speed and a frame of 100 ms, it is 1. A server with no fixed frame time takes the time of a normal turn.
[[nodiscard]] constexpr uint32_t ServerTurnsInFrame(float speed, std::optional<std::chrono::microseconds> frameTime) noexcept
{
	// NOLINTNEXTLINE(readability-simplify-boolean-expr): The negated test also takes a NaN value.
	if (!(speed >= SpeedOfPeriod(k_ShortestPeriod) && speed <= SpeedOfPeriod(k_LongestPeriod)))
	{
		return 1;
	}
	const auto frame = static_cast<double>(frameTime.value_or(k_NormalTurnTime).count());
	const auto turns = frame * static_cast<double>(ClockSpeedOf(speed)) / static_cast<double>(k_NormalTurnTime.count());
	if (!(turns > 1.0))
	{
		return 1;
	}
	if (turns >= static_cast<double>(k_MostServerTurnsInFrame))
	{
		return k_MostServerTurnsInFrame;
	}
	const auto whole = static_cast<uint32_t>(turns);
	return static_cast<double>(whole) < turns ? whole + 1 : whole;
}

/// What the clock of a computer does when a frame of the clock of the room comes
struct Follow
{
	/// The new speed of the game clock, none when the time of a turn did not change
	std::optional<float> clockSpeed;
	/// The next turn starts from its turn, not as a turn that is late, because the turns stopped for a pause
	bool fromTurn {false};

	[[nodiscard]] bool operator==(const Follow&) const = default;
};

/// How the clock of a computer follows a frame of the clock of the room, from the time of a turn and the period that it
/// set last
[[nodiscard]] constexpr Follow FollowOf(const game_socket::GameTime& room, float lastSpeed,
                                        std::optional<uint32_t> lastPeriod = std::nullopt) noexcept
{
	const bool changed = room.speed != lastSpeed || room.period != lastPeriod;
	return Follow {.clockSpeed = changed ? std::optional(ClockSpeedOf(room.speed, room.period)) : std::nullopt,
	               .fromTurn = room.act == game_socket::TimeAct::Play || room.act == game_socket::TimeAct::Step};
}

/// The line that every player sees after a change of the clock. None for a step, which comes too often for a line, and
/// for a frame with no name of a player: a frame to a player who joins or comes back, or a change of the server.
[[nodiscard]] inline std::optional<std::string> NoticeOf(const game_socket::GameTime& time)
{
	if (time.by.empty())
	{
		return std::nullopt;
	}
	switch (time.act)
	{
	case game_socket::TimeAct::Pause:
		return fmt::format("{} paused the game", time.by);
	case game_socket::TimeAct::Play:
		return fmt::format("{} started the game again", time.by);
	case game_socket::TimeAct::Speed:
		return fmt::format("{} set the speed of the game to {:.2f}", time.by, time.speed);
	case game_socket::TimeAct::Step:
		return std::nullopt;
	}
	return std::nullopt;
}

} // namespace openblack::online::room_clock
