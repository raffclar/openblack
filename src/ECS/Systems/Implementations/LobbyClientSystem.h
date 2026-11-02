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

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ECS/Systems/LobbyClientSystemInterface.h"
#include "Online/Backoff.h"
#include "Online/HttpClientInterface.h"
#include "Online/PushChannelInterface.h"
#include "Online/TokenStoreInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

/// Where the lobby is
struct LobbyClientConfig
{
	/// The https:// origin of the lobby on the desktop, for example "https://bw.example.org". In the browser it is
	/// empty: the lobby is the origin of the page.
	std::string origin;
	lobby_api::ClientKind client {lobby_api::ClientKind::Desktop};
};

/// The times of the client
namespace lobby_times
{
/// The client gets a new access token this long before the old one stops, so that a call does not arrive late
constexpr std::chrono::seconds k_RefreshMargin {30};
/// The first wait before the push channel opens again after a loss. Each loss after it doubles the wait, to the most.
constexpr std::chrono::seconds k_FirstReconnectWait {1};
constexpr std::chrono::seconds k_MostReconnectWait {60};
} // namespace lobby_times

class LobbyClientSystem final: public LobbyClientSystemInterface
{
public:
	/// The clock of the computer. The lobby gives the end of each token as a time of the calendar.
	using Clock = std::function<std::chrono::system_clock::time_point()>;

	/// With no config, the game has no lobby, and each call fails with NotConfigured
	LobbyClientSystem(std::optional<LobbyClientConfig> config, online::HttpClientInterface& http,
	                  std::unique_ptr<online::PushChannelInterface> push, std::unique_ptr<online::TokenStoreInterface> tokens,
	                  Clock clock);
	~LobbyClientSystem() override;
	LobbyClientSystem(const LobbyClientSystem&) = delete;
	LobbyClientSystem& operator=(const LobbyClientSystem&) = delete;
	LobbyClientSystem(LobbyClientSystem&&) = delete;
	LobbyClientSystem& operator=(LobbyClientSystem&&) = delete;

	void Update() override;

	[[nodiscard]] bool IsConfigured() const override { return _config.has_value(); }
	[[nodiscard]] LobbySessionState GetState() const override { return _state; }
	[[nodiscard]] std::optional<lobby_api::AccountRef> GetAccount() const override { return _account; }
	[[nodiscard]] bool IsPushOpen() const override { return _push->IsOpen(); }
	[[nodiscard]] bool IsEmailRequired() const override { return _emailRequired; }

	void Login(std::string name, std::string password, LobbyCallback<lobby_api::AccountRef> callback) override;
	void ResumeSession(LobbyCallback<lobby_api::AccountRef> callback) override;
	void Logout(bool everywhere, LobbyCallback<LobbyDone> callback) override;

	void GetMe(LobbyCallback<lobby_api::Me> callback) override;
	void GetServers(LobbyCallback<lobby_api::ServerList> callback) override;
	[[nodiscard]] std::span<const lobby_api::ServerEntry> GetKnownServers() const override { return _servers; }
	void WatchServers(bool watch) override;
	void WatchChannel(std::string channel, bool watch) override;
	void RequestJoinTicket(std::string serverId, game_socket::Role role,
	                       LobbyCallback<lobby_api::JoinTicket> callback) override;
	void SendChatLine(std::string channel, std::string text, LobbyCallback<LobbyDone> callback) override;
	void UseClan(std::string serverId, std::optional<int64_t> clanId, LobbyCallback<lobby_api::UsedClan> callback) override;
	void CheckAccountName(std::string name, LobbyCallback<lobby_api::AccountNameCheck> callback) override;

	void GetMyClans(LobbyCallback<lobby_api::ClanList> callback) override;
	void GetClan(int64_t clanId, LobbyCallback<lobby_api::ClanDetails> callback) override;
	void GetClanLogo(int64_t clanId, LobbyCallback<std::vector<uint8_t>> callback) override;
	void GetClanCreature(int64_t clanId, LobbyCallback<lobby_api::ClanCreatureReply> callback) override;
	void PutClanCreature(int64_t clanId, lobby_api::ClanCreature creature, LobbyCallback<LobbyDone> callback) override;
	void GetMaps(LobbyCallback<lobby_api::MapList> callback) override;
	void DownloadMap(const lobby_api::OnlineMap& map, LobbyCallback<std::vector<uint8_t>> callback) override;

