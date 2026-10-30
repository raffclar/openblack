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

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <GameSocket.h>
#include <LobbyApi.h>

namespace openblack::ecs::systems
{

/// Why a call of the lobby failed
enum class LobbyErrorKind : uint8_t
{
	None,
	/// The game has no address of a lobby
	NotConfigured,
	/// The call needs a session, and the player is not logged in, or the lobby ended the session
	NotLoggedIn,
	/// The account name is not 3 to 32 letters and digits
	InvalidName,
	/// The password is empty or longer than 32 characters
	InvalidPassword,
	/// A login or a logout is in progress
	Busy,
	/// The address is not an HTTPS address, or a WSS address for a game socket
	AddressRefused,
	Network,
	Tls,
	Timeout,
	Cancelled,
	/// The lobby answered with a body that the client cannot read
	BadAnswer,
	/// The lobby refused the call (400, 403, 404, 409, 422). A login with a wrong name or password gives this too.
	Refused,
	/// The lobby got too many calls (429). The error gives the seconds to wait.
	RateLimited,
	/// The lobby failed (500 and more)
	ServerFault,
};

struct LobbyError
{
	LobbyErrorKind kind {LobbyErrorKind::None};
	/// The HTTP status, 0 when there was no answer
	int32_t status {0};
	/// The detail of the problem that the lobby gave, can be empty
	std::string detail;
	/// The code of the problem that the lobby gave, for example "server_full". Empty when the answer had none.
	std::string code;
	/// The seconds to wait before the next call, for RateLimited
	std::optional<int32_t> retryAfterSeconds;
};

/// The result of a call: a value, or an error
template <typename Value>
struct LobbyResult
{
	std::optional<Value> value;
	LobbyError error;

	[[nodiscard]] bool Ok() const { return value.has_value(); }
};

/// The value of a call that gives nothing back
struct LobbyDone
{
};

template <typename Value>
using LobbyCallback = std::function<void(LobbyResult<Value>&&)>;

enum class LobbySessionState : uint8_t
{
	LoggedOut,
	/// A login or a resume of the session is in progress
	LoggingIn,
	LoggedIn,
};

using LobbyEventListener = std::function<void(const lobby_api::PushEvent& event)>;

/// The client of the lobby (bwgame-service): the session of the player, the server list, the join tickets, the clans,
/// the rankings, the real-world weather and the creature upload. The calls go over HTTPS to the REST API of the lobby,
/// and the events come on the push channel, which the client keeps open while the player is logged in.
///
/// Each call returns at once. Its callback runs later, in a frame, after the answer comes. When the client can see the
/// failure without the lobby, for example with no session, the callback runs before the call returns.
///
/// The session: a login gives an access token, which lasts 15 minutes, and a refresh token, which works one time. When
/// the access token is old, the client uses the refresh token to get a new pair before the next call. The store of
/// the platform keeps the refresh token between two runs of the game.
class LobbyClientSystemInterface
{
public:
	virtual ~LobbyClientSystemInterface() = default;

	/// Once a frame, after the web client moved its transfers on: the push channel moves on, and opens again after a
	/// loss
	virtual void Update() = 0;

	/// Whether the game has the address of a lobby
	[[nodiscard]] virtual bool IsConfigured() const = 0;
	[[nodiscard]] virtual LobbySessionState GetState() const = 0;
	/// The account of the session, while the player is logged in
	[[nodiscard]] virtual std::optional<lobby_api::AccountRef> GetAccount() const = 0;
	/// Whether the push channel is open
	[[nodiscard]] virtual bool IsPushOpen() const = 0;
	/// The account of the session has no email address. The lobby refuses most calls until the player adds one on the
	/// account page of the site.
	[[nodiscard]] virtual bool IsEmailRequired() const = 0;

	/// Logs in with a name and a password. The client checks the name before it calls the lobby.
	virtual void Login(std::string name, std::string password, LobbyCallback<lobby_api::AccountRef> callback) = 0;
	/// Continues the session of an earlier run with the refresh token that the store keeps, or that the cookie holds in
	/// the browser
	virtual void ResumeSession(LobbyCallback<lobby_api::AccountRef> callback) = 0;
	/// Ends the session here, or every session of the account. The client forgets the tokens also when the lobby does
	/// not answer.
	virtual void Logout(bool everywhere, LobbyCallback<LobbyDone> callback) = 0;

