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
#include <string>
#include <string_view>
#include <vector>

/// The JSON shapes of the lobby's REST API, version 1, and of the events of its push channel. The lobby is
/// bwgame-service. All shapes of the API are in this file, so that a test can compare them with the OpenAPI document
/// of the lobby. The names of the JSON members are in the comments of the members, and they are the same as the names
/// of the members in snake case.
///
/// The client sends and gets JSON with the content type application/json, except for these:
/// - The logo of a clan, which comes as a BMP file (image/bmp).
/// - The creature upload, which goes as multipart/form-data.
namespace openblack::lobby_api
{

/// The start of the path of each call of the API
constexpr std::string_view k_ApiPrefix = "/api/v1";
/// The path of the push channel, a WSS connection
constexpr std::string_view k_EventsPath = "/api/v1/events";

/// The fewest and the most characters of an account name
constexpr size_t k_FewestNameCharacters = 3;
constexpr size_t k_MostNameCharacters = 32;
/// The most characters of a password. The password box of the game takes 32 characters.
constexpr size_t k_MostPasswordCharacters = 32;

/// True when the text is an account name that the lobby can have: 3 to 32 plain (ASCII) letters and digits. The lobby
/// compares the names without regard to case.
[[nodiscard]] bool IsValidAccountName(std::string_view name) noexcept;

/// The kind of client. The lobby gives a desktop client its refresh token in the answer. The lobby gives a browser
/// client its refresh token in an HttpOnly cookie, which the script of the page cannot read.
enum class ClientKind : uint8_t
{
	Desktop,
	Browser,
};

/// POST /api/v1/sessions
struct LoginRequest
{
	/// "name"
	std::string name;
	/// "password"
	std::string password;
	/// "client": "desktop" or "browser"
	ClientKind client {ClientKind::Desktop};
};

/// POST /api/v1/sessions/refresh. A refresh token works one time. The answer gives a new one.
struct RefreshRequest
{
	/// "refresh_token". A browser client sends none, because the cookie holds it.
	std::optional<std::string> refreshToken;
};

/// "account": the account that a session is for
struct AccountRef
{
	/// "id"
	int64_t id {0};
	/// "name"
	std::string name;
};

/// The answer of a login and of a refresh
struct Session
{
	/// "access_token": the bearer token of the calls, which lasts 15 minutes
	std::string accessToken;
	/// "expires_in": the seconds until the access token stops
	int64_t expiresIn {0};
	/// "refresh_token": only for a desktop client
	std::optional<std::string> refreshToken;
	/// "account"
	AccountRef account;
};

/// "location": the place of the player, for the real-world weather
struct Location
{
	/// "country": the two-letter code of the country
	std::string country;
	/// "area"
	std::string area;
	/// "city"
	std::string city;
};

/// A clan in a list
struct ClanSummary
{
	/// "id"
	int64_t id {0};
	/// "name"
	std::string name;
	/// "points"
	double points {0.0};
};

/// GET /api/v1/me
struct Me
{
	/// "id"
	int64_t id {0};
	/// "name"
	std::string name;
	/// "email"
	std::string email;
	/// "email_verified"
	bool emailVerified {false};
	/// "points"
	double points {0.0};
	/// "clans"
	std::vector<ClanSummary> clans;
	/// "location": none when the player did not give a place
	std::optional<Location> location;
};

/// A game server in the list of GET /api/v1/servers, and the data of the event "servers.changed"
struct ServerEntry
{
	/// "id"
	int64_t id {0};
	/// "name"
	std::string name;
	/// "players": the number of players in the server now
	int32_t players {0};
	/// "max_players"
	int32_t maxPlayers {0};
	/// "playing": the game started
	bool playing {false};
	/// "version": the version of the game that the server runs
	std::string version;
	/// "password_needed"
	bool passwordNeeded {false};
	/// "locked"
	bool locked {false};
	/// "land": the name of the land
	std::string land;
};

/// GET /api/v1/servers: {"servers": [...]}
struct ServerList
{
	std::vector<ServerEntry> servers;
};

/// POST /api/v1/servers/{id}/tickets
struct JoinTicket
{
	/// "url": the WSS address of the game socket of the server
	std::string url;
	/// "ticket": the signed ticket that the client shows to the game server
	std::string ticket;
	/// "expires_in": the seconds until the ticket stops
	int64_t expiresIn {0};
};

/// GET /api/v1/me/clans: {"clans": [...]}
struct ClanList
{
	std::vector<ClanSummary> clans;
};

/// GET /api/v1/clans/{id}
struct ClanDetails
{
	/// "id"
	int64_t id {0};
	/// "name"
	std::string name;
	/// "description": the motto of the clan
	std::string description;
	/// "leader": the name of the leader's account
	std::string leader;
	/// "url": the web site of the clan, can be empty
	std::string url;
	/// "points": the credits of the clan
	double points {0.0};
	/// "members": the number of members
	int32_t members {0};
};

/// A line of a ranking
struct RankingEntry
{
	/// "position": 1 for the first
	int32_t position {0};
	/// "id"
	int64_t id {0};
	/// "name"
	std::string name;
	/// "points"
	double points {0.0};
};

/// GET /api/v1/rankings/players?page=N and GET /api/v1/rankings/clans?page=N
struct RankingPage
{
	/// "page": 1 for the first page
	int32_t page {1};
	/// "pages": the number of pages
	int32_t pages {1};
	/// "entries"
	std::vector<RankingEntry> entries;
};

/// GET /api/v1/players/{id}
struct PlayerDetails
{
	/// "id"
	int64_t id {0};
	/// "name"
	std::string name;
	/// "points"
	double points {0.0};
	/// "games_ranked"
	int32_t gamesRanked {0};
	/// "clans"
	std::vector<ClanSummary> clans;
};

/// GET /api/v1/me/weather?date=YYYY-MM-DD: the real-world weather at the place of the player on a day
struct RealWeather
{
	/// "temperature": in degrees
	int32_t temperature {0};
	/// "visibility"
	int32_t visibility {0};
	/// "type": the weather preset, 0 to 12
	int32_t type {0};
	/// "city"
	std::string city;
	/// "area": can be empty
	std::string area;
	/// "country": the two-letter code of the country
	std::string country;
};

/// A day of the calendar, for the weather
struct Date
{
	int32_t year {0};
	int32_t month {0};
	int32_t day {0};
};

/// POST /api/v1/me/creatures: the files of the player's creature as multipart/form-data, as the creature upload tool of
/// the game sent them
struct CreatureUpload
{
	/// The field "creature": creature.lhp, the package of the values and the pictures of the creature's web page
	std::vector<uint8_t> package;
	/// The field "mind": the creature's mind file, with its file name
	std::vector<uint8_t> mind;
	std::string mindFileName;
	/// The field "physique": the creature's physique file, with its file name
	std::vector<uint8_t> physique;
	std::string physiqueFileName;
};

/// The names of the fields of the creature upload
constexpr std::string_view k_UploadPackageField = "creature";
constexpr std::string_view k_UploadMindField = "mind";
constexpr std::string_view k_UploadPhysiqueField = "physique";
constexpr std::string_view k_UploadPackageFileName = "creature.lhp";

/// The answer of POST /api/v1/me/creatures
struct CreatureUploadReply
{
	/// "url": the web page of the creature
	std::string url;
};

/// The creature of a clan, which the game server gets before a clan game and puts back after it, at
/// /api/v1/clans/{id}/creature. The files are vanilla's mind file and physique file, as base64 in the JSON.
struct ClanCreature
{
	/// "alignment"
	float alignment {0.0f};
	/// "mind"
	std::vector<uint8_t> mind;
	/// "physique"
	std::vector<uint8_t> physique;
};

/// Before the first clan game the clan has no creature. The lobby then tells the game server to make a new creature of
/// a type with a name.
struct NewClanCreature
{
	/// "type": the species of the creature, 0 to 16
	int32_t type {0};
	/// "name"
	std::string name;
};

/// GET /api/v1/clans/{id}/creature: {"creature": {...}} or {"new": {...}}
struct ClanCreatureReply
{
	std::optional<ClanCreature> creature;
	std::optional<NewClanCreature> make;
};

/// The body of an answer with an error (RFC 9457, application/problem+json)
struct Problem
{
	/// "type"
	std::string type;
	/// "title"
	std::string title;
	/// "status"
	int32_t status {0};
	/// "detail"
	std::string detail;
};

/// An event of the push channel: {"seq": n, "type": "...", "data": {...}}
struct PushEvent
{
	/// "seq": the number of the event. On a new connection the client sends the last number that it got.
	int64_t seq {0};
	/// "type", for example "servers.changed"
	std::string type;
	/// "data": the JSON text of the data of the event
	std::string data;
};

/// The names of the events of the push channel
namespace events
{
constexpr std::string_view k_Ping = "ping";
constexpr std::string_view k_ChatLine = "chat.line";
constexpr std::string_view k_ChatJoined = "chat.joined";
constexpr std::string_view k_ChatLeft = "chat.left";
constexpr std::string_view k_PresenceChanged = "presence.changed";
constexpr std::string_view k_ServersChanged = "servers.changed";
constexpr std::string_view k_RoomChanged = "room.changed";
constexpr std::string_view k_ClanInvited = "clan.invited";
constexpr std::string_view k_ClanRequest = "clan.request";
constexpr std::string_view k_AccountMuted = "account.muted";
constexpr std::string_view k_AccountBanned = "account.banned";
constexpr std::string_view k_PointsChanged = "points.changed";
} // namespace events

/// The data of "servers.changed": a server that is new or changed, or the id of a server that went from the list
struct ServerChange
{
	/// The data has the members of ServerEntry, or {"id": n, "removed": true}
	ServerEntry server;
	bool removed {false};
};

/// The data of "points.changed"
struct PointsChange
{
	/// "points"
	double points {0.0};
};

/// The data of "account.banned"
struct Ban
{
	/// "reason"
	std::string reason;
};

/// The frames that the client sends on the push channel. Each action is a REST call, so the client sends only these.
enum class PushOp : uint8_t
{
	/// {"op": "resume", "last_seq": n}: the first frame on a new connection
	Resume,
	/// {"op": "subscribe", "channel": "..."}
	Subscribe,
	/// {"op": "unsubscribe", "channel": "..."}
	Unsubscribe,
	/// {"op": "pong"}: the answer to the event "ping"
	Pong,
};

/// The channel of the push events about the server list
constexpr std::string_view k_ServersChannel = "servers";

} // namespace openblack::lobby_api