	void GetPlayerRanking(int32_t page, LobbyCallback<lobby_api::RankingPage> callback) override;
	void GetClanRanking(int32_t page, LobbyCallback<lobby_api::RankingPage> callback) override;
	void GetPlayer(int64_t playerId, LobbyCallback<lobby_api::PlayerDetails> callback) override;
	void GetPlayerDetailLines(int64_t playerId, std::string version,
	                          LobbyCallback<lobby_api::PlayerDetailLines> callback) override;

	void GetWeather(lobby_api::Date date, LobbyCallback<lobby_api::RealWeather> callback) override;
	void UploadCreature(lobby_api::CreatureUpload upload, LobbyCallback<lobby_api::CreatureUploadReply> callback) override;
	void SendSkirmishPing(LobbyCallback<LobbyDone> callback) override;

	uint32_t AddEventListener(LobbyEventListener listener) override;
	void RemoveEventListener(uint32_t id) override;

private:
	/// A call of the API that needs the access token
	struct Call
	{
		online::HttpMethod method {online::HttpMethod::Get};
		/// The path after the start of the API, for example "/me"
		std::string path;
		std::vector<online::HttpHeader> headers;
		std::vector<uint8_t> body;
		std::function<void(online::HttpResponse&&)> done;
		/// The lobby refused the access token one time, and the client got a new one
		bool retried {false};
		/// A large file (online::HttpRequest::large)
		bool large {false};
	};

	[[nodiscard]] std::string Address(std::string_view apiPath) const;
	[[nodiscard]] bool AccessTokenValid() const;
	/// Sends the call with the access token, after a refresh of the tokens when the access token is old
	void Authorized(Call call);
	void Dispatch(Call call);
	/// Gets a new pair of tokens. The function runs after it, with the error or with None.
	void Refresh(std::function<void(const LobbyError&)> after);
	void ApplySession(const lobby_api::Session& session);
	/// Forgets the session and the tokens here and in the store
	void EndSession();
	/// Sends a call that needs the session, and reads its answer with a parser
	template <typename Value, typename Parser>
	void CallFor(online::HttpMethod method, std::string path, LobbyCallback<Value> callback, Parser parse, bool large = false);

	void OpenPush();
	void HandlePush(std::string_view text);
	void HandleControl(const lobby_api::PushEvent& frame);
	void PushClosed(online::PushClose reason);

	std::optional<LobbyClientConfig> _config;
	online::HttpClientInterface& _http;
	std::unique_ptr<online::PushChannelInterface> _push;
	std::unique_ptr<online::TokenStoreInterface> _tokens;
	Clock _clock;

	LobbySessionState _state {LobbySessionState::LoggedOut};
	std::optional<lobby_api::AccountRef> _account;
	bool _emailRequired {false};
	std::string _accessToken;
	std::chrono::system_clock::time_point _accessExpiry {};
	/// The refresh token of this run, also when the store cannot keep it
	std::optional<std::string> _refreshToken;
	/// The number of the session. An answer that comes after the session ended is ignored.
	uint32_t _generation {0};
	bool _refreshing {false};
	std::vector<std::function<void(const LobbyError&)>> _afterRefresh;
	bool _loggingOut {false};

	std::vector<lobby_api::ServerEntry> _servers;
	std::optional<std::string> _serversTag;
	/// The channels of the push channel that the client wants
	std::vector<std::string> _channels;

	/// The epoch of the push process and the number of the last event that the client got. Empty before the first
	/// connection.
	std::string _epoch;
	int64_t _lastSeq {0};
	bool _pushUnavailable {false};
	bool _pushWanted {false};
	std::optional<std::chrono::system_clock::time_point> _reconnectAt;
	online::ReconnectBackoff _reconnectWait {lobby_times::k_FirstReconnectWait, lobby_times::k_MostReconnectWait};

	std::vector<std::pair<uint32_t, LobbyEventListener>> _listeners;
	uint32_t _nextListener {1};
};

/// The error of an answer of the lobby. None for a 2xx answer.
[[nodiscard]] LobbyError LobbyErrorOf(const online::HttpResponse& response);

} // namespace openblack::ecs::systems
