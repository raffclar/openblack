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
	/// "name": the lobby compares it without regard to case
	std::string name;
	/// "password"
	std::string password;
	/// "cookie": true for the browser, which gets its refresh token in an HttpOnly cookie
	bool cookie {false};
};

/// POST /api/v1/sessions/refresh. A refresh token works one time. The answer gives a new one.
struct RefreshRequest
{
	/// "refresh_token". A browser client sends none, because the cookie holds it.
	std::optional<std::string> refreshToken;
};

/// "account": an account by its id and its name
struct AccountRef
{
	/// "id"
	int64_t id {0};
	/// "name"
	std::string name;
};

/// The answer of a login (201) and of a refresh (200). The times are milliseconds since 1970 (UTC), from the ISO 8601
/// times of the lobby.
struct Session
{
	/// "access_token": the bearer token of the calls, which lasts 15 minutes
	std::string accessToken;
	/// "access_token_expires"
	int64_t accessTokenExpires {0};
	/// "refresh_token": null for the browser, whose cookie holds it
	std::optional<std::string> refreshToken;
	/// "refresh_token_expires"
	int64_t refreshTokenExpires {0};
	/// "account"
	AccountRef account;
	/// "email_required": the account has no email address. The lobby refuses every other call with 403
	/// "email_required" until the player adds an address on the account page of the site.
	bool emailRequired {false};
};

/// A clan of the account in GET /api/v1/me
struct ClanRef
{
	/// "id"
	int64_t id {0};
	/// "name"
	std::string name;
	/// "role": the role of the account in the clan
	std::string role;
};

/// GET /api/v1/me. The place (country, area, city) is for the real-world weather.
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
	/// "games_ranked"
	int32_t gamesRanked {0};
	/// "country": the two-letter code of Scripts/country.lhw, for example gb
	std::string country;
	/// "area"
	std::string area;
	/// "city"
	std::string city;
	/// "clans"
	std::vector<ClanRef> clans;
	/// "date_joined"
	int64_t dateJoined {0};
};

/// A god or an observer in the room of a game server
struct RoomPlayer
{
	/// "account"
	AccountRef account;
	/// "role": "god" or "observer"
	std::string role;
	/// "clan": the clan that the god plays for, or null
	std::optional<int64_t> clan;
};

/// A game server in the list of GET /api/v1/servers, and the "server" of the event "servers.changed"
struct ServerEntry
{
	/// "id"
	std::string id;
	/// "hostname": the name of the room
	std::string hostname;
	/// "land"
	std::string land;
	/// "gamemode": "openstaging" (in the room before the game), "closedplaying" or "exiting"
	std::string gamemode;
	/// "locked"
	bool locked {false};
	/// "password": the room needs a password, which the game server checks
	bool password {false};
	/// "numplayers": the gods in the room
	int32_t numplayers {0};
	/// "maxplayers"
	int32_t maxplayers {0};
	/// "gamever", "infoversion", "creatureversion", "oosversion": the versions of the game server
	std::string gamever;
	int32_t infoversion {0};
	int32_t creatureversion {0};
	int32_t oosversion {0};
	/// "players"
	std::vector<RoomPlayer> players;
	/// "url": the game socket, wss://<host>/game/<id>
	std::string url;
	/// "updated": the time of the last status of the server, 0 for none
	int64_t updated {0};
};

/// A god or an observer in the status report of a game server
struct ServerStatusPlayer
{
	/// "account": the account id from the player's ticket
	int64_t account {0};
	/// "role": "god" or "observer"
	std::string role;
	/// "clan": the clan that the god plays for, or null
	std::optional<int64_t> clan;
};

/// PUT /api/v1/servers/{id}/status: the report of a game server every 5 seconds, over mutual TLS
struct ServerStatus
{
	/// "hostname": the name of the room
	std::string hostname;
	/// "land"
	std::string land;
	/// "gamemode": "openstaging" (in the room before the game), "closedplaying" or "exiting"
	std::string gamemode;
	/// "locked"
	bool locked {false};
	/// "password": the room needs a password
	bool password {false};
	/// "maxplayers": 1 to 32 gods
	int32_t maxplayers {1};
	/// "gamever": the version of the game server, 16 characters at most
	std::string gamever;
	/// "players"
	std::vector<ServerStatusPlayer> players;
};

/// GET /api/v1/servers: {"servers": [...]}
struct ServerList
{
	std::vector<ServerEntry> servers;
};

