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

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

/// The JSON text frames of the game socket between a client and a game server, after the socket opens and next to the
/// binary frames of the turns (see the turn codec of the game). Each frame is an object with a "type" member. Nothing
/// here throws. A parse gives none when the text is not a frame that it knows.
namespace openblack::game_socket
{

/// What a ticket lets the account do in the room
enum class Role : uint8_t
{
	/// The account takes a god slot and plays
	God,
	/// The account only watches. It has no god slot and sends no commands.
	Observer,
};

/// Client to server: "room.join". The first frame on a new socket.
struct Join
{
	/// The lobby's join ticket (a PASETO v4.public token)
	std::string ticket;
	/// The game's password, when the game needs one
	std::string gamePassword;
	/// The name of the player's profile, on the local network only, where no ticket names the player
	std::string name;
	/// The build id of the client, such as "0.1.0+0123456789ab/codec34" ("build", optional). A server of another build
	/// refuses the client with "version".
	std::string build;
	/// The SHA-256 of the client's file of the land of the game, as 64 hex digits ("map", optional). A server with another
	/// file refuses the client with "map".
	std::string map;
};

/// Client to server: "room.resume". A client that lost its socket opens a new one and sends this in place of a join. It
/// keeps its world, so the server sends the turns after the last turn that the client did.
struct Resume
{
	/// The secret of the session that the server gave in "room.joined"
	std::string session;
	/// The last turn that the client did
	uint32_t turn {0};
	/// The world of the client parted from the server's: the server sends its world state first ("world", optional)
	bool world {false};
};

/// Client to server: "room.ready". The client loaded the land and waits for the start of the game.
struct Ready
{
	bool ready {true};
};

/// Client to server: "room.leave". The player leaves the game. The server then defeats the god.
struct Leave
{
};

/// A winning condition that the host of a network game picked: the number of the condition (0 to 14, in the order of
/// the game) and the value that a god must get to
struct ConditionValue
{
	uint8_t condition {0};
	uint32_t value {0};

