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
		return "the player has no editor claim";
	case Refusal::SpeedOutOfRange:
		return "the speed is not from 0.1 to 10";
	case Refusal::StepWhilePlaying:
		return "a step comes only while the game is paused";
	case Refusal::TooManySteps:
		return "too many steps wait for their turns";
	}
	return "";
}

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

/// Why the server does not take the request, none when it takes it
[[nodiscard]] inline std::optional<Refusal> RefusalOf(const Rule& rule, bool editorClaim, const State& state,
                                                      const game_socket::EditorTime& request) noexcept
{
	if (!MayChange(rule, editorClaim))
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

/// What the clock of a computer does when a frame of the clock of the room comes
struct Follow
{
	/// The new speed of the game clock, none when the time of a turn did not change
	std::optional<float> clockSpeed;
	/// The next turn starts from its turn, not as a turn that is late, because the turns stopped for a pause
	bool fromTurn {false};

	[[nodiscard]] bool operator==(const Follow&) const = default;
};

/// How the clock of a computer follows a frame of the clock of the room, from the time of a turn that it set last
[[nodiscard]] constexpr Follow FollowOf(const game_socket::GameTime& room, float lastSpeed) noexcept
{
	return Follow {.clockSpeed = room.speed != lastSpeed ? std::optional(ClockSpeedOf(room.speed)) : std::nullopt,
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