/// The roles of a join ticket
constexpr std::string_view k_GodRole = "god";
constexpr std::string_view k_ObserverRole = "observer";

/// The body of POST /api/v1/servers/{id}/tickets
struct TicketRequest
{
	/// "role": "god" takes a god slot. "observer" only watches, takes no slot and can join a full room.
	std::string role {k_GodRole};
};

/// POST /api/v1/servers/{id}/tickets (201)
struct JoinTicket
{
	/// "url": the WSS address of the game socket of the server
	std::string url;
	/// "ticket": a PASETO v4.public token that the client sends when the game socket opens
	std::string ticket;
	/// "expires": 60 seconds after the lobby made it
	int64_t expires {0};
	/// "role": the role that the ticket gives
	std::string role {k_GodRole};
};

/// The codes of the problems that the front end tells the player about
namespace problems
{
/// The account has no email address (403)
constexpr std::string_view k_EmailRequired = "email_required";
/// The gods fill the room of the server. An observer ticket can still join (409).
constexpr std::string_view k_ServerFull = "server_full";
/// The admins of the server banned the account (403)
constexpr std::string_view k_BannedFromServer = "banned_from_server";
/// The game of the server started (409)
constexpr std::string_view k_GameStarted = "game_started";
/// The room of the server is locked (409)
constexpr std::string_view k_ServerLocked = "server_locked";
} // namespace problems

/// A clan in a list. Not in the contract of the lobby yet: the shape of the plan.
struct ClanSummary
{
	/// "id"
	int64_t id {0};
	/// "name"
	std::string name;
	/// "points"
	double points {0.0};
};

/// GET /api/v1/me/clans: {"clans": [...]}. Not in the contract of the lobby yet.
struct ClanList
{
	std::vector<ClanSummary> clans;
};

/// GET /api/v1/clans/{id}. Not in the contract of the lobby yet.
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

/// GET /api/v1/rankings/players?page=N and GET /api/v1/rankings/clans?page=N. Not in the contract of the lobby
/// yet.
struct RankingPage
{
	/// "page": 1 for the first page
	int32_t page {1};
	/// "pages": the number of pages
	int32_t pages {1};
	/// "entries"
	std::vector<RankingEntry> entries;
};

/// GET /api/v1/players/{id}. Not in the contract of the lobby yet.
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

/// GET /api/v1/me/weather?date=YYYY-MM-DD: the real-world weather at the place of the player on a day. Not in the
/// contract of the lobby yet.
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
/// the game sent them. Not in the contract of the lobby yet.
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
	/// "code": the code of the error that a program can read
	std::string code;
};

/// A frame of the push channel. An event: {"seq": n, "type": "...", "time": "...", "data": {...}}. A control frame
/// ("hello", "subscribed", "unsubscribed", "resumed", "resync", "error") has no "seq".
struct PushEvent
{
	/// "seq": the number of the event, which goes up by one for each event that the lobby sends to any client. None for
	/// a control frame.
	std::optional<int64_t> seq;
	/// "type", for example "servers.changed"
	std::string type;
	/// "time": the ISO 8601 time of the event, empty for a control frame
	std::string time;
	/// "data": the JSON text of the data of the frame
	std::string data;
};

/// The names of the frames of the push channel
namespace events
{
// The control frames
constexpr std::string_view k_Hello = "hello";
constexpr std::string_view k_Subscribed = "subscribed";
constexpr std::string_view k_Unsubscribed = "unsubscribed";
constexpr std::string_view k_Resumed = "resumed";
constexpr std::string_view k_Resync = "resync";
constexpr std::string_view k_Error = "error";
// The events
constexpr std::string_view k_ChatLine = "chat.line";
constexpr std::string_view k_ChatJoined = "chat.joined";
constexpr std::string_view k_ChatLeft = "chat.left";
constexpr std::string_view k_PresenceChanged = "presence.changed";
constexpr std::string_view k_ServersChanged = "servers.changed";
constexpr std::string_view k_ServersRemoved = "servers.removed";
constexpr std::string_view k_RoomChanged = "room.changed";
constexpr std::string_view k_ClanInvited = "clan.invited";
constexpr std::string_view k_ClanRequest = "clan.request";
constexpr std::string_view k_ServerBanned = "server.banned";
constexpr std::string_view k_ServerUnbanned = "server.unbanned";
constexpr std::string_view k_AccountMuted = "account.muted";
constexpr std::string_view k_AccountBanned = "account.banned";
constexpr std::string_view k_PointsChanged = "points.changed";
constexpr std::string_view k_RoomClanChosen = "room.clan_chosen";
} // namespace events

