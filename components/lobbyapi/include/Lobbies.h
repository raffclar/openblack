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

#include "LobbyApi.h"

/// The shapes of the lobby rooms of bwgame-service: the calls under /api/v1/lobbies and the events of the push channels
/// "lobbies" and "lobby-<id>". A lobby is a record of the service. A game server starts only when the host starts the
/// game.
namespace openblack::lobby_api
{

/// The state of a lobby in the service
enum class LobbyState : uint8_t
{
	/// The players join, pick slots and get ready
	Open,
	/// The host started the game, and the game starts after 5 seconds
	Countdown,
	/// Each game server is in use. The lobby waits in a line for a free server.
	Queued,
	/// The service gave a game server to the lobby. The players get tickets and join it.
	Assigned,
	InGame,
	/// The game of the lobby ended
	Ended,
	Closed,
};

/// What an empty god slot holds
enum class SlotKind : uint8_t
{
	/// A player can take the slot
	Open,
	/// A computer god plays the slot at the start
	Computer,
	/// Nobody plays the slot
	Closed,
};

enum class LobbyVisibility : uint8_t
{
	/// The lobby list shows the lobby
	Public,
	/// Only the code of the lobby finds it
	Private,
};

enum class MemberRole : uint8_t
{
	/// The member plays a god
	God,
	/// The member only watches
	Observer,
};

[[nodiscard]] std::string_view LobbyStateName(LobbyState state);
[[nodiscard]] std::optional<LobbyState> LobbyStateOf(std::string_view name);
[[nodiscard]] std::string_view SlotKindName(SlotKind kind);
[[nodiscard]] std::optional<SlotKind> SlotKindOf(std::string_view name);
[[nodiscard]] std::string_view VisibilityName(LobbyVisibility visibility);
[[nodiscard]] std::optional<LobbyVisibility> VisibilityOf(std::string_view name);
[[nodiscard]] std::string_view MemberRoleName(MemberRole role);
[[nodiscard]] std::optional<MemberRole> MemberRoleOf(std::string_view name);

/// The most characters of the name of a lobby
constexpr size_t k_MostLobbyNameCharacters = 32;
/// The most characters of the password of a lobby
constexpr size_t k_MostLobbyPasswordCharacters = 32;
/// The god slots of a lobby: 2 to 32, and at most the citadels of the map
constexpr int32_t k_LeastGodSlots = 2;
constexpr int32_t k_MostGodSlots = 32;
/// The time limit of a game: 10 minutes to 6 hours. The default is 30 minutes.
constexpr int32_t k_LeastTimeLimitMinutes = 10;
constexpr int32_t k_MostTimeLimitMinutes = 360;
constexpr int32_t k_DefaultTimeLimitMinutes = 30;
/// The number of the conditions of a game
constexpr size_t k_ConditionCount = 15;
/// The characters of the code of a private lobby, with no dash
constexpr size_t k_LobbyCodeCharacters = 8;

/// The map of a lobby
struct LobbyMap
{
	/// "file", for example "Online Maps/mpm_2p_1.map"
	std::string file;
	/// "name"
	std::string name;
	/// "sha256": the hash of the file in lower-case hexadecimal
	std::string sha256;
	/// "size": the bytes of the file
	int64_t size {0};
};

/// The settings of a game that the host sets
struct LobbySettings
{
	/// "time_limit_minutes"
	int32_t timeLimitMinutes {k_DefaultTimeLimitMinutes};
	/// "conditions": the values of the conditions of the game, at most 15
	std::vector<int32_t> conditions;
	/// "observers_allowed"
	bool observersAllowed {true};
};

/// The game servers of the service: "pool" of the list, and the data of "pool.changed"
struct PoolState
{
	/// "free"
	int32_t free {0};
	/// "total"
	int32_t total {0};
};

/// A lobby in the lobby list
struct LobbySummary
{
	/// "id"
	std::string id;
	/// "name"
	std::string name;
	/// "host"
	AccountRef host;
	/// "map"
	LobbyMap map;
	/// "gods": the players in god slots
	int32_t gods {0};
	/// "god_slots"
	int32_t godSlots {0};
	/// "observers"
	int32_t observers {0};
	/// "locked": the lobby needs a password
	bool locked {false};
	/// "state"
	LobbyState state {LobbyState::Open};
	/// "queue_position": the place of the lobby in the line for a free server
	std::optional<int32_t> queuePosition;
	/// "started_at": milliseconds since 1970 (UTC)
	std::optional<int64_t> startedAt;
	/// "created": milliseconds since 1970 (UTC)
	std::optional<int64_t> created;
	/// "build_id": the build of openblack that the lobby needs
	std::string buildId;
};

/// GET /api/v1/lobbies
struct LobbyList
{
	/// "lobbies"
	std::vector<LobbySummary> lobbies;
	/// "pool", when the service gives it
	std::optional<PoolState> pool;
};

/// A member of a lobby
struct LobbyMember
{
	/// "account"
	AccountRef account;
	/// "role"
	MemberRole role {MemberRole::God};
	/// "slot": the god slot from 0, none for an observer or a god with no slot
	std::optional<int32_t> slot;
	/// "ready"
	bool ready {false};
	/// "joined_at": milliseconds since 1970 (UTC)
	std::optional<int64_t> joinedAt;
	/// "away": the push channel of the member is closed
	bool away {false};
};

/// GET /api/v1/lobbies/{id}: the full lobby
struct Lobby
{
	/// "id"
	std::string id;
	/// "code": the 8 characters that find the lobby
	std::string code;
	/// "name"
	std::string name;
	/// "host"
	AccountRef host;
	/// "visibility"
	LobbyVisibility visibility {LobbyVisibility::Public};
	/// "locked": the lobby needs a password. The service never sends the password.
	bool locked {false};
	/// "map"
	LobbyMap map;
	/// "god_slots"
	int32_t godSlots {0};
	/// "slot_kinds": what each god slot holds when no player takes it
	std::vector<SlotKind> slotKinds;
	/// "settings"
	LobbySettings settings;
	/// "build_id"
	std::string buildId;
	/// "state"
	LobbyState state {LobbyState::Open};
	/// "queue_position"
	std::optional<int32_t> queuePosition;
	/// "countdown_at": the time of the start in milliseconds since 1970 (UTC), during the countdown
	std::optional<int64_t> countdownAt;
	/// "game_id": the game server of the lobby, after the service gave one
	std::string gameId;
	/// "members"
	std::vector<LobbyMember> members;
};

/// POST /api/v1/lobbies
struct LobbyCreateRequest
{
	/// "name"
	std::string name;
	/// "map": {"file", "sha256"}
	LobbyMap map;
	/// "god_slots"
	int32_t godSlots {k_LeastGodSlots};
	/// "slot_kinds"
	std::vector<SlotKind> slotKinds;
	/// "visibility"
	LobbyVisibility visibility {LobbyVisibility::Public};
	/// "password": none or empty for a lobby with no password
	std::optional<std::string> password;
	/// "settings"
	LobbySettings settings;
	/// "build_id"
	std::string buildId;
};

/// PATCH /api/v1/lobbies/{id}: the fields that the host changes. A change clears the ready of each member.
struct LobbyChangeRequest
{
	std::optional<std::string> name;
	std::optional<LobbyMap> map;
	std::optional<std::vector<SlotKind>> slotKinds;
	std::optional<LobbyVisibility> visibility;
	/// An empty password removes the password
	std::optional<std::string> password;
	std::optional<LobbySettings> settings;
};

/// POST /api/v1/lobbies/{id}/members
struct LobbyJoinRequest
{
	/// "password"
	std::optional<std::string> password;
	/// "build_id"
	std::string buildId;
	/// "role"
	MemberRole role {MemberRole::God};
	/// "slot"
	std::optional<int32_t> slot;
};

/// PUT /api/v1/lobbies/{id}/members/me
struct MemberChangeRequest
{
	/// "ready"
	std::optional<bool> ready;
	/// "slot"
	std::optional<int32_t> slot;
};

/// POST /api/v1/lobbies/{id}/host
struct HostRequest
{
	/// "account": the id of the account
	int64_t account {0};
};

/// POST /api/v1/lobbies/quick-join
struct QuickJoinRequest
{
	/// "build_id"
	std::string buildId;
};

/// The answer of POST /api/v1/lobbies/{id}/start (202)
struct StartReply
{
	/// "state"
	LobbyState state {LobbyState::Countdown};
	/// "at": the time of the start in milliseconds since 1970 (UTC)
	std::optional<int64_t> at;
	/// "position": the place in the line, when the state is Queued
	std::optional<int32_t> position;
};

/// The answer of GET /api/v1/lobbies/by-code/{code}
struct LobbyCodeReply
{
	/// "id"
	std::string id;
};

/// The names of the push channels of the lobbies
constexpr std::string_view k_LobbiesChannel = "lobbies";
/// The channel of one lobby is this text and the id of the lobby
constexpr std::string_view k_LobbyChannelPrefix = "lobby-";

/// The names of the events of the lobbies
namespace lobby_events
{
// The channel "lobbies"
constexpr std::string_view k_LobbiesChanged = "lobbies.changed";
constexpr std::string_view k_LobbiesRemoved = "lobbies.removed";
constexpr std::string_view k_PoolChanged = "pool.changed";
// The channel "lobby-<id>"
constexpr std::string_view k_MemberJoined = "member.joined";
constexpr std::string_view k_MemberLeft = "member.left";
constexpr std::string_view k_MemberKicked = "member.kicked";
constexpr std::string_view k_MemberBanned = "member.banned";
constexpr std::string_view k_MemberChanged = "member.changed";
constexpr std::string_view k_MemberAway = "member.away";
constexpr std::string_view k_MemberBack = "member.back";
constexpr std::string_view k_HostChanged = "host.changed";
constexpr std::string_view k_LobbyChanged = "lobby.changed";
constexpr std::string_view k_StartCountdown = "start.countdown";
constexpr std::string_view k_StartCancelled = "start.cancelled";
constexpr std::string_view k_StartQueued = "start.queued";
constexpr std::string_view k_GameAssigned = "game.assigned";
constexpr std::string_view k_GameStarted = "game.started";
constexpr std::string_view k_GameEnded = "game.ended";
constexpr std::string_view k_LobbyClosed = "lobby.closed";
} // namespace lobby_events

/// The data of "lobbies.changed" ({"id", "summary"}) and of "lobbies.removed" ({"id"})
struct LobbyListChange
{
	std::string id;
	/// None for a removal
	std::optional<LobbySummary> summary;
};

/// The data of the events of the members ("member.*") and of "host.changed"
struct MemberEvent
{
	/// "lobby": the id of the lobby, empty when the event does not give it
	std::string lobby;
	/// "account"
	AccountRef account;
	/// "by": the host that kicked or banned the member
	std::optional<AccountRef> by;
	/// "role", for "member.joined"
	std::optional<MemberRole> role;
	/// "slot"
	std::optional<int32_t> slot;
	/// "ready", for "member.changed"
	std::optional<bool> ready;
};

/// The data of "lobby.changed": {"fields": {...}, "ready_cleared": true}
struct LobbyChange
{
	/// "lobby"
	std::string lobby;
	std::optional<std::string> name;
	std::optional<LobbyMap> map;
	std::optional<int32_t> godSlots;
	std::optional<std::vector<SlotKind>> slotKinds;
	std::optional<LobbyVisibility> visibility;
	std::optional<bool> locked;
	std::optional<LobbySettings> settings;
	/// "ready_cleared": the change cleared the ready of each member
	bool readyCleared {false};
};

/// The data of the events of the start and of the end of a lobby: "start.countdown" {at}, "start.cancelled" {why,
/// account}, "start.queued" {position}, "game.assigned" {game_id}, "game.started", "game.ended", "lobby.closed" {reason}
struct LobbyStateEvent
{
	/// "lobby"
	std::string lobby;
	/// "at"
	std::optional<int64_t> at;
	/// "why": the reason of a cancelled start, for example "not_ready"
	std::string why;
	/// "account": the player that cancelled the start
	std::optional<AccountRef> account;
	/// "position"
	std::optional<int32_t> position;
	/// "game_id"
	std::string gameId;
	/// "reason": the reason of a close, for example "host_closed"
	std::string reason;
};

} // namespace openblack::lobby_api
