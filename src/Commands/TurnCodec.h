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
/// - Turn (server to clients): turn (u32), block count (u16), then each block: sender (u8), observer (u8), sequence
///   (u16), count (u8), then the commands. The sender is the god slot of the player (player_slots::SlotOfPlayer). The
///   observer is the number of the observer that sent the block, from 1, and 0 for the player of a god.
/// - Checksum (both ways): turn (u32), checksum (u64). The server sends its checksum of each turn, and each client sends
///   its own.
/// - Ping and pong: id (u32), time (u64). A pong sends back the id and the time of its ping.
/// - World part (server to one client): turn (u32), part (u16), parts (u16), then the bytes of the part to the end of
///   the frame. The parts of a turn, in order, are the server's world state at the start of that turn.
/// - Command: tag (u8), then the fields of the command in the order of its structure. A thing is its entity number
///   (u32). A place or a velocity is three floats. A let-go has the velocity, then the spin, the origin and the
///   angles of the start. A seed or a leash is its number (u8).
/// - A line of the chat: the count of the players that get it (u8), the god slot of each (u8), then the count of its
///   characters (u16) and each character as UTF-16 (u16). A taunt has its number (u32) in place of the characters.
/// - An observer that comes in: the count of the characters of its name (u8), then each character as UTF-16 (u16).
/// - An edit of the editor: the kind (u8), the thing, the kind of the item (u8), the type of the item (u32), the
///   challenge (u32), the position, the angle (f32), the scale (f32) and the request (u16). A pose then has the nine
///   numbers of the turn by columns (f32), 1 when the thing keeps its angles or 0 (u8), the kept angles and the scale of
///   each axis.
/// - A debug cheat: the cheat (u8), then 1 for on or 0 for off (u8).
/// - An edit of a creature by the editor: the creature, the value (u8) and the amount (f32).
/// - A skip to a land of the story: the land (u8), 2 to 5.
/// - An edit of a villager by the editor: the villager, the field (u8), the slot of states (u8), the number (u32) and the
///   amount (f32).
/// - A new creature of the editor: the species (u8), the owner (u8), the position, the angle, the size, the alignment,
///   the fatness and the strength (f32), 1 when it has tattoos or 0 (u8), each tattoo as a tattoo command has it, then
///   the request (u16).
/// - A stroke of the land brush of the editor: the mode (u8), the inner and the outer radius (f32), the step (u8), the
///   country (u8), then the x and the z of the middle (f32).
/// - A path of the editor: the kind (u8), the count of the points (u16), the x and the z of each point (f32), then the
///   first and the last building.
/// - A thing that the editor makes again: the request (u16), the kind of the item (u8), the type of the item (u32), the
///   challenge (u32), the position, the nine numbers of the turn by columns (f32), 1 when the thing keeps its angles or 0
///   (u8), the kept angles, the scale of each axis, 1 for a creature or 0 (u8), the owner (u8), the mind (u32), then the
///   size, the alignment, the fatness and the strength (f32).
/// - A mind file of the editor: the use (u8), the hash of the file (u64), then for a load the creature, or for a new
///   creature each field as a new creature of the editor has it.
/// - An act of the Scripts tab of the editor: the act (u8), the id (u32), the address (u32), the index (u16), the bits of
///   the value (u32), the count of the parameters (u8), then each parameter (f32).
/// - An act of the Land script tab of the editor: the act (u8), the command (u32), then the hash of the file (u64).
/// - A change of the program of the editor: the use (u8), the hash of the file (u64), then the script (u32).
/// - An act of the Advisors debug window: the act (u8), the advisor (u8), the place on the screen (two f32), the
///   position, 1 when the advisor points in the world or 0 (u8), the animation (u8), the speed (f32), then the emotion
///   (u8).
/// - A change of the debug menus of the world: the act (u8), the player (u8), the index (u8), then the value (f32).
/// - A line of the debug Console: the count of its bytes (u16), then each byte.
namespace openblack::turn_codec
{

/// The version of the format. A change of the format gives a new version.
inline constexpr uint8_t k_Version = 43;
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
	WorldPart = 6,
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
	PlayerLeft = 17,
	FightGround = 18,
	FightMove = 19,
	FightCharge = 20,
	CreatureReaction = 21,
	HandClick = 22,
	SeedGesture = 23,
	HandPick = 24,
	PlayerJoined = 25,
	MiracleRequest = 26,
	MiracleRepeat = 27,
	HandCast = 28,
	HandCastUnlock = 29,
	LeashPick = 30,
	HandShakeOff = 31,
	GripDust = 32,
	HelpReport = 33,
	HelpQuery = 34,
	CreatureFightCheat = 35,
	ViewReport = 36,
	TempleLeashOff = 37,
	GiveCallOff = 38,
	CreatureTattoo = 39,
	LeashLength = 40,
	ChatSpeech = 41,
	ChatTaunt = 42,
	QuickSave = 43,
	QuickLoad = 44,
	MusicMood = 45,
	IconChargeStop = 46,
	DialogueClick = 47,
	PuzzleTap = 48,
	FishScare = 49,
	ObserversJoined = 50,
	ObserversLeft = 51,
	ObserverIn = 52,
	ObserverOut = 53,
	HandLockRelease = 54,
	EditorEdit = 55,
	DebugCheat = 56,
	EditorCreatureEdit = 57,
	EditorCreatureAct = 58,
	DebugLandSkip = 59,
	EditorVillagerEdit = 60,
	EditorCreatureSpawn = 61,
	EditorLandStroke = 62,
	EditorPath = 63,
	EditorRemake = 65,
	EditorMindLoad = 67,
	EditorScript = 70,
	EditorLandScript = 71,
	EditorScriptProgram = 72,
	DebugAdvisor = 76,
	DebugWorld = 77,
	DebugConsoleLine = 78,
};