/// The data of "hello" (the first frame on each connection), "resumed" and "resync": the epoch of the push process
/// and the number of its last event
struct PushPosition
{
	/// "epoch": a new push process has a new epoch, and the events of the old one are lost
	std::string epoch;
	/// "seq"
	int64_t seq {0};
};

/// The data of "error"
struct PushError
{
	/// "code"
	std::string code;
	/// "detail"
	std::string detail;
	/// "channel", when the error is about a channel
	std::string channel;
};

/// The data of "servers.changed" ({"server": {...}}) and of "servers.removed" ({"id": "...", "reason": "..."})
struct ServerChange
{
	ServerEntry server;
	bool removed {false};
	/// The reason of a removal
	std::string reason;
};

/// The data of "points.changed" after a ranked game
struct PointsChange
{
	/// "points": the new points of the player
	double points {0.0};
	/// "games_ranked"
	int32_t gamesRanked {0};
	/// "position": the place of the player in the game
	int32_t position {0};
	/// "change": the points won or lost
	double change {0.0};
};

/// The most characters of a chat line. The chat box of the game takes 254 characters.
constexpr size_t k_MostChatCharacters = 254;

/// POST /api/v1/chat/channels/{channel}/messages: a line of 1 to 254 characters to the lobby or to the room of a server
struct ChatLineRequest
{
	/// "text"
	std::string text;
};

/// The data of "chat.line": {"channel": "...", "from": {"id": n, "name": "..."}, "text": "..."}
struct ChatLine
{
	/// "channel"
	std::string channel;
	/// "from"
	AccountRef from;
	/// "text"
	std::string text;
};

/// GET /api/v1/account-names/{name}: whether a name can have a new account
struct AccountNameCheck
{
	/// "name"
	std::string name;
	/// "valid": 3 to 32 letters and digits, with no blocked word or reserved name
	bool valid {false};
	/// "available": valid, and no account has it
	bool available {false};
};

/// PUT /api/v1/servers/{id}/clan: "Use This Clan" for a god in the room of the server, or none to play for itself
struct UseClanRequest
{
	/// "clan": null to play for itself
	std::optional<int64_t> clan;
};

/// The answer of "Use This Clan": {"clan": {...}} or {"clan": null}
struct UsedClan
{
	std::optional<ClanRef> clan;
};

/// The data of "room.changed": who is in the room of a server
struct RoomChange
{
	/// "server": the id of the server
	std::string server;
	/// "gamemode"
	std::string gamemode;
	/// "players"
	std::vector<RoomPlayer> players;
};

/// The data of "room.clan_chosen": a god in the room plays for a clan, or for itself
struct RoomClanChoice
{
	/// "server"
	std::string server;
	/// "account"
	AccountRef account;
	/// "clan": none when the god plays for itself
	std::optional<ClanRef> clan;
};

/// The data of "server.banned": a game server banned the account
struct ServerBanned
{
	/// "server": "id"
	std::string server;
	/// "server": "name"
	std::string name;
	/// "reason"
	std::string reason;
};

/// The data of "account.banned"
struct Ban
{
	/// "reason"
	std::string reason;
};

/// The channels of the push channel
constexpr std::string_view k_LobbyChannel = "lobby";
constexpr std::string_view k_ServersChannel = "servers";
/// The channel of the room of a game server is this text and the id of the server
constexpr std::string_view k_ServerChannelPrefix = "server-";

/// The handshake of the push channel. The desktop sends the header "Authorization: Bearer <access token>". A browser
/// cannot send headers on a WebSocket, so it asks for the subprotocols "bwgame.v1" and "bearer.<access token>". The
/// lobby answers with "bwgame.v1".
constexpr std::string_view k_PushSubprotocol = "bwgame.v1";
constexpr std::string_view k_PushTokenSubprotocolPrefix = "bearer.";

/// The close codes of the push channel
constexpr uint16_t k_CloseSignedOut = 4001;
constexpr uint16_t k_CloseBanned = 4003;
constexpr uint16_t k_CloseTooManyFrames = 4008;

} // namespace openblack::lobby_api
