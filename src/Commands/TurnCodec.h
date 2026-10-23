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
#include <span>
#include <variant>
#include <vector>

#include "Commands/PlayerCommands.h"

/// The binary frames of the game socket between a client and the game server: the commands that a client sends, the
/// turns that the server sends, the checksums of the turns and the pings. All other messages on the socket are JSON
/// text frames. A binary frame starts with the version of the format and the kind of the frame. All numbers are little
/// endian, and each float goes as the 32 bits of its value, so every computer gets the same bits.
///
/// The format:
/// - Frame: version (u8), kind (u8), then the body of the kind.
/// - Commands (client to server): sequence (u16), turn (u32), count (u8), then the commands.
/// - Turn (server to clients): turn (u32), block count (u16), then each block: sender (u8), sequence (u16), count (u8),
///   then the commands.
/// - Checksum (client to server): turn (u32), checksum (u64).
/// - Ping and pong: id (u32), time (u64). A pong sends back the id and the time of its ping.
/// - Command: tag (u8), then the fields of the command in the order of its structure. A thing is its entity number
///   (u32). A place or a velocity is three floats. A let-go has the velocity, then the spin, the origin and the
///   angles of the start.
namespace openblack::turn_codec
{

/// The version of the format. A change of the format gives a new version.
inline constexpr uint8_t k_Version = 4;
/// The largest frame that a computer sends or accepts
inline constexpr size_t k_MostFrameBytes = 64 * 1024;
/// The most commands in one block, as a client sends them
inline constexpr size_t k_MostBlockCommands = player_commands::CommandQueue::k_BlockSize;

enum class FrameKind : uint8_t
{
	Commands = 1,
	Turn = 2,
	Checksum = 3,
	Ping = 4,
	Pong = 5,
};

/// The tag of each command on the wire. The numbers never change, also when the commands change their order.
enum class CommandTag : uint8_t
{
	LeashKey = 1,
	LeashTap = 2,
	CreatureFeedback = 3,
	HandTake = 4,
	HandLetGo = 5,
	HandTap = 6,
	HandGive = 7,
	LeashOrder = 8,
	LeashPostTap = 9,
	HandReport = 10,
	HandTwist = 11,
	HandLock = 12,
	HandUnlock = 13,
	HandApply = 14,
	TotemShare = 15,
	CreatureBruise = 16,
};

/// The sizes of the parts of the frames, in bytes
namespace sizes
{
inline constexpr size_t k_Header = 2;
inline constexpr size_t k_Entity = 4;
inline constexpr size_t k_Vector = 12;
/// The sequence, the turn and the count of a block that a client sends
inline constexpr size_t k_CommandsBody = 7;
/// The turn and the block count of a turn
inline constexpr size_t k_TurnBody = 6;
/// The sender, the sequence and the count of a block in a turn
inline constexpr size_t k_Stamp = 4;
inline constexpr size_t k_Checksum = k_Header + 12;
inline constexpr size_t k_Ping = k_Header + 12;
} // namespace sizes

/// The commands of one client, as it sends them to the server
struct CommandBlock
{
	/// The number of the block. Each client counts its blocks from 0, and the count wraps.
	uint16_t sequence {0};
	/// The last turn that the client did when it made the block
	uint32_t turn {0};
	std::vector<player_commands::Action> actions;
};

/// A block in a turn, with the stamp of the server: the player that sent it and the number of the block
struct StampedBlock
{
	PlayerNames sender {PlayerNames::PLAYER_ONE};
	uint16_t sequence {0};
	std::vector<player_commands::Action> actions;
};

/// One turn: the blocks of all the players, in the order that the server got them
struct TurnBundle
{
	uint32_t turn {0};
	std::vector<StampedBlock> blocks;
};

/// The checksum of the synced world after a turn
struct TurnChecksum
{
	uint32_t turn {0};
	uint64_t checksum {0};
};

/// A ping. The time is in microseconds of the clock of the sender, which only the sender reads.
struct Ping
{
	uint32_t id {0};
	uint64_t time {0};
};

/// The answer to a ping, with the id and the time of the ping
struct Pong
{
	uint32_t id {0};
	uint64_t time {0};
};

using Frame = std::variant<CommandBlock, TurnBundle, TurnChecksum, Ping, Pong>;

enum class DecodeError : uint8_t
{
	None,
	/// The frame has no bytes
	Empty,
	/// The frame has a version that this build does not know
	Version,
	/// The frame has a kind that this build does not know
	Kind,
	/// The frame stops before its end
	Truncated,
	/// The frame has more bytes after its end
	TrailingBytes,
	/// A command has a tag that this build does not know
	Command,
	/// A field has a value out of its range: a key, an order, a player or a count
	Value,
	/// The frame is larger than the largest frame
	TooLarge,
};

struct DecodeResult
{
	DecodeError error {DecodeError::None};
	/// The frame, when there is no error
	std::optional<Frame> frame;
};

/// The tag of a command
[[nodiscard]] CommandTag TagOf(const player_commands::Action& action) noexcept;
/// The size of a command on the wire, with its tag
[[nodiscard]] size_t SizeOf(const player_commands::Action& action) noexcept;
/// The size of a frame on the wire
[[nodiscard]] size_t SizeOf(const Frame& frame) noexcept;

/// Adds the bytes of a frame to the end of a buffer. Returns false, and adds nothing, when the frame breaks a limit: too
/// many commands in a block, too many blocks, a sender that is not a player, or a frame that is too large.
[[nodiscard]] bool Append(const Frame& frame, std::vector<uint8_t>& out);
/// The bytes of a frame. It is empty when the frame breaks a limit.
[[nodiscard]] std::vector<uint8_t> Encode(const Frame& frame);
/// Reads a frame. It accepts only a whole frame of this version with all values in their ranges.
[[nodiscard]] DecodeResult Decode(std::span<const uint8_t> bytes);

} // namespace openblack::turn_codec
