/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <chrono>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <LobbyApi.h>
#include <gtest/gtest.h>

#include "Online/HttpClientInterface.h"
#include "Online/PushChannelInterface.h"
#include "Online/TokenStoreInterface.h"

// Enable this define because the test makes the system itself
#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/LobbyClientSystem.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace std::chrono_literals;
using online::HttpMethod;
using online::HttpResponse;

namespace
{

/// A web client that keeps each request until the test answers it
class FakeHttp final: public online::HttpClientInterface
{
public:
	struct Sent
	{
		online::HttpRequest request;
		online::HttpCallback callback;
	};

	online::HttpRequestId Send(online::HttpRequest request, online::HttpCallback callback) override
	{
		sent.push_back({std::move(request), std::move(callback)});
		return static_cast<online::HttpRequestId>(sent.size());
	}
	void Cancel(online::HttpRequestId /*id*/) override {}
	void Poll() override {}
	[[nodiscard]] size_t Pending() const override { return sent.size(); }

	/// Answers the oldest request
	void Answer(int32_t status, std::string_view body, std::vector<online::HttpHeader> headers = {})
	{
		ASSERT_FALSE(sent.empty());
		auto next = std::move(sent.front());
		sent.pop_front();
		next.callback(HttpResponse {
		    .error = online::HttpError::None,
		    .status = status,
		    .headers = std::move(headers),
		    .body = {body.begin(), body.end()},
		});
	}
	void Fail(online::HttpError error)
	{
		ASSERT_FALSE(sent.empty());
		auto next = std::move(sent.front());
		sent.pop_front();
		next.callback(HttpResponse {.error = error});
	}
	[[nodiscard]] const online::HttpRequest& Next() const { return sent.front().request; }
	[[nodiscard]] std::string NextBody() const { return {Next().body.begin(), Next().body.end()}; }
	[[nodiscard]] std::optional<std::string> NextHeader(std::string_view name) const
	{
		for (const auto& [key, value] : Next().headers)
		{
			if (key == name)
			{
				return value;
			}
		}
		return std::nullopt;
	}

	std::deque<Sent> sent;
};

class FakePush final: public online::PushChannelInterface
{
public:
	void Open(std::string url, std::string accessToken, online::PushHandlers handlers) override
	{
		++opens;
		lastUrl = std::move(url);
		lastToken = std::move(accessToken);
		_handlers = std::move(handlers);
	}
	void Send(std::string_view text) override { frames.emplace_back(text); }
	void Close() override
	{
		if (_open)
		{
			_open = false;
			++closes;
		}
	}
	void Poll() override {}
	[[nodiscard]] bool IsOpen() const override { return _open; }

	void Accept()
	{
		_open = true;
		_handlers.opened();
	}
	void Deliver(std::string_view text) { _handlers.message(text); }
	void Lose(online::PushClose reason)
	{
		_open = false;
		_handlers.closed(reason);
	}

	int opens {0};
	int closes {0};
	std::string lastUrl;
	std::string lastToken;
	std::vector<std::string> frames;

private:
	online::PushHandlers _handlers;
	bool _open {false};
};

class FakeTokens final: public online::TokenStoreInterface
{
public:
	explicit FakeTokens(std::optional<std::string>& kept)
	    : _kept(kept)
	{
	}
	[[nodiscard]] std::optional<std::string> Load() override { return _kept; }
	bool Save(std::string_view token) override
	{
		_kept = std::string(token);
		return true;
	}
	void Clear() override { _kept.reset(); }

private:
	std::optional<std::string>& _kept;
};

constexpr std::string_view k_Session =
    R"({"access_token":"access1","expires_in":900,"refresh_token":"refresh1","account":{"id":7,"name":"abc"}})";
constexpr std::string_view k_Rotated =
    R"({"access_token":"access2","expires_in":900,"refresh_token":"refresh2","account":{"id":7,"name":"abc"}})";
constexpr std::string_view k_Origin = "https://bw.example";

/// A client with fakes, and a clock that the test moves
class LobbyClientTest: public ::testing::Test
{
protected:
	void Make(lobby_api::ClientKind client = lobby_api::ClientKind::Desktop, bool configured = true)
	{
		auto push = std::make_unique<FakePush>();
		_push = push.get();
		std::optional<LobbyClientConfig> config;
		if (configured)
		{
			config = LobbyClientConfig {
			    .origin = client == lobby_api::ClientKind::Desktop ? std::string(k_Origin) : std::string {},
			    .client = client,
			};
		}
		_client = std::make_unique<LobbyClientSystem>(config, _http, std::move(push), std::make_unique<FakeTokens>(_kept),
		                                              [this]() { return _now; });
	}

