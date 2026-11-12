/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>
#include <string>
#include <string_view>

/// The life of a game server of the pool, which plays the game of one lobby and then ends (deviations 236 and 247). The
/// server ends when every god left, when the game is decided and the players had time to see the end, when no game
/// started in the time to join, or at its deadline (6 hours at the latest). Each end has its own exit code, so that the
/// slot agent knows why the server ended. Pure, tested with fakes.
namespace openblack::server::game_lifetime
{

/// The longest time limit of a lobby game in minutes: 6 hours
inline constexpr uint32_t k_LongestLobbyTimeLimitMinutes = 360;
/// The longest life of a game server of a lobby game in seconds, from the time when it listens: 6 hours
inline constexpr int64_t k_LongestLifeSeconds = 6LL * 60 * 60;
/// The time in seconds that the members of the lobby have to join, when the operator does not give one
inline constexpr int64_t k_DefaultJoinWindowSeconds = 120;
/// The server writes a warning to its log this many seconds before its deadline
inline constexpr int64_t k_WarningSeconds = 10LL * 60;
/// The time in seconds that the players have to see the end of a decided game before the server ends
inline constexpr int64_t k_AfterDecisionSeconds = 5LL * 60;

/// Why a game server ends
enum class End : uint8_t
{
	/// The server continues
	None,
	/// Every god left the game, or was defeated, before a decision of the time limit
	AllGodsLeft,
	/// The time limit decided the game: then every god left, or the time after the decision ended
	TimeLimit,
	/// The deadline of the server came: the lobby's deadline, or 6 hours after the server started to listen
	Deadline,
	/// No game started in the time to join: too few gods joined, or the gods in the room were not ready
	NoStart,
	/// The winning conditions decided the game, and the time after the decision ended with a god still in the game
	Decided,
};

/// The exit code of the server for each end. A failure of the server is EXIT_FAILURE (1): the slot agent starts the
/// server again from its snapshot. The agent does not start the server again after one of these codes.
inline constexpr int k_ExitAllGodsLeft = 0;
inline constexpr int k_ExitTimeLimit = 10;
inline constexpr int k_ExitDeadline = 11;
inline constexpr int k_ExitNoStart = 12;
inline constexpr int k_ExitDecided = 13;

[[nodiscard]] int ExitCodeOf(End end) noexcept;

/// The line of the log for the end, with the exit code
[[nodiscard]] std::string LineOf(End end);

/// Why the operator cannot give a lobby game this time limit in minutes, or none when the operator can. The limits are
/// those of the host's controls, with 6 hours as the longest. A build with the developer tools also accepts a time limit
/// of 1 to 9 minutes, for short test games.
[[nodiscard]] std::optional<std::string_view> LobbyTimeLimitRefusal(uint32_t minutes, bool developerTools) noexcept;

/// The deadline of the server in seconds since 1970 (UTC): the lobby's deadline, but 6 hours after the server started to
/// listen at the latest
[[nodiscard]] int64_t DeadlineOf(std::optional<int64_t> lobbyDeadline, int64_t listenedAt) noexcept;

/// The limits of the life of the server
struct Limits
{
	/// The time in seconds after the server started to listen in which the members of the lobby join. None: the room
	/// waits for its players with no limit, as a server with no lobby does.
	std::optional<int64_t> joinWindowSeconds;
	/// The deadline in seconds since 1970 (UTC). None: no deadline.
	std::optional<int64_t> deadline;
};

/// What the server sees in its room at one time
struct Room
{
	/// The seconds since the server started to listen
	int64_t secondsSinceListen {0};
	/// The time in seconds since 1970 (UTC)
	int64_t now {0};
	/// The game started, or the start handshake runs
	bool started {false};
	/// The gods with a player in the room before the start
	size_t godsInRoom {0};
	/// The fewest gods that start the game
	uint32_t minGods {1};
	/// Every god left the game after the start
	bool gameOver {false};
	/// The seconds since 1970 (UTC) when the game was decided. None while the game is not decided.
	std::optional<int64_t> decidedAt;
	/// The time limit decided the game
	bool decidedByTimeLimit {false};
};

/// Why the server ends now, or End::None. The rules, in this order:
/// - Every god left the game: TimeLimit after a decision of the time limit, else AllGodsLeft.
/// - The deadline came: Deadline.
/// - No start at the end of the time to join with too few gods in the room, or no start at two times the time to join:
///   NoStart.
/// - The players of a decided game had the time to see the end: TimeLimit or Decided.
[[nodiscard]] End Check(const Limits& limits, const Room& room) noexcept;

/// The gods for which the room waits before the start: every god of the lobby during the time to join, then the fewest
/// gods that start the game
[[nodiscard]] uint32_t GodsToWaitFor(size_t lobbyGods, uint32_t minGods, bool joinWindowOver) noexcept;

/// True when the warning before the deadline is due: from 10 minutes before the deadline
[[nodiscard]] bool WarningDue(std::optional<int64_t> deadline, int64_t now) noexcept;

} // namespace openblack::server::game_lifetime