	virtual void GetMe(LobbyCallback<lobby_api::Me> callback) = 0;
	/// The server list. The client sends the tag of the list that it has, and the lobby sends a list only when it changed.
	virtual void GetServers(LobbyCallback<lobby_api::ServerList> callback) = 0;
	/// The last server list, with the changes that came on the push channel
	[[nodiscard]] virtual std::span<const lobby_api::ServerEntry> GetKnownServers() const = 0;
	/// Asks the lobby for the changes of the server list on the push channel, or stops them
	virtual void WatchServers(bool watch) = 0;
	/// Asks the lobby for the events of a channel of the push channel ("lobby", "servers" or "server-<id>"), or stops
	/// them. The client asks again for each channel when the push channel opens again.
	virtual void WatchChannel(std::string channel, bool watch) = 0;
	/// The lands of network games that the lobby gives to the players of a room that plays them (deviation 169). A lobby
	/// with no such list gives an error.
	virtual void GetMaps(LobbyCallback<lobby_api::MapList> callback)
	{
		callback({.value = std::nullopt, .error = {.kind = LobbyErrorKind::NotConfigured}});
	}
	/// The bytes of the file of a land of the list. The lobby gives the file only to a player with a ticket for a room
	/// that plays it. The answer can come compressed; the callback gets the bytes of the file.
	virtual void DownloadMap(const lobby_api::OnlineMap& /*map*/, LobbyCallback<std::vector<uint8_t>> callback)
	{
		callback({.value = std::nullopt, .error = {.kind = LobbyErrorKind::NotConfigured}});
	}
	/// A ticket to join a game server, as a god or as an observer. The address of the game socket must be a WSS
	/// address.
	virtual void RequestJoinTicket(std::string serverId, game_socket::Role role,
	                               LobbyCallback<lobby_api::JoinTicket> callback) = 0;
	/// Sends a chat line of 1 to 254 characters to a channel: "lobby", or "server-<id>" for the room of a server. The
	/// line comes back to each player in the channel as a "chat.line" event.
	virtual void SendChatLine(std::string channel, std::string text, LobbyCallback<LobbyDone> callback) = 0;
	/// "Use This Clan": the player plays for one of their clans in the room of a server, or for no clan
	virtual void UseClan(std::string serverId, std::optional<int64_t> clanId, LobbyCallback<lobby_api::UsedClan> callback) = 0;
	/// Whether a name can have a new account. This call needs no session.
	virtual void CheckAccountName(std::string name, LobbyCallback<lobby_api::AccountNameCheck> callback) = 0;

	virtual void GetMyClans(LobbyCallback<lobby_api::ClanList> callback) = 0;
	virtual void GetClan(int64_t clanId, LobbyCallback<lobby_api::ClanDetails> callback) = 0;
	/// The logo of a clan: the bytes of a BMP file
	virtual void GetClanLogo(int64_t clanId, LobbyCallback<std::vector<uint8_t>> callback) = 0;

	virtual void GetPlayerRanking(int32_t page, LobbyCallback<lobby_api::RankingPage> callback) = 0;
	virtual void GetClanRanking(int32_t page, LobbyCallback<lobby_api::RankingPage> callback) = 0;
	virtual void GetPlayer(int64_t playerId, LobbyCallback<lobby_api::PlayerDetails> callback) = 0;

	/// The real-world weather on a day at the place of the player's account
	virtual void GetWeather(lobby_api::Date date, LobbyCallback<lobby_api::RealWeather> callback) = 0;
	/// Sends the files of the player's creature to the lobby, which gives the address of the creature's web page
	virtual void UploadCreature(lobby_api::CreatureUpload upload, LobbyCallback<lobby_api::CreatureUploadReply> callback) = 0;
	/// Tells the lobby that a skirmish of the player is decided. The call sends no data: the session tells the lobby
	/// which account sends it.
	virtual void SendSkirmishPing(LobbyCallback<LobbyDone> callback) = 0;

	/// A listener gets each event of the push channel, after the client used it. Returns the id of the listener.
	virtual uint32_t AddEventListener(LobbyEventListener listener) = 0;
	virtual void RemoveEventListener(uint32_t id) = 0;
};

} // namespace openblack::ecs::systems