	/// Logs in and opens the push channel
	void LogIn()
	{
		_client->Login("abc", "secret", [](auto&& /*result*/) {});
		_http.Answer(201, k_Session);
		_push->Accept();
	}

	FakeHttp _http;
	FakePush* _push {nullptr};
	std::optional<std::string> _kept;
	std::chrono::steady_clock::time_point _now {std::chrono::steady_clock::time_point {} + 1000h};
	std::unique_ptr<LobbyClientSystem> _client;
};

} // namespace

TEST_F(LobbyClientTest, WithNoLobbyEachCallFailsAtOnce)
{
	Make(lobby_api::ClientKind::Desktop, false);
	std::optional<LobbyErrorKind> kind;
	_client->Login("abc", "secret", [&](auto&& result) { kind = result.error.kind; });
	EXPECT_EQ(kind, LobbyErrorKind::NotConfigured);
	_client->GetWeather({}, [&](auto&& result) { kind = result.error.kind; });
	EXPECT_EQ(kind, LobbyErrorKind::NotConfigured);
	EXPECT_TRUE(_http.sent.empty());
	EXPECT_FALSE(_client->IsConfigured());
}

TEST_F(LobbyClientTest, LoginChecksTheNameAndThePasswordFirst)
{
	Make();
	std::optional<LobbyErrorKind> kind;
	_client->Login("a_b", "secret", [&](auto&& result) { kind = result.error.kind; });
	EXPECT_EQ(kind, LobbyErrorKind::InvalidName);
	_client->Login("abc", "", [&](auto&& result) { kind = result.error.kind; });
	EXPECT_EQ(kind, LobbyErrorKind::InvalidPassword);
	_client->Login("abc", std::string(33, 'p'), [&](auto&& result) { kind = result.error.kind; });
	EXPECT_EQ(kind, LobbyErrorKind::InvalidPassword);
	EXPECT_TRUE(_http.sent.empty());
}

TEST_F(LobbyClientTest, LoginKeepsTheRefreshTokenAndOpensThePushChannel)
{
	Make();
	std::optional<lobby_api::AccountRef> account;
	_client->Login("abc", "secret", [&](auto&& result) { account = result.value; });
	EXPECT_EQ(_client->GetState(), LobbySessionState::LoggingIn);
	ASSERT_EQ(_http.sent.size(), 1u);
	EXPECT_EQ(_http.Next().method, HttpMethod::Post);
	EXPECT_EQ(_http.Next().url, "https://bw.example/api/v1/sessions");
	EXPECT_EQ(_http.NextBody(), R"({"client":"desktop","name":"abc","password":"secret"})");
	_http.Answer(201, k_Session);

	ASSERT_TRUE(account.has_value());
	EXPECT_EQ(account->name, "abc");
	EXPECT_EQ(_client->GetState(), LobbySessionState::LoggedIn);
	EXPECT_EQ(_kept, "refresh1");
	EXPECT_EQ(_push->opens, 1);
	EXPECT_EQ(_push->lastUrl, "wss://bw.example/api/v1/events");
	EXPECT_EQ(_push->lastToken, "access1");
	_push->Accept();
	ASSERT_EQ(_push->frames.size(), 1u);
	EXPECT_EQ(_push->frames[0], R"({"last_seq":0,"op":"resume"})");
}

