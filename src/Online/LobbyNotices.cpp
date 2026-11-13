/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LobbyNotices.h"

#include <algorithm>
#include <iterator>

namespace openblack::online::lobby
{

namespace
{

[[nodiscard]] constexpr bool IsGameStart(RoomPhase phase) noexcept
{
	return phase == RoomPhase::Assigned || phase == RoomPhase::InGame;
}

[[nodiscard]] constexpr bool IsRemoval(std::optional<EndReason> end) noexcept
{
	return end == EndReason::Kicked || end == EndReason::Banned;
}

} // namespace

NoticeSnapshot SnapshotOf(const LobbyRoom& room)
{
	NoticeSnapshot snapshot {
	    .lobbyId = room.Data().id,
	    .phase = room.Phase(),
	    .end = room.End().has_value() ? std::optional(room.End()->reason) : std::nullopt,
	    .joins = room.SystemLineCount(SystemLineKind::Joined),
	    .leaves = room.SystemLineCount(SystemLineKind::Left),
	    .removals = room.SystemLineCount(SystemLineKind::Kicked) + room.SystemLineCount(SystemLineKind::Banned),
	    .countdownSeconds = room.SystemLineCount(SystemLineKind::Countdown),
	    .readyPlayers = {},
	};
	for (const auto& slot : room.Slots())
	{
		if (slot.holder == SlotHolder::Player && slot.ready && !slot.own)
		{
			snapshot.readyPlayers.push_back(slot.account.id);
		}
	}
	std::ranges::sort(snapshot.readyPlayers);
	return snapshot;
}

LobbyNotice NoticeBetween(const NoticeSnapshot& before, const NoticeSnapshot& after)
{
	if (before.lobbyId != after.lobbyId)
	{
		return {};
	}
	const bool removed = !IsRemoval(before.end) && IsRemoval(after.end);
	if (after.phase == RoomPhase::Ended)
	{
		// The room of the player ended. Only a kick or a ban gives a notice: the leave of the player has its own click.
		return {.sound = removed, .flash = removed};
	}
	std::vector<int64_t> newlyReady;
	std::ranges::set_difference(after.readyPlayers, before.readyPlayers, std::back_inserter(newlyReady));
	const bool countdownStarts = before.phase == RoomPhase::Open && after.phase == RoomPhase::Countdown;
	const bool gameStarts = !IsGameStart(before.phase) && before.phase != RoomPhase::Ended && IsGameStart(after.phase);
	const bool flash = countdownStarts || gameStarts;
	const bool sound = flash || after.joins > before.joins || after.leaves > before.leaves ||
	                   after.removals > before.removals || after.countdownSeconds > before.countdownSeconds ||
	                   !newlyReady.empty();
	return {.sound = sound, .flash = flash};
}

} // namespace openblack::online::lobby