	[[nodiscard]] bool operator==(const ConditionValue&) const = default;
};

/// Server to client: "game.start". The world starts to turn from this turn.
/// A human god of the game, for the names in the chat
struct God
{
	uint8_t slot {0};
	/// The name of the account
	std::string name;
	/// The id of the lobby account of the player ("account", optional). It is the lasting number of the player, for
	/// example in the friend list. None on the local network, where no ticket names an account.
	std::optional<int64_t> account;
	/// The join ticket of the player has the editor claim ("editor", optional). Only such a player opens the editor and
	/// turns on the debug cheats in a network game.
	bool editor {false};
	bool operator==(const God&) const = default;
};

struct Start
{
	/// The script of the land, as a path under the Scripts folder of the game (for example "Land1.txt" or
	/// "Playgrounds/construct.txt"). None for the testbed and the network map for 32 gods.
	std::string land;
	bool testbed {false};
	uint32_t seed {0};
	/// The turn that the world is at when the game starts
	uint32_t turn {0};
	/// How the new game on the land starts, as the game's --skip-opening gives it (for example "cow"). Empty for the
	/// testbed.
	std::string opening;
	/// The network map for 32 gods in place of a land ("gods_ring_map", optional)
	bool godsRingMap {false};
	/// Tools: every god with a citadel is a computer god, and the room has no human god ("all_computer_gods", optional).
	/// Only a developer build plays such a game.
	bool allComputerGods {false};
	/// Tools: the seed of the personalities of the computer gods ("ai_seed", optional). None: the built-in personalities.
	std::optional<uint32_t> aiSeed;
	/// Tools: the id of the testbed scenario that every computer plays from the first turn ("scenario", optional). Only a
	/// developer build plays such a game.
	std::optional<std::string> scenario;
	/// The time limit of the game in minutes ("time_limit", optional). Every computer decides the game at the same turn
	/// when the time is up. None: the default time limit of the game.
	std::optional<uint32_t> timeLimit;
	/// The id of the game in the lobby ("game_id", optional). The desync events of the server and of the clients of one
	/// game carry this id. Empty: the server has no lobby id, and each client makes its own id.
	std::string gameId;
	/// The winning conditions of the game ("conditions", optional). Each god gets each condition at the start. Empty: the
	/// game has no winning conditions.
	std::vector<ConditionValue> conditions;
	/// The names of the players of the gods, by god slot ("names", optional). A slot with no player has an empty name.
	std::vector<std::string> names;
	/// The human gods of the game, by slot, as the game starts or as an observer joins ("gods", optional)
	std::vector<God> gods;
	/// The god slots that the lobby closed, from 0 ("closed_slots", optional). No human and no computer god plays such a
	/// slot. Each other slot with no human gets a computer god at the start.
	std::vector<uint8_t> closedSlots;
};

/// Server to client: "room.joined". The server accepted the ticket.
struct Joined
{
	Role role {Role::God};
	/// The god slot of the player, from 0. None for an observer.
	std::optional<uint8_t> slot;
	/// The secret that the client sends in a resume
	std::string session;
	/// The name of the account
	std::string name;
	/// The land, the seed and the start of the server's world ("start", optional). A client that joins from the command
	/// line loads this land with this seed before it is ready, so the game starts from the same world on each computer.
	std::optional<Start> start;
	/// The number of an observer in the game, from 1 ("observer", optional). The turns stamp the commands of the
	/// observer with it. 0 for a god, and for an observer of an older server.
	uint8_t observer {0};
	/// The join ticket of the player has the editor claim ("editor", optional). Then the player can open the editor and
	/// turn on the debug cheats. The server refuses the edits and the cheats of every other player.
	bool editor {false};
	/// The join ticket of the player has the debugger claim ("debugger", optional). Only then the client shows its
	/// debug menu bar. An older server does not send it, and then the client does not show the bar.
	bool debugger {false};
	/// The files of the game's folder that the server's world read, relative to the game's folder ("data_files",
	/// optional). The client sends the print of these files from its own folder (data.print). An older server sends
	/// none, and then the client sends no print.
	std::vector<std::string> dataFiles;
	/// The server keeps the facts of a desync and compares the worlds entity by entity ("capture", optional). Only then
	/// the client sends its facts, its blocks, its entities and its world after a desync.
	bool capture {false};
};

/// Server to client: "room.refused". The server closes the socket after this frame.
struct Refused
{
	/// A short word for the reason: "ticket", "full", "started", "closed", "protocol", "resync", "kicked", "version" (the
	/// client has another build than the server, or sent a binary frame of another version of the format of the turns),
	/// "map" (the client has another file of the land), "load" (the client did not load the world of a lobby game in time),
	/// "data" (a file of the game data of the client is not the server's)
	std::string reason;
	/// The build id of the server ("build", optional), with the refusals "protocol" and "version". The client log names
	/// it beside the build id of the client. An older client does not read it.
	std::string build;
	/// The refusal "data": the first file of the game data of the client that is not the server's, in words ("file",
	/// optional)
	std::string file;
	/// The refusal "data": the path of that file in the game folder ("path", optional). The box of the client names it.
	std::string path;
};

/// Server to client: "room.resumed". The server accepted the resume. The turns after the client's turn follow.
struct Resumed
{
	/// The last turn that the server did
	uint32_t turn {0};
	/// The server's world state at the start of that turn follows as world parts, before the turns ("world", optional)
	bool world {false};
};

/// Server to client: "player.state". A god's player lost the socket, came back, or left the game.
struct PlayerState
{
	enum class State : uint8_t
	{
		/// The socket closed. The god stays idle until the player comes back or the time to come back ends.
		Away,
		Back,
		/// The god is defeated and its slot is closed
		Left,
	};
	uint8_t slot {0};
	State state {State::Away};
};

/// The start of the game has steps. When the gods are ready, the server asks each player in the room if it has the
/// land (start.query). Each player replies with the checksum of its land file (start.reply). When a player does not
/// have the land, the server stops the start (start.cancel). When a checksum is not the server's, the server also
/// removes that player from the room (room.refused "kicked"). When all players have the land, the server asks the gods
/// for their creatures (start.transfer). Each god sends its creature file (start.creature), and the server sends each
/// creature file to all players. When a player leaves during these steps, the server stops the start (start.stop).
/// After the last creature file, the server sends game.start.

/// Server to client: "start.query". The gods are ready, and the server asks if the player has the land of the game.
struct StartQuery
{
	/// The script of the land under the Scripts folder of the game, or none for the testbed
	std::string land;
	bool testbed {false};
	/// The checksum of the server's land file
	uint64_t checksum {0};
};

/// Client to server: "start.reply"
struct StartReply
{
	/// The player has the land
	bool have {false};
	/// The checksum of the player's land file
	uint64_t checksum {0};
};

/// Server to client: "start.cancel". A player does not have the land, so the game does not start.
struct StartCancel
{
};

/// Server to client: "start.stop". A player left the room during the start, so the game does not start.
struct StartStop
{
};

/// Server to client: "start.transfer". All players have the land. Each god sends its creature file.
struct StartTransfer
{
};

/// The largest creature file that a start.creature frame holds
inline constexpr size_t k_MostCreatureFileBytes = 512 * 1024;

/// "start.creature". Client to server: the creature file of the player's god, empty when the player has no creature.
/// Server to client: the creature file of the god of a slot.
struct StartCreature
{
	/// The god's slot. None from a client.
	std::optional<uint8_t> slot;
	std::vector<uint8_t> file;
	/// The alignment that the god starts the game at: the alignment of its player's profile, from -1 (evil) to 1 (good)
	float alignment {0.0f};
};

/// The largest mind file of the creature tools of the editor that an editor.mind_file frame holds. This is a limit of the
/// network game only; a game alone opens a mind file of any size.
inline constexpr size_t k_MostEditorMindFileBytes = 512 * 1024;

/// "editor.mind_file". Client to server: a mind file that the creature tools of the editor opened on the computer of a
/// player with the editor claim. The server keeps the file, and sends it to every player in the game, also to the sender.
/// Then a turn command uses the file by the hash of its bytes (player_commands::EditorMindLoad). The server sends each
/// file that it keeps again to a player who comes back or joins, before the turns.
struct EditorMindFile
{
	std::vector<uint8_t> file;

