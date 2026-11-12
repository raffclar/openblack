/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameLifetime.h"

#include <algorithm>

#include <fmt/format.h>

#include "ECS/GameEndConditions.h"

namespace openblack::server::game_lifetime
{

int ExitCodeOf(End end) noexcept
{
	switch (end)
	{
	case End::None:
	case End::AllGodsLeft:
		return k_ExitAllGodsLeft;
	case End::TimeLimit:
		return k_ExitTimeLimit;
	case End::Deadline:
		return k_ExitDeadline;
	case End::NoStart:
		return k_ExitNoStart;
	case End::Decided:
		return k_ExitDecided;
	}
	return k_ExitAllGodsLeft;
}

std::string LineOf(End end)
{
	std::string_view why = "no end";
	switch (end)
	{
	case End::None:
		break;
	case End::AllGodsLeft:
		why = "every god left the game";
		break;
	case End::TimeLimit:
		why = "the time limit decided the game";
		break;
	case End::Deadline:
		why = "the deadline of the server came";
		break;
	case End::NoStart:
		why = "no game started in the time to join";
		break;
	case End::Decided:
		why = "the winning conditions decided the game";
		break;
	}
	return fmt::format("The game server ends: {} (exit code {})", why, ExitCodeOf(end));
}

std::optional<std::string_view> LobbyTimeLimitRefusal(uint32_t minutes, bool developerTools) noexcept
{
	if (minutes > k_LongestLobbyTimeLimitMinutes)
	{
		return "the time limit of a lobby game must be 360 minutes (6 hours) or less";
	}
	const auto shortest = developerTools ? game_end::k_ShortestTestTimeLimitMinutes : game_end::k_ShortestTimeLimitMinutes;
	if (minutes < shortest)
	{
		return developerTools ? "the time limit must be 1 minute or more" : "the time limit must be 10 minutes or more";
	}
	return std::nullopt;
}

int64_t DeadlineOf(std::optional<int64_t> lobbyDeadline, int64_t listenedAt) noexcept
{
	const auto longest = listenedAt + k_LongestLifeSeconds;
	return lobbyDeadline.has_value() ? std::min(*lobbyDeadline, longest) : longest;
}

End Check(const Limits& limits, const Room& room) noexcept
{
	if (room.gameOver)
	{
		return room.decidedAt.has_value() && room.decidedByTimeLimit ? End::TimeLimit : End::AllGodsLeft;
	}
	if (limits.deadline.has_value() && room.now >= *limits.deadline)
	{
		return End::Deadline;
	}
	if (!room.started && limits.joinWindowSeconds.has_value())
	{
		const auto window = *limits.joinWindowSeconds;
		const bool tooFew = room.godsInRoom < std::max<uint32_t>(room.minGods, 1);
		if ((room.secondsSinceListen >= window && tooFew) || room.secondsSinceListen >= 2 * window)
		{
			return End::NoStart;
		}
	}
	if (room.decidedAt.has_value() && room.now - *room.decidedAt >= k_AfterDecisionSeconds)
	{
		return room.decidedByTimeLimit ? End::TimeLimit : End::Decided;
	}
	return End::None;
}

uint32_t GodsToWaitFor(size_t lobbyGods, uint32_t minGods, bool joinWindowOver) noexcept
{
	if (joinWindowOver)
	{
		return minGods;
	}
	return std::max(minGods, static_cast<uint32_t>(lobbyGods));
}

bool WarningDue(std::optional<int64_t> deadline, int64_t now) noexcept
{
	return deadline.has_value() && now >= *deadline - k_WarningSeconds;
}

} // namespace openblack::server::game_lifetime
