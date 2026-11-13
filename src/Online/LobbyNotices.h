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

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "LobbyRoom.h"

/// The notices of the lobby room (deviation 251). The front end plays its own click sound, and flashes the window or the
/// title of the browser tab, when an event of the room occurs. These rules have no global state, so that the tests can
/// give them rooms.
namespace openblack::online::lobby
{

/// The title of the browser tab while the tab asks for the attention of the player
constexpr std::string_view k_AttentionTitle = "(!) openblack";

/// The parts of the room that the notices compare from one frame to the next
struct NoticeSnapshot
{
	std::string lobbyId;
	RoomPhase phase {RoomPhase::Open};
	std::optional<EndReason> end;
	/// The counts of the system lines of the room
	uint32_t joins {0};
	uint32_t leaves {0};
	/// The other players that a host kicked or banned
	uint32_t removals {0};
	/// The lines of the seconds of the countdown
	uint32_t countdownSeconds {0};
	/// The accounts of the other players in god slots that are ready, in increasing order
	std::vector<int64_t> readyPlayers;
};

/// What the front end does for the events between two snapshots
struct LobbyNotice
{
	/// Play the click sound of the front end
	bool sound {false};
	/// Ask for the attention of the player: the window flashes, or the browser tab shows k_AttentionTitle
	bool flash {false};

	[[nodiscard]] constexpr bool Any() const noexcept { return sound || flash; }
	[[nodiscard]] constexpr bool operator==(const LobbyNotice&) const noexcept = default;
};

[[nodiscard]] NoticeSnapshot SnapshotOf(const LobbyRoom& room);

/// The notice of the events between the two snapshots of the same lobby:
/// - A join, a leave, a kick or a ban of a different player, a different player that becomes ready, and each second of
///   the countdown play the sound.
/// - The start of the countdown, the start of the game, and a kick or a ban of the player play the sound and flash.
/// The end of the room gives no other notice. Two snapshots of different lobbies give no notice.
[[nodiscard]] LobbyNotice NoticeBetween(const NoticeSnapshot& before, const NoticeSnapshot& after);

/// The window flashes only when it does not have the focus
[[nodiscard]] constexpr bool FlashesWindow(const LobbyNotice& notice, bool focused) noexcept
{
	return notice.flash && !focused;
}

} // namespace openblack::online::lobby