	[[nodiscard]] bool operator==(const EditorMindFile&) const = default;
};

/// The largest file of the script tabs of the editor that the editor.script_file frames hold: a compiled script program,
/// the source of a script or a land script. The program of the game has about 3 MB. This is a limit of the network game
/// only.
inline constexpr size_t k_MostEditorScriptFileBytes = 8 * 1024 * 1024;
/// The most bytes of one editor.script_file frame. Each part but the last holds this many bytes.
inline constexpr size_t k_MostEditorScriptPartBytes = 48 * 1024;

/// "editor.script_file", both ways. One part of a file of the script tabs of the editor, from the computer of a player with
/// the editor claim. The parts of a file go in order. The server keeps the whole file and sends it in parts to every
/// player, also to the sender. Then a turn command uses the file by the hash of its bytes. The server sends each file that
/// it keeps again to a player who comes back or joins, before the turns.
struct EditorScriptFile
{
	/// What the file holds
	enum class Kind : uint8_t
	{
		/// A compiled script program (.chl)
		Program = 0,
		/// The edited source of one script, which each computer compiles at the turn
		Source = 1,
		/// The text of a land script
		LandScript = 2,
	};

	Kind kind {Kind::Program};
	/// The hash of the bytes of the whole file (editor::mind_loads::HashOf)
	uint64_t hash {0};
	/// The part, from 0, and the count of the parts
	uint32_t index {0};
	uint32_t count {0};
	/// The bytes of the whole file
	uint32_t size {0};
	/// The bytes of this part
	std::vector<uint8_t> bytes;

	[[nodiscard]] bool operator==(const EditorScriptFile&) const = default;
};

/// Client to server: "game.stop_request". The end box of a decided game opened on the client. The server stops the turns
/// of the game, if they did not stop before.
struct StopRequest
{
};

/// Server to client: "game.stop". The server stopped the turns of the game. No turn comes after this turn.
struct GameStop
{
	uint32_t turn {0};
};

/// The most characters of a line of the room's chat, as the chat box of the game takes them (UTF-16 code units)
inline constexpr size_t k_MostRoomChatCharacters = 254;

/// "room.chat", in a room on the local network before the start. Client to server: a line that the player typed. Server
/// to client: a line of another player in the room. A line with no addressee goes to every player in the room but the
/// sender, whose game shows its own line when it sends it. A line with an addressee goes only to that player.
struct RoomChat
{
	/// The name of the player who typed the line. Empty from a client.
	std::string from;
	std::string text;
	/// From a client: the name of the one player who gets the line, empty for all the players. Empty from the server.
	std::string to;
	/// From the server: the line went to this player only
	bool privateLine {false};
	/// The sender picked a team: the game sends one private line to each player of the team
	bool team {false};