TEST_F(LobbyClientTest, WrongPasswordIsRefusedAndKeepsNothing)
{
	Make();
	std::optional<LobbyErrorKind> kind;
	_client->Login("abc", "wrong", [&](auto&& result) { kind = result.error.kind; });
	_http.Answer(401, R"({"title":"Unauthorized","status":401,"detail":"Wrong name or password"})");
	EXPECT_EQ(kind, LobbyErrorKind::Refused);
	EXPECT_EQ(_client->GetState(), LobbySessionState::LoggedOut);
	EXPECT_FALSE(_kept.has_value());
	EXPECT_EQ(_push->opens, 0);
}

TEST_F(LobbyClientTest, CallsSendTheAccessToken)
{
	Make();
	LogIn();
	std::optional<lobby_api::Me> me;
	_client->GetMe([&](auto&& result) { me = result.value; });
	EXPECT_EQ(_http.Next().url, "https://bw.example/api/v1/me");
	EXPECT_EQ(_http.NextHeader("Authorization"), "Bearer access1");
	_http.Answer(200, R"({"id":7,"name":"abc"})");
	ASSERT_TRUE(me.has_value());
	EXPECT_EQ(me->id, 7);
}

TEST_F(LobbyClientTest, AnOldAccessTokenIsRefreshedOnceForTheCallsThatWait)
{
	Make();
	LogIn();
	_now += 15min;
	int answers = 0;
	_client->GetMe([&](auto&& result) { answers += result.Ok() ? 1 : 0; });
	_client->GetMyClans([&](auto&& result) { answers += result.Ok() ? 1 : 0; });
	// One refresh, and the two calls wait for it
	ASSERT_EQ(_http.sent.size(), 1u);
	EXPECT_EQ(_http.Next().url, "https://bw.example/api/v1/sessions/refresh");
	EXPECT_EQ(_http.NextBody(), R"({"refresh_token":"refresh1"})");
	_http.Answer(200, k_Rotated);
	// The new refresh token replaces the old one at once, because each token works one time
	EXPECT_EQ(_kept, "refresh2");
	ASSERT_EQ(_http.sent.size(), 2u);
	EXPECT_EQ(_http.NextHeader("Authorization"), "Bearer access2");
	_http.Answer(200, R"({"id":7,"name":"abc"})");
	_http.Answer(200, R"({"clans":[]})");
	EXPECT_EQ(answers, 2);
}

TEST_F(LobbyClientTest, TheTokenIsRefreshedBeforeItStops)
{
	Make();
	LogIn();
	// 30 seconds before the end the client already gets a new pair
	_now += 900s - lobby_times::k_RefreshMargin;
	_client->GetMe([](auto&& /*result*/) {});
	EXPECT_EQ(_http.Next().url, "https://bw.example/api/v1/sessions/refresh");
}

TEST_F(LobbyClientTest, ARefusedAccessTokenGetsOneRefreshAndOneRetry)
{
	Make();
	LogIn();
	std::optional<LobbyResult<lobby_api::Me>> got;
	_client->GetMe([&](auto&& result) { got = std::move(result); });
	_http.Answer(401, "{}");
	EXPECT_EQ(_http.Next().url, "https://bw.example/api/v1/sessions/refresh");
	_http.Answer(200, k_Rotated);
	EXPECT_EQ(_http.NextHeader("Authorization"), "Bearer access2");
	// A second refusal goes to the caller
	_http.Answer(401, "{}");
	ASSERT_TRUE(got.has_value());
	EXPECT_EQ(got->error.kind, LobbyErrorKind::NotLoggedIn);
	EXPECT_TRUE(_http.sent.empty());
}

TEST_F(LobbyClientTest, ARefusedRefreshEndsTheSession)
{
	Make();
	LogIn();
	_now += 1h;
	std::optional<LobbyErrorKind> kind;
	_client->GetMe([&](auto&& result) { kind = result.error.kind; });
	_http.Answer(401, R"({"detail":"The refresh token was used before"})");
	EXPECT_EQ(kind, LobbyErrorKind::NotLoggedIn);
	EXPECT_EQ(_client->GetState(), LobbySessionState::LoggedOut);
	EXPECT_FALSE(_kept.has_value());
	EXPECT_FALSE(_client->GetAccount().has_value());
	EXPECT_EQ(_push->closes, 1);
}

