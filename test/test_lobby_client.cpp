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
#include <fmt/format.h>
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

/// The test starts at 10:00, and the access token of the login lasts until 10:15
constexpr std::string_view k_Session =
    R"({"token_type":"Bearer","access_token":"access1","access_token_expires":"2026-10-22T10:15:00.000Z",)"
    R"("refresh_token":"refresh1","refresh_token_expires":"2026-11-21T10:00:00.000Z","account":{"id":7,"name":"abc"}})";
constexpr std::string_view k_Rotated =
    R"({"token_type":"Bearer","access_token":"access2","access_token_expires":"2026-10-22T12:00:00.000Z",)"
    R"("refresh_token":"refresh2","refresh_token_expires":"2026-11-21T10:00:00.000Z","account":{"id":7,"name":"abc"}})";
/// The first frame of the push channel
constexpr std::string_view k_Hello = R"({"type":"hello","data":{"epoch":"e1","seq":0,"account":{"id":7,"name":"abc"}}})";

/// A server as the lobby lists it
std::string ServerJson(std::string_view id, int32_t players)
{
	return fmt::format(R"({{"id":"{}","hostname":"Room {}","land":"Test","gamemode":"openstaging","locked":false,)"
	                   R"("password":false,"numplayers":{},"maxplayers":32,"gamever":"1.0","infoversion":1,)"
	                   R"("creatureversion":1,"oosversion":1,"players":[],"url":"wss://bw.example/game/{}",)"
	                   R"("updated":"2026-10-22T10:00:00Z"}})",
	                   id, id, players, id);
}
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
		_push->Deliver(k_Hello);
	}

	FakeHttp _http;
	FakePush* _push {nullptr};
	std::optional<std::string> _kept;
	/// 2026-10-22T10:00:00Z
	std::chrono::system_clock::time_point _now {std::chrono::seconds {1792663200}};
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
	EXPECT_EQ(_http.NextBody(), R"({"cookie":false,"name":"abc","password":"secret"})");
	_http.Answer(201, k_Session);

	ASSERT_TRUE(account.has_value());
	EXPECT_EQ(account->name, "abc");
	EXPECT_EQ(_client->GetState(), LobbySessionState::LoggedIn);
	EXPECT_EQ(_kept, "refresh1");
	EXPECT_EQ(_push->opens, 1);
	EXPECT_EQ(_push->lastUrl, "wss://bw.example/api/v1/events");
	EXPECT_EQ(_push->lastToken, "access1");
	// The client waits for "hello", and with no channel it asks for nothing
	_push->Accept();
	_push->Deliver(k_Hello);
	EXPECT_TRUE(_push->frames.empty());
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
	_http.Answer(200, R"({"access_token":"a","access_token_expires":"2026-10-22T10:15:00Z","refresh_token":null,)"
	                  R"("refresh_token_expires":"2026-11-21T10:00:00Z","account":{"id":7,"name":"abc"}})");
	EXPECT_EQ(_client->GetState(), LobbySessionState::LoggedIn);
	EXPECT_FALSE(_kept.has_value());
	EXPECT_EQ(_push->lastUrl, "/api/v1/events");

	_client->Login("abc", "pw", [](auto&& /*result*/) {});
	EXPECT_EQ(_http.NextBody(), R"({"cookie":true,"name":"abc","password":"pw"})");
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
	_push->Deliver(R"({"seq":1,"type":"account.banned","time":"2026-10-22T10:00:01Z","data":{}})");
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
	_http.Answer(200, R"({"servers":[)" + ServerJson("a", 1) + "]}", {{"etag", "\"v1\""}});
	ASSERT_TRUE(list.has_value());
	EXPECT_EQ(_client->GetKnownServers().size(), 1u);

	list.reset();
	_client->GetServers([&](auto&& result) { list = result.value; });
	EXPECT_EQ(_http.NextHeader("If-None-Match"), "\"v1\"");
	_http.Answer(304, "");
	ASSERT_TRUE(list.has_value());
	EXPECT_EQ(list->servers.size(), 1u);

	_client->WatchServers(true);
	EXPECT_EQ(_push->frames.back(), R"({"channel":"servers","type":"subscribe"})");
	_push->Deliver(R"({"seq":1,"type":"servers.changed","time":"2026-10-22T10:00:01Z","data":{"server":)" + ServerJson("b", 0) +
	               "}}");
	_push->Deliver(R"({"seq":2,"type":"servers.changed","time":"2026-10-22T10:00:02Z","data":{"server":)" + ServerJson("a", 5) +
	               "}}");
	ASSERT_EQ(_client->GetKnownServers().size(), 2u);
	EXPECT_EQ(_client->GetKnownServers()[0].numplayers, 5);
	_push->Deliver(R"({"seq":3,"type":"servers.removed","time":"2026-10-22T10:00:03Z","data":{"id":"b","reason":"timeout"}})");
	EXPECT_EQ(_client->GetKnownServers().size(), 1u);
	_client->WatchServers(false);
	EXPECT_EQ(_push->frames.back(), R"({"channel":"servers","type":"unsubscribe"})");
}