	[[nodiscard]] bool operator==(const RoomChat&) const = default;
};

/// Server to client: "observer.left". The session of an observer ended after the start: the observer left, or did not
/// come back in time. The other players see that the observer left the game.
struct ObserverLeft
{
	/// The name of the observer's account
	std::string name;
};

/// Server to client: "command.refused". The server refused commands of this player, and no turn holds them. The game
/// of the player shows a line that tells why.
struct CommandRefused
{
	/// Why the server refused the commands. k_NoEditorClaim: edits of the editor or debug cheats from a player whose join
	/// ticket has no editor claim. k_NoStoryLands: a skip to a land of the story, which a network game does not have.
	/// k_NoDebuggerClaim: acts of the debug windows from a player whose join ticket has no debugger claim.
	/// k_WorldRefused: a world file or a scenario of the editor that the server cannot load.
	std::string reason;
	/// The number of commands that the server refused
	uint32_t commands {0};

	[[nodiscard]] bool operator==(const CommandRefused&) const = default;
};

/// The reason of a refusal of edits of the editor or of debug cheats (see CommandRefused)
inline constexpr std::string_view k_NoEditorClaim = "no_editor_claim";
/// The reason of a refusal of a skip to a land of the story: a network game has no lands of the story
inline constexpr std::string_view k_NoStoryLands = "no_story_lands";
/// The reason of a refusal of the acts of the debug windows from a player whose join ticket has no debugger claim
inline constexpr std::string_view k_NoDebuggerClaim = "no_debugger_claim";

/// What the editor asks of the clock of the room (see EditorTime)
enum class TimeAct : uint8_t
{
	/// The server sends no turns
	Pause,
	/// The server sends the turns again
	Play,
	/// While the clock is paused, the server sends one turn
	Step,
	/// The server changes the time between two turns
	Speed,
};

/// Client to server: "editor.time". The editor of a player with the editor claim asks the server to pause, play, step or
/// change the speed of the clock of the room. The server checks the request, logs it and tells every player (GameTime).
/// The server refuses the request with "command.refused" when the player has no editor claim.
struct EditorTime
{
	TimeAct act {TimeAct::Pause};
	/// With the act Speed: the time of a turn as a multiple of a tenth of a second, as the speed of the editor gives it.
	/// The server takes 0.1 to 10.
	float speed {1.0f};

	[[nodiscard]] bool operator==(const EditorTime&) const = default;
};

/// Server to client: "game.time". The clock of the room after a change. The server sends it to every player after each
/// request that it took, and to a player who joins or comes back. Each computer then sets its clock to this speed.
struct GameTime
{
	/// The server sends no turns, except the turns of a step
	bool paused {false};
	/// The time of a turn as a multiple of a tenth of a second (1 is normal)
	float speed {1.0f};
	/// The change that the server took. A frame to a player who joins or comes back has the act Play.
	TimeAct act {TimeAct::Play};
	/// The name of the player who asked for the change. Empty when the server made the change, or for a frame to a
	/// player who joins or comes back.
	std::string by;
	/// The number of turns that the server sent when it took the change
	uint32_t turn {0};

	[[nodiscard]] bool operator==(const GameTime&) const = default;
};

/// The reason of a refusal of a request of the clock with a value that the server does not take
inline constexpr std::string_view k_TimeRefused = "time_refused";

/// Client to server: "game.loaded". The player's computer has the land and loaded the world of the started game. The
/// client sends it once after game.start, and again after it comes back if the server did not get it.
struct Loaded
{
};

/// Server to client: "game.wait". In a lobby game, the turns of the game do not start until each god's player loaded the
/// world (deviation 254). The server sends this frame each time the list of these players changes. The players who
/// loaded can move the camera, but the world does not change. When the list is empty, the pause ended and the turns
/// come.
struct LoadWait
{
	/// The names of the gods' players who did not load the world yet
	std::vector<std::string> loading;
	/// The seconds until the server stops the wait, when the frame went. Then each player who did not load leaves the
	/// game. Each client counts down from this value. 0 in the frame that ends the wait.
	uint32_t secondsLeft {0};