TEST_F(LobbyClientTest, ANetworkFailureOfTheRefreshKeepsTheSession)
{
	Make();
	LogIn();
	_now += 1h;
	std::optional<LobbyErrorKind> kind;
	_client->GetMe([&](auto&& result) { kind = result.error.kind; });
	_http.Fail(online::HttpError::Timeout);
	EXPECT_EQ(kind, LobbyErrorKind::Timeout);
	EXPECT_EQ(_client->GetState(), LobbySessionState::LoggedIn);
	EXPECT_EQ(_kept, "refresh1");
}

TEST_F(LobbyClientTest, ResumeUsesTheTokenOfTheStore)
{
	_kept = "stored";
	Make();
	std::optional<lobby_api::AccountRef> account;
	_client->ResumeSession([&](auto&& result) { account = result.value; });
	EXPECT_EQ(_http.Next().url, "https://bw.example/api/v1/sessions/refresh");
	EXPECT_EQ(_http.NextBody(), R"({"refresh_token":"stored"})");
	_http.Answer(200, k_Rotated);
	ASSERT_TRUE(account.has_value());
	EXPECT_EQ(account->id, 7);
	EXPECT_EQ(_kept, "refresh2");
	EXPECT_EQ(_push->opens, 1);
}

TEST_F(LobbyClientTest, ResumeWithNoTokenFailsWithoutACall)
{
	Make();
	std::optional<LobbyErrorKind> kind;
	_client->ResumeSession([&](auto&& result) { kind = result.error.kind; });
	EXPECT_EQ(kind, LobbyErrorKind::NotLoggedIn);
	EXPECT_TRUE(_http.sent.empty());
	EXPECT_EQ(_client->GetState(), LobbySessionState::LoggedOut);
}

TEST_F(LobbyClientTest, TheBrowserUsesThePathsOfTheOriginAndTheCookie)
{
	Make(lobby_api::ClientKind::Browser);
	_client->ResumeSession([](auto&& /*result*/) {});
	// The cookie holds the refresh token, so the body has none
	EXPECT_EQ(_http.Next().url, "/api/v1/sessions/refresh");
	EXPECT_EQ(_http.NextBody(), "{}");
	_http.Answer(200, R"({"access_token":"a","expires_in":900,"account":{"id":7,"name":"abc"}})");
	EXPECT_EQ(_client->GetState(), LobbySessionState::LoggedIn);
	EXPECT_FALSE(_kept.has_value());
	EXPECT_EQ(_push->lastUrl, "/api/v1/events");

	_client->Login("abc", "pw", [](auto&& /*result*/) {});
	EXPECT_EQ(_http.NextBody(), R"({"client":"browser","name":"abc","password":"pw"})");
}

TEST_F(LobbyClientTest, LogoutForgetsTheTokensAlsoWhenTheLobbyFails)
{
	Make();
	LogIn();
	std::optional<LobbyErrorKind> kind;
	_client->Logout(true, [&](auto&& result) { kind = result.error.kind; });
	EXPECT_EQ(_http.Next().method, HttpMethod::Delete);
	EXPECT_EQ(_http.Next().url, "https://bw.example/api/v1/sessions");
	_http.Fail(online::HttpError::Network);
	EXPECT_EQ(kind, LobbyErrorKind::Network);
	EXPECT_EQ(_client->GetState(), LobbySessionState::LoggedOut);
	EXPECT_FALSE(_kept.has_value());

	LogIn();
	_client->Logout(false, [&](auto&& result) { kind = result.error.kind; });
	EXPECT_EQ(_http.Next().url, "https://bw.example/api/v1/sessions/current");
	_http.Answer(204, "");
	EXPECT_EQ(kind, LobbyErrorKind::None);
}

