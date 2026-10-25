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
#include <span>
#include <string_view>
#include <vector>

#include <GameSocket.h>

/// The rules of the start of a network game: the check that each player has the server's land, and the creature files
/// that the gods send before the game starts
namespace openblack::online::start_rules
{

/// The checksum of a land file, which the players compare before a network game starts. It is two 32-bit sums of the
/// bytes of the file. The low sum adds each byte, and for each line feed also the low 6 bits of its place in the file.
/// The high sum adds 9 times each byte and each byte divided by 3. A file that does not open has the checksum 0.
[[nodiscard]] uint64_t LandChecksum(std::span<const uint8_t> file) noexcept;

/// Whether the land that the server names is a plain relative path in the Scripts folder: not empty, no root, no drive
/// and no ".." part
[[nodiscard]] bool IsLandPath(std::string_view land) noexcept;

/// The reply to the server's query. The testbed has no file, so each player has it, with the checksum of no file. A
/// land is there when its file was read.
[[nodiscard]] game_socket::StartReply Reply(const game_socket::StartQuery& query,
                                            const std::optional<std::vector<uint8_t>>& landFile) noexcept;

/// What the server does with the reply of a player
enum class Verdict : uint8_t
{
	/// The player has the server's land
	Same,
	/// The player does not have the land: the start stops
	Missing,
	/// The player's land file is not the server's: the start stops and the server removes the player from the room
	Different,
};

[[nodiscard]] Verdict Judge(uint64_t serverChecksum, const game_socket::StartReply& reply) noexcept;

} // namespace openblack::online::start_rules