	[[nodiscard]] bool operator==(const LoadWait&) const = default;
};

/// The most characters of the game id in a "game.start" frame
inline constexpr size_t k_MostGameIdCharacters = 64;
/// The most parts in a "sync.parts" frame
inline constexpr size_t k_MostSyncParts = 4096;
/// The most bytes of the name of a part in a "sync.parts" frame
inline constexpr size_t k_MostSyncPartNameBytes = 256;

/// Both ways: "sync.parts". The hash of each part of the synced world of one computer at a turn at which the worlds
/// parted. A client sends its parts after it comes back from the part. The server writes the parts that differ to its log
/// and sends its own parts back, and the client writes the parts that differ to its log. Only the logs use the frame.
struct SyncParts
{
	uint32_t turn {0};
	/// The name and the hash of each part
	std::vector<std::pair<std::string, uint64_t>> parts;

	[[nodiscard]] bool operator==(const SyncParts&) const = default;
};

/// The most files in a "data.print" frame and in the files of "room.joined"
inline constexpr size_t k_MostDataFiles = 512;
/// The most bytes of the path of a file in these frames
inline constexpr size_t k_MostDataPathBytes = 260;
/// The most bytes of a short text of the facts, such as a build id or a land
inline constexpr size_t k_MostFactBytes = 256;

/// The print of one file of the game data
struct DataFile
{
	/// The path relative to the game's folder, with "/" between the parts
	std::string path;
	/// False when the client does not have the file
	bool present {false};
	uint64_t size {0};
	uint64_t hash {0};

	[[nodiscard]] bool operator==(const DataFile&) const = default;
};

/// Client to server: "data.print". The print of each file of the server's list (room.joined), from the client's own game
/// folder. The server refuses a client whose print differs with "data", and names the first file that differs.
struct DataPrint
{
	std::vector<DataFile> files;
	/// What the client's game folder tells of its copy: the last patch, such as "1.20", and the language, as the 4
	/// hexadecimal digits of the Windows locale. Empty when the folder does not tell.
	std::string patch;
	std::string language;

	[[nodiscard]] bool operator==(const DataPrint&) const = default;
};

/// Client to server: "sync.facts". After its world parts from the server's world, the client sends the facts that decide
/// whether the two worlds can be the same. The server compares them with its own facts and names the first that
/// differs.
struct SyncFacts
{
	/// The turn at which the worlds parted
	uint32_t turn {0};
	/// The build id of the client
	std::string build;
	/// The land that the client loaded, and the SHA-256 of its file (64 hexadecimal digits, or empty)
	std::string land;
	std::string landSha256;
	/// The SHA-256 of the land file that the client got from the lobby ("map" of the join), or empty
	std::string map;
	/// The seed of the client's world
	uint32_t seed {0};
	/// One hash of the client's print of the game data (data.print)
	uint64_t data {0};
	std::string patch;
	std::string language;

	[[nodiscard]] bool operator==(const SyncFacts&) const = default;
};

/// The most parts in one capture
inline constexpr size_t k_MostCaptureParts = 8;

/// Server to client: "sync.capture". At the end of this turn, the client hashes each entity of these parts, and sends
/// the blocks (sync.blocks) and its world (sync.world). The client rejoins after the capture.
struct SyncCapture
{
	uint32_t turn {0};
	std::vector<std::string> parts;

	[[nodiscard]] bool operator==(const SyncCapture&) const = default;
};

/// The most blocks of one part in a "sync.blocks" frame
inline constexpr size_t k_MostSyncBlocks = 1024;

/// The hash of each block of the entities of one part
struct PartBlocks
{
	std::string part;
	/// The count of the entities of the part on the computer
	uint32_t entities {0};
	std::vector<uint64_t> blocks;

	[[nodiscard]] bool operator==(const PartBlocks&) const = default;
};

/// Client to server: "sync.blocks". The blocks of each part of the capture at its turn.
struct SyncBlocks
{
	uint32_t turn {0};
	std::vector<PartBlocks> parts;

	[[nodiscard]] bool operator==(const SyncBlocks&) const = default;
};

/// The blocks of one part that the server asks for
struct PartAsk
{
	std::string part;
	std::vector<uint32_t> blocks;

	[[nodiscard]] bool operator==(const PartAsk&) const = default;
};

/// Server to client: "sync.ask". The first blocks that differ from the server's. The client sends the entities of these
/// blocks (sync.entities).
struct SyncAsk
{
	uint32_t turn {0};
	std::vector<PartAsk> parts;