TEST_F(LobbyClientTest, AnswersOfAnEndedSessionAreIgnored)
{
	Make();
	LogIn();
	std::optional<LobbyErrorKind> kind;
	_now += 1h;
	_client->GetMe([&](auto&& result) { kind = result.error.kind; });
	// The ban comes while the refresh waits
	_push->Deliver(R"({"seq":1,"type":"account.banned","data":{}})");
	_http.Answer(200, k_Rotated);
	EXPECT_EQ(kind, LobbyErrorKind::Cancelled);
	EXPECT_EQ(_client->GetState(), LobbySessionState::LoggedOut);
	// The token of the late answer is not kept
	EXPECT_FALSE(_kept.has_value());
	EXPECT_TRUE(_http.sent.empty());
}

TEST_F(LobbyClientTest, ServerListUsesTheTagAndThePushChanges)
{
	Make();
	LogIn();
	std::optional<lobby_api::ServerList> list;
	_client->GetServers([&](auto&& result) { list = result.value; });
	EXPECT_FALSE(_http.NextHeader("If-None-Match").has_value());
	_http.Answer(200, R"({"servers":[{"id":1,"name":"A","players":1,"max_players":32}]})", {{"etag", "\"v1\""}});
	ASSERT_TRUE(list.has_value());
	EXPECT_EQ(_client->GetKnownServers().size(), 1u);

	list.reset();
	_client->GetServers([&](auto&& result) { list = result.value; });
	EXPECT_EQ(_http.NextHeader("If-None-Match"), "\"v1\"");
	_http.Answer(304, "");
	ASSERT_TRUE(list.has_value());
	EXPECT_EQ(list->servers.size(), 1u);

	_client->WatchServers(true);
	EXPECT_EQ(_push->frames.back(), R"({"channel":"servers","op":"subscribe"})");
	_push->Deliver(R"({"seq":1,"type":"servers.changed","data":{"id":2,"name":"B","players":0,"max_players":32}})");
	_push->Deliver(R"({"seq":2,"type":"servers.changed","data":{"id":1,"name":"A","players":5,"max_players":32}})");
	ASSERT_EQ(_client->GetKnownServers().size(), 2u);
	EXPECT_EQ(_client->GetKnownServers()[0].players, 5);
	_push->Deliver(R"({"seq":3,"type":"servers.changed","data":{"id":2,"removed":true}})");
	EXPECT_EQ(_client->GetKnownServers().size(), 1u);
}

TEST_F(LobbyClientTest, PushEventsGoToTheListenersOnceInOrder)
{
	Make();
	LogIn();
	std::vector<int64_t> seen;
	const auto id = _client->AddEventListener([&](const lobby_api::PushEvent& event) { seen.push_back(event.seq); });
	_push->Deliver(R"({"seq":1,"type":"chat.line","data":{}})");
	_push->Deliver(R"({"seq":1,"type":"chat.line","data":{}})");
	_push->Deliver(R"({"seq":2,"type":"chat.line","data":{}})");
	_push->Deliver("not json");
	EXPECT_EQ(seen, (std::vector<int64_t> {1, 2}));

	_push->Deliver(R"({"type":"ping"})");
	EXPECT_EQ(_push->frames.back(), R"({"op":"pong"})");

	_client->RemoveEventListener(id);
	_push->Deliver(R"({"seq":3,"type":"chat.line","data":{}})");
	EXPECT_EQ(seen.size(), 2u);
}

TEST_F(LobbyClientTest, ALostChannelOpensAgainWithTheLastEvent)
{
	Make();
	LogIn();
	_client->WatchServers(true);
	_push->Deliver(R"({"seq":9,"type":"chat.line","data":{}})");
	_push->Lose(online::PushClose::Lost);
	_client->Update();
	EXPECT_EQ(_push->opens, 1);
	_now += lobby_times::k_FirstReconnectWait;
	_client->Update();
	EXPECT_EQ(_push->opens, 2);
	_push->frames.clear();
	_push->Accept();
	ASSERT_EQ(_push->frames.size(), 2u);
	EXPECT_EQ(_push->frames[0], R"({"last_seq":9,"op":"resume"})");
	EXPECT_EQ(_push->frames[1], R"({"channel":"servers","op":"subscribe"})");
}