/// The sizes of the parts of the frames, in bytes
namespace sizes
{
inline constexpr size_t k_Header = 2;
inline constexpr size_t k_Entity = 4;
inline constexpr size_t k_Vector = 12;
/// The fields of a new creature of the editor: the species, the owner, the place, five numbers, the tattoo flag and the
/// tattoos
inline constexpr size_t k_CreatureSpawn = 1 + 1 + k_Vector + (5 * 4) + 1 + (5 * creature_tattoo::k_SlotCount);
/// The sequence, the turn and the count of a block that a client sends
inline constexpr size_t k_CommandsBody = 7;
/// The turn and the block count of a turn
inline constexpr size_t k_TurnBody = 6;
/// The sender, the observer, the sequence and the count of a block in a turn
inline constexpr size_t k_Stamp = 5;
inline constexpr size_t k_Checksum = k_Header + 12;
inline constexpr size_t k_Ping = k_Header + 12;
/// The turn, the part and the count of the parts of a world part, before its bytes
inline constexpr size_t k_WorldPartHead = k_Header + 8;
} // namespace sizes

/// The most characters in a line of the chat. The game keeps this many characters of a line that comes in.
inline constexpr size_t k_MostChatCharacters = 1023;

/// The most characters of the name of an observer that comes in
inline constexpr size_t k_MostObserverNameCharacters = 64;

/// The highest number of an observer. The numbers of the observers that are in the game are all different.
inline constexpr uint8_t k_MostObserverNumber = 127;

/// The most bytes of the world state in one world part
inline constexpr size_t k_WorldPartBytes = 60 * 1024;

/// The commands of one client, as it sends them to the server
struct CommandBlock
{
	/// The number of the block. Each session of a client counts its blocks from 0 through all its sockets, and the count
	/// wraps. Thus the server knows a block that a client sends again after it came back.
	uint16_t sequence {0};
	/// The last turn that the client did when it made the block
	uint32_t turn {0};
	std::vector<player_commands::Action> actions;
};

/// True when the number of a block comes after the other number. The count wraps, so a number is after the numbers in the
/// half of the count before it.
[[nodiscard]] constexpr bool SequenceAfter(uint16_t sequence, uint16_t other) noexcept
{
	const auto distance = static_cast<uint16_t>(sequence - other);
	return distance != 0 && distance < 0x8000;
}

/// A block in a turn, with the stamp of the server: the player that sent it, the observer that sent it and the number of
/// the block
struct StampedBlock
{
	PlayerNames sender {PlayerNames::PLAYER_ONE};
	uint16_t sequence {0};
	std::vector<player_commands::Action> actions;
	/// The number of the observer of the god of the observers that sent the block, from 1. 0 for the player of a god.
	uint8_t observer {0};
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

/// A part of the server's world state at the start of a turn, for a client that comes back
struct WorldPart
{
	uint32_t turn {0};
	uint16_t part {0};
	uint16_t parts {0};
	std::vector<uint8_t> bytes;
};

using Frame = std::variant<CommandBlock, TurnBundle, TurnChecksum, Ping, Pong, WorldPart>;

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
/// The size of a block in a turn on the wire: its stamp and its commands
[[nodiscard]] size_t BlockSize(const StampedBlock& block) noexcept;

/// Adds the bytes of a frame to the end of a buffer. Returns false, and adds nothing, when the frame breaks a limit: too
/// many commands in a block, too many blocks, a sender that is not a player, or a frame that is too large.
[[nodiscard]] bool Append(const Frame& frame, std::vector<uint8_t>& out);
/// The bytes of a frame. It is empty when the frame breaks a limit.
[[nodiscard]] std::vector<uint8_t> Encode(const Frame& frame);
/// The world parts of a world state at the start of a turn: at least one, each with at most k_WorldPartBytes
[[nodiscard]] std::vector<WorldPart> SplitWorld(uint32_t turn, std::span<const uint8_t> state);
/// Reads a frame. It accepts only a whole frame of this version with all values in their ranges.
[[nodiscard]] DecodeResult Decode(std::span<const uint8_t> bytes);

} // namespace openblack::turn_codec