	[[nodiscard]] bool operator==(const SyncAsk&) const = default;
};

/// The most entities of one part in a "sync.entities" frame
inline constexpr size_t k_MostSyncEntities = 1024;

/// The hash of one entity of a part, with what it is and where it is
struct SyncEntity
{
	uint32_t entity {0};
	uint64_t hash {0};
	std::string kind;
	std::optional<std::array<float, 3>> position;

	[[nodiscard]] bool operator==(const SyncEntity&) const = default;
};

struct PartEntityHashes
{
	std::string part;
	std::vector<SyncEntity> entities;

	[[nodiscard]] bool operator==(const PartEntityHashes&) const = default;
};

/// Client to server: "sync.entities". The entities of the blocks that the server asked for.
struct SyncEntities
{
	uint32_t turn {0};
	std::vector<PartEntityHashes> parts;

	[[nodiscard]] bool operator==(const SyncEntities&) const = default;
};

/// The most bytes of the world in one "sync.world" frame, before Base64
inline constexpr size_t k_MostSyncWorldChunkBytes = 48 * 1024;
/// The most bytes of the world that a client sends in all
inline constexpr size_t k_MostSyncWorldBytes = 16 * 1024 * 1024;

/// Client to server: "sync.world". One part of the client's saved world at the turn of the capture, for the server's
/// record of the desync. The parts go in order.
struct SyncWorld
{
	uint32_t turn {0};
	/// The number of this part from 0, and the count of the parts
	uint32_t index {0};
	uint32_t count {0};
	/// The bytes of the whole world
	uint32_t size {0};
	std::vector<uint8_t> bytes;

	[[nodiscard]] bool operator==(const SyncWorld&) const = default;
};

/// The most bytes of a world file that a player with the editor claim loads for the whole room, in all. It is the limit
/// of the world of a capture.
inline constexpr size_t k_MostEditorWorldBytes = k_MostSyncWorldBytes;
/// The most bytes of a world file in one "editor.world" frame, before Base64
inline constexpr size_t k_MostEditorWorldChunkBytes = k_MostSyncWorldChunkBytes;

/// "editor.world". Client to server: one part of a world file that Load world file of the editor opened on the computer
/// of a player with the editor claim. The parts go in order. When the last part came, the world of the server loads the
/// file between two turns, and the server sends its new world to every player, as to a player who comes back.
struct EditorWorld
{
	/// The number of this part from 0, and the count of the parts
	uint32_t index {0};
	uint32_t count {0};
	/// The bytes of the whole file
	uint32_t size {0};
	std::vector<uint8_t> bytes;

	[[nodiscard]] bool operator==(const EditorWorld&) const = default;
};

/// The most bytes of the id of a testbed scenario in an "editor.scenario" frame
inline constexpr size_t k_MostScenarioIdBytes = 128;

/// "editor.scenario". Client to server: a player with the editor claim runs a testbed scenario for the whole room. The
/// server ends the current game, and starts the room again on the testbed with the scenario: every player gets a new
/// "game.start" that names the scenario.
struct EditorScenario
{
	std::string id;

	[[nodiscard]] bool operator==(const EditorScenario&) const = default;
};

/// The reason of a refusal of a world file or a scenario of the editor that no computer can load (see CommandRefused)
inline constexpr std::string_view k_WorldRefused = "world_refused";

using Frame = std::variant<Join, Resume, Ready, Leave, Joined, Refused, Start, Resumed, PlayerState, StartQuery, StartReply,
                           StartCancel, StartStop, StartTransfer, StartCreature, StopRequest, GameStop, RoomChat, ObserverLeft,
                           SyncParts, Loaded, LoadWait, DataPrint, SyncFacts, SyncCapture, SyncBlocks, SyncAsk, SyncEntities,
                           SyncWorld, CommandRefused, EditorMindFile, EditorTime, GameTime, EditorScriptFile, EditorWorld,
                           EditorScenario>;

[[nodiscard]] std::string ToJson(const Frame& frame);
[[nodiscard]] std::optional<Frame> Parse(std::string_view text);

[[nodiscard]] std::string_view RoleName(Role role) noexcept;
[[nodiscard]] std::optional<Role> RoleFrom(std::string_view name) noexcept;

/// The name of an act of the clock in the frames: "pause", "play", "step" or "speed"
[[nodiscard]] std::string_view TimeActName(TimeAct act) noexcept;
[[nodiscard]] std::optional<TimeAct> TimeActFrom(std::string_view name) noexcept;

} // namespace openblack::game_socket