TEST_F(LobbyClientTest, TheWaitAfterALossDoublesToTheMost)
{
	Make();
	LogIn();
	auto wait = lobby_times::k_FirstReconnectWait;
	for (int loss = 0; loss < 10; ++loss)
	{
		_push->Lose(online::PushClose::Lost);
		_now += wait - 1s;
		_client->Update();
		const auto opens = _push->opens;
		_now += 1s;
		_client->Update();
		EXPECT_EQ(_push->opens, opens + 1) << loss;
		wait = std::min(wait * 2, lobby_times::k_MostReconnectWait);
	}
}

TEST_F(LobbyClientTest, ABuildWithNoTransportDoesNotTryAgain)
{
	Make();
	LogIn();
	_push->Lose(online::PushClose::Unavailable);
	_now += 1h;
	_client->Update();
	EXPECT_EQ(_push->opens, 1);
}

TEST_F(LobbyClientTest, ABanEndsTheSession)
{
	Make();
	LogIn();
	bool told = false;
	_client->AddEventListener([&](const lobby_api::PushEvent& event) { told = event.type == "account.banned"; });
	_push->Deliver(R"({"seq":1,"type":"account.banned","data":{"reason":"cheat"}})");
	EXPECT_TRUE(told);
	EXPECT_EQ(_client->GetState(), LobbySessionState::LoggedOut);
	EXPECT_FALSE(_kept.has_value());
}

