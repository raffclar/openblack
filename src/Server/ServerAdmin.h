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

#include "Server/LockstepHost.h"

/// The report of the room of a game server for its operator. The inspector of the server, on 127.0.0.1 only, shows it.
/// The report has no secret: no secret of a session and no hash of a secret.
namespace openblack::server::admin
{

/// One god slot of the room
struct SeatReport
{
	uint8_t seat {0};
	/// "open", "taken", "away" or "closed"
	std::string_view state;
	std::string name;
	/// The account of the player at the lobby: the subject of the ticket
	std::string account;
	bool ready {false};
	/// A socket of the player is open now
	bool connected {false};
	uint32_t sessionNumber {0};
	uint32_t resumes {0};
	/// The commands of the god that the server put in its turns
	uint32_t commands {0};
	/// The blocks that the client sent again and that the server left out
	uint32_t blocksAgain {0};
	uint32_t checksumsCompared {0};
	uint32_t checksumsSame {0};
	std::optional<uint32_t> firstDifferentTurn;
	/// The last checksum of the client that the server compared differed from the checksum of the server
	bool differs {false};

	bool operator==(const SeatReport&) const = default;
};

/// The whole room
struct RoomReport
{
	bool starting {false};
	bool started {false};
	/// The game started and no player of a god is connected
	bool paused {false};
	bool gameOver {false};
	/// The world turns the replayed turns of a restored game
	bool replaying {false};
	/// The next turn that the server closes
	uint32_t turnsDone {0};
	std::optional<uint32_t> stoppedAfter;
	/// The observers that are connected or can come back
	uint32_t observers {0};
	uint32_t refusedBlocks {0};
	/// The account ids that the lobby banned from the server
	std::vector<int64_t> bans;
	std::vector<SeatReport> seats;

	bool operator==(const RoomReport&) const = default;
};

/// The name of the state of a god slot in the report
[[nodiscard]] std::string_view StateName(LockstepHost::SeatState state) noexcept;

/// The report of one god slot. The report copies no secret of the session and no hash of a secret.
[[nodiscard]] SeatReport SeatReportOf(uint8_t seat, const LockstepHost::Seat& state);

/// The report of the room of the host
[[nodiscard]] RoomReport RoomReportOf(const LockstepHost& host);

} // namespace openblack::server::admin