TEST_F(LobbyClientTest, PushEventsGoToTheListenersOnceInOrder)
{
	Make();
	LogIn();
	std::vector<std::string> seen;
	const auto id = _client->AddEventListener([&](const lobby_api::PushEvent& event) {
		seen.push_back(event.type + (event.seq.has_value() ? std::to_string(*event.seq) : std::string {}));
	});
	_push->Deliver(R"({"seq":1,"type":"chat.line","time":"2026-10-22T10:00:01Z","data":{}})");
	_push->Deliver(R"({"seq":1,"type":"chat.line","time":"2026-10-22T10:00:01Z","data":{}})");
	_push->Deliver(R"({"seq":5,"type":"chat.line","time":"2026-10-22T10:00:02Z","data":{}})");
	_push->Deliver(R"({"type":"error","data":{"code":"no_access","detail":"Join the server's room first."}})");
	_push->Deliver("not json");
	EXPECT_EQ(seen, (std::vector<std::string> {"chat.line1", "chat.line5", "error"}));

	_client->RemoveEventListener(id);
	_push->Deliver(R"({"seq":6,"type":"chat.line","time":"2026-10-22T10:00:03Z","data":{}})");
	EXPECT_EQ(seen.size(), 3u);
}

TEST_F(LobbyClientTest, ALostChannelOpensAgainWithTheLastEvent)
{
	Make();
	LogIn();
	_client->WatchServers(true);
	_client->WatchChannel("lobby", true);
	_push->Deliver(R"({"seq":9,"type":"chat.line","time":"2026-10-22T10:00:01Z","data":{}})");
	_push->Lose(online::PushClose::Lost);
	_client->Update();
	EXPECT_EQ(_push->opens, 1);
	_now += lobby_times::k_FirstReconnectWait;
	_client->Update();
	EXPECT_EQ(_push->opens, 2);
	_push->frames.clear();
	_push->Accept();
	EXPECT_TRUE(_push->frames.empty());
	_push->Deliver(R"({"type":"hello","data":{"epoch":"e1","seq":12,"account":{"id":7,"name":"abc"}}})");
	ASSERT_EQ(_push->frames.size(), 1u);
	EXPECT_EQ(_push->frames[0], R"({"channels":["servers","lobby"],"epoch":"e1","seq":9,"type":"resume"})");

	// The lobby lost the events: the client continues from the lobby's number
	_push->Deliver(R"({"type":"resync","data":{"epoch":"e2","seq":50}})");
	std::vector<int64_t> seen;
	_client->AddEventListener([&](const lobby_api::PushEvent& event) { seen.push_back(event.seq.value_or(0)); });
	_push->Deliver(R"({"seq":45,"type":"chat.line","time":"2026-10-22T10:00:02Z","data":{}})");
	_push->Deliver(R"({"seq":51,"type":"chat.line","time":"2026-10-22T10:00:03Z","data":{}})");
	EXPECT_EQ(seen, (std::vector<int64_t> {51}));
}

TEST_F(LobbyClientTest, AFirstConnectionSubscribesTheChannels)
{
	Make();
	_client->WatchServers(true);
	_client->Login("abc", "secret", [](auto&& /*result*/) {});
	_http.Answer(201, k_Session);
	_push->Accept();
	EXPECT_TRUE(_push->frames.empty());
	_push->Deliver(k_Hello);
	ASSERT_EQ(_push->frames.size(), 1u);
	EXPECT_EQ(_push->frames[0], R"({"channel":"servers","type":"subscribe"})");
}

TEST_F(LobbyClientTest, ASignOutOrABanOfTheChannelEndsTheSession)
{
	Make();
	LogIn();
	_push->Lose(online::PushClose::SignedOut);
	EXPECT_EQ(_client->GetState(), LobbySessionState::LoggedOut);
	EXPECT_FALSE(_kept.has_value());
	LogIn();
	_push->Lose(online::PushClose::Banned);
	EXPECT_EQ(_client->GetState(), LobbySessionState::LoggedOut);
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
	_push->Deliver(R"({"seq":1,"type":"account.banned","time":"2026-10-22T10:00:01Z","data":{"reason":"cheat"}})");
	EXPECT_TRUE(told);
	EXPECT_EQ(_client->GetState(), LobbySessionState::LoggedOut);
	EXPECT_FALSE(_kept.has_value());
}

TEST_F(LobbyClientTest, AJoinTicketMustBeForAnEncryptedSocket)
{
	Make();
	LogIn();
	std::optional<LobbyResult<lobby_api::JoinTicket>> got;
	_client->RequestJoinTicket("eden", game_socket::Role::God, [&](auto&& result) { got = std::move(result); });
	EXPECT_EQ(_http.Next().method, HttpMethod::Post);
	EXPECT_EQ(_http.Next().url, "https://bw.example/api/v1/servers/eden/tickets");
	EXPECT_EQ(_http.NextBody(), R"({"role":"god"})");
	_http.Answer(201, R"({"url":"ws://bw.example/game/eden","ticket":"t","expires":"2026-10-22T10:01:00Z"})");
	ASSERT_TRUE(got.has_value());
	EXPECT_EQ(got->error.kind, LobbyErrorKind::AddressRefused);

	_client->RequestJoinTicket("eden", game_socket::Role::Observer, [&](auto&& result) { got = std::move(result); });
	EXPECT_EQ(_http.NextBody(), R"({"role":"observer"})");
	_http.Answer(201,
	             R"({"url":"wss://bw.example/game/eden","ticket":"t","expires":"2026-10-22T10:01:00Z","role":"observer"})");
	ASSERT_TRUE(got->Ok());
	EXPECT_EQ(got->value->ticket, "t");
	EXPECT_EQ(got->value->role, "observer");
}

TEST_F(LobbyClientTest, AFullRoomGivesTheCodeOfTheProblem)
{
	Make();
	LogIn();
	std::optional<LobbyResult<lobby_api::JoinTicket>> got;
	_client->RequestJoinTicket("eden", game_socket::Role::God, [&](auto&& result) { got = std::move(result); });
	_http.Answer(409,
	             R"({"type":"about:blank","title":"Conflict","status":409,)"
	             R"("detail":"The room is full. You can still join as an observer.","instance":"/x","code":"server_full"})");
	ASSERT_TRUE(got.has_value());
	EXPECT_EQ(got->error.kind, LobbyErrorKind::Refused);
	EXPECT_EQ(got->error.code, "server_full");
}

TEST_F(LobbyClientTest, TheSessionSaysWhenTheAccountNeedsAnEmailAddress)
{
	Make();
	EXPECT_FALSE(_client->IsEmailRequired());
	_client->Login("abc", "secret", [](auto&& /*result*/) {});
	_http.Answer(201, R"({"token_type":"Bearer","access_token":"access1","access_token_expires":"2026-10-22T10:15:00.000Z",)"
	                  R"("refresh_token":"refresh1","refresh_token_expires":"2026-11-21T10:00:00.000Z",)"
	                  R"("account":{"id":7,"name":"abc"},"email_required":true,"second_factor":"not_needed"})");
	EXPECT_TRUE(_client->IsEmailRequired());
	_client->Logout(false, [](auto&& /*result*/) {});
	_http.Answer(204, "");
	EXPECT_FALSE(_client->IsEmailRequired());
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