TEST_F(LobbyClientTest, AJoinTicketMustBeForAnEncryptedSocket)
{
	Make();
	LogIn();
	std::optional<LobbyResult<lobby_api::JoinTicket>> got;
	_client->RequestJoinTicket(5, [&](auto&& result) { got = std::move(result); });
	EXPECT_EQ(_http.Next().method, HttpMethod::Post);
	EXPECT_EQ(_http.Next().url, "https://bw.example/api/v1/servers/5/tickets");
	_http.Answer(201, R"({"url":"ws://bw.example/game/5","ticket":"t","expires_in":60})");
	ASSERT_TRUE(got.has_value());
	EXPECT_EQ(got->error.kind, LobbyErrorKind::AddressRefused);

	_client->RequestJoinTicket(5, [&](auto&& result) { got = std::move(result); });
	_http.Answer(201, R"({"url":"wss://bw.example/game/5","ticket":"t","expires_in":60})");
	ASSERT_TRUE(got->Ok());
	EXPECT_EQ(got->value->ticket, "t");
}

TEST_F(LobbyClientTest, ClansRankingAndPlayerUseTheirPaths)
{
	Make();
	LogIn();
	_client->GetClan(3, [](auto&& /*result*/) {});
	EXPECT_EQ(_http.Next().url, "https://bw.example/api/v1/clans/3");
	_http.Answer(200, R"({"id":3,"name":"A"})");
	_client->GetPlayerRanking(2, [](auto&& /*result*/) {});
	EXPECT_EQ(_http.Next().url, "https://bw.example/api/v1/rankings/players?page=2");
	_http.Answer(200, R"({"page":2,"pages":2,"entries":[]})");
	_client->GetClanRanking(1, [](auto&& /*result*/) {});
	EXPECT_EQ(_http.Next().url, "https://bw.example/api/v1/rankings/clans?page=1");
	_http.Answer(200, R"({"page":1,"pages":1,"entries":[]})");
	_client->GetPlayer(4, [](auto&& /*result*/) {});
	EXPECT_EQ(_http.Next().url, "https://bw.example/api/v1/players/4");
	_http.Answer(200, R"({"id":4,"name":"x"})");
}

TEST_F(LobbyClientTest, TheLogoMustBeABmpFile)
{
	Make();
	LogIn();
	std::optional<LobbyResult<std::vector<uint8_t>>> got;
	_client->GetClanLogo(3, [&](auto&& result) { got = std::move(result); });
	EXPECT_EQ(_http.Next().url, "https://bw.example/api/v1/clans/3/logo");
	_http.Answer(200, "BM\x01\x02");
	ASSERT_TRUE(got->Ok());
	EXPECT_EQ(got->value->size(), 4u);
	_client->GetClanLogo(3, [&](auto&& result) { got = std::move(result); });
	_http.Answer(200, "<html>");
	EXPECT_EQ(got->error.kind, LobbyErrorKind::BadAnswer);
}

TEST_F(LobbyClientTest, WeatherAsksForTheDay)
{
	Make();
	LogIn();
	std::optional<lobby_api::RealWeather> weather;
	_client->GetWeather({.year = 2026, .month = 10, .day = 2}, [&](auto&& result) { weather = result.value; });
	EXPECT_EQ(_http.Next().url, "https://bw.example/api/v1/me/weather?date=2026-10-02");
	_http.Answer(200, R"({"temperature":12,"visibility":80,"type":2,"city":"Guildford","area":"Surrey","country":"gb"})");
	ASSERT_TRUE(weather.has_value());
	EXPECT_EQ(weather->type, 2);
}

TEST_F(LobbyClientTest, CreatureUploadSendsTheThreeFiles)
{
	Make();
	LogIn();
	std::optional<lobby_api::CreatureUploadReply> reply;
	_client->UploadCreature(
	    lobby_api::CreatureUpload {
	        .package = {'L', 'H'},
	        .mind = {1},
	        .mindFileName = "_A_b_c.chl",
	        .physique = {2},
	        .physiqueFileName = "_A_b_c.phy",
	    },
	    [&](auto&& result) { reply = result.value; });
	EXPECT_EQ(_http.Next().url, "https://bw.example/api/v1/me/creatures");
	const auto type = _http.NextHeader("Content-Type");
	ASSERT_TRUE(type.has_value());
	EXPECT_TRUE(type->starts_with("multipart/form-data; boundary=openblack-"));
	const auto body = _http.NextBody();
	EXPECT_NE(body.find(R"(name="creature"; filename="creature.lhp")"), std::string::npos);
	EXPECT_NE(body.find(R"(name="mind"; filename="_A_b_c.chl")"), std::string::npos);
	EXPECT_NE(body.find(R"(name="physique"; filename="_A_b_c.phy")"), std::string::npos);
	_http.Answer(201, R"({"url":"https://bw.example/creatures/abc"})");
	ASSERT_TRUE(reply.has_value());
	EXPECT_EQ(reply->url, "https://bw.example/creatures/abc");
}

TEST_F(LobbyClientTest, ErrorsOfTheLobbyHaveTheirKinds)
{
	EXPECT_EQ(LobbyErrorOf(HttpResponse {.status = 200}).kind, LobbyErrorKind::None);
	EXPECT_EQ(LobbyErrorOf(HttpResponse {.status = 404}).kind, LobbyErrorKind::Refused);
	EXPECT_EQ(LobbyErrorOf(HttpResponse {.status = 503}).kind, LobbyErrorKind::ServerFault);
	EXPECT_EQ(LobbyErrorOf(HttpResponse {.error = online::HttpError::Tls}).kind, LobbyErrorKind::Tls);
	EXPECT_EQ(LobbyErrorOf(HttpResponse {.error = online::HttpError::AddressRefused}).kind, LobbyErrorKind::AddressRefused);
	const std::string_view problem = R"({"detail":"Slow down"})";
	const auto limited =
	    LobbyErrorOf(HttpResponse {.status = 429, .headers = {{"retry-after", "5"}}, .body = {problem.begin(), problem.end()}});
	EXPECT_EQ(limited.kind, LobbyErrorKind::RateLimited);
	EXPECT_EQ(limited.retryAfterSeconds, 5);
	EXPECT_EQ(limited.detail, "Slow down");
}

TEST_F(LobbyClientTest, CallsWithNoSessionFailWithoutACall)
{
	Make();
	std::optional<LobbyErrorKind> kind;
	_client->GetMe([&](auto&& result) { kind = result.error.kind; });
	EXPECT_EQ(kind, LobbyErrorKind::NotLoggedIn);
	EXPECT_TRUE(_http.sent.empty());
}
