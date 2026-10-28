/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <deque>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <LobbyApiJson.h>
#include <gtest/gtest.h>

#include "Online/HttpClientInterface.h"
#include "Server/LobbyBans.h"

using namespace openblack;
using namespace openblack::server;

namespace
{

/// A lobby that keeps each request until the test answers it
class FakeLobby final: public online::HttpClientInterface
{
public:
	online::HttpRequestId Send(online::HttpRequest request, online::HttpCallback callback) override
	{
		sent.emplace_back(std::move(request), std::move(callback));
		return static_cast<online::HttpRequestId>(sent.size());
	}
	void Cancel(online::HttpRequestId /*id*/) override {}
	void Poll() override {}
	[[nodiscard]] size_t Pending() const override { return sent.size(); }

	void Answer(int32_t status, std::string_view body)
	{
		ASSERT_FALSE(sent.empty());
		auto next = std::move(sent.front());
		sent.pop_front();
		next.second(online::HttpResponse {
		    .error = online::HttpError::None, .status = status, .headers = {}, .body = {body.begin(), body.end()}});
	}

	std::deque<std::pair<online::HttpRequest, online::HttpCallback>> sent;
};

online::HttpResponse Response(int32_t status, std::string_view body)
{
	return {.error = online::HttpError::None, .status = status, .headers = {}, .body = {body.begin(), body.end()}};
}

/// The answer of the lobby with the bans of two accounts, as bwgame-service sends it
constexpr std::string_view k_TwoBans =
    R"({"bans":[{"account":{"id":101,"name":"Spammer"},"reason":"spam","by":"Admin","created":"2026-10-26T10:00:00Z",)"
    R"("expires":null},{"account":{"id":7,"name":"Cheat"},"reason":"","by":"","created":"2026-10-26T10:00:00Z",)"
    R"("expires":"2026-10-27T10:00:00Z"}]})";

LockstepHost::Seat SeatOf(LockstepHost::SeatState state, std::string account)
{
	LockstepHost::Seat seat;
	seat.state = state;
	seat.account = std::move(account);
	return seat;
}

} // namespace

TEST(LobbyBans, TheListOfTheLobbyGivesTheBannedAccounts)
{
	const auto list = lobby_api::ParseServerBanList(k_TwoBans);
	ASSERT_TRUE(list.has_value());
	ASSERT_EQ(list->bans.size(), 2u);
	EXPECT_EQ(list->bans[0].account.id, 101);
	EXPECT_EQ(list->bans[0].account.name, "Spammer");
	EXPECT_EQ(list->bans[0].reason, "spam");
	EXPECT_EQ(lobby_bans::BannedAccounts(Response(200, k_TwoBans)), (std::set<int64_t> {7, 101}));
	EXPECT_EQ(lobby_bans::BannedAccounts(Response(200, R"({"bans":[]})")), std::set<int64_t> {});
}

TEST(LobbyBans, AnAnswerThatIsNotAListGivesNoBans)
{
	EXPECT_FALSE(lobby_bans::BannedAccounts(Response(403, R"({"bans":[]})")).has_value());
	EXPECT_FALSE(lobby_bans::BannedAccounts(Response(200, "not json")).has_value());
	EXPECT_FALSE(lobby_bans::BannedAccounts(Response(200, R"({"servers":[]})")).has_value());
	EXPECT_FALSE(lobby_bans::BannedAccounts(Response(200, R"({"bans":[{"reason":"no account"}]})")).has_value());
	auto error = Response(0, {});
	error.error = online::HttpError::Network;
	EXPECT_FALSE(lobby_bans::BannedAccounts(error).has_value());
}

TEST(LobbyBans, TheAddressOfTheBansFollowsTheLobbyAndTheServer)
{
	EXPECT_EQ(lobby_bans::BansAddress("https://lobby.test/api/v1/", "w1"), "https://lobby.test/api/v1/servers/w1/bans");
	EXPECT_EQ(lobby_bans::BansAddress("https://lobby.test/api/v1", "w1"), "https://lobby.test/api/v1/servers/w1/bans");
}

TEST(LobbyBans, OnlyTheAccountIdOfABanMatches)
{
	const std::set<int64_t> banned {7, 101};
	EXPECT_TRUE(lobby_bans::IsBanned("101", banned));
	EXPECT_FALSE(lobby_bans::IsBanned("102", banned));
	EXPECT_FALSE(lobby_bans::IsBanned("1010", banned));
	// A subject that is not an account id is never banned
	EXPECT_FALSE(lobby_bans::IsBanned("alice", banned));
	EXPECT_FALSE(lobby_bans::IsBanned("", banned));
}

TEST(LobbyBans, ABanTakesTheSlotsThatTheAccountHolds)
{
	const std::vector<LockstepHost::Seat> seats {
	    SeatOf(LockstepHost::SeatState::Taken, "100"), SeatOf(LockstepHost::SeatState::Taken, "101"),
	    SeatOf(LockstepHost::SeatState::Open, ""),     SeatOf(LockstepHost::SeatState::Away, "7"),
	    SeatOf(LockstepHost::SeatState::Closed, "7"),
	};
	EXPECT_EQ(lobby_bans::BannedSeats(seats, {7, 101}), (std::vector<uint8_t> {1, 3}));
	EXPECT_TRUE(lobby_bans::BannedSeats(seats, {}).empty());
}

TEST(LobbyBans, TheReaderAsksOnceAtATimeAndKeepsTheLastBansAfterAFailure)
{
	FakeLobby lobby;
	std::vector<std::set<int64_t>> heard;
	lobby_bans::Reader reader(lobby, "https://lobby.test/api/v1/servers/w1/bans",
	                          [&heard](std::set<int64_t> banned) { heard.push_back(std::move(banned)); });
	reader.Fetch();
	reader.Fetch();
	ASSERT_EQ(lobby.sent.size(), 1u);
	EXPECT_EQ(lobby.sent.front().first.method, online::HttpMethod::Get);
	EXPECT_EQ(lobby.sent.front().first.url, "https://lobby.test/api/v1/servers/w1/bans");
	EXPECT_TRUE(lobby.sent.front().first.headers.empty());
	EXPECT_TRUE(reader.Fetching());
	lobby.Answer(200, k_TwoBans);
	EXPECT_FALSE(reader.Fetching());
	ASSERT_EQ(heard.size(), 1u);
	EXPECT_EQ(heard.back(), (std::set<int64_t> {7, 101}));

	// The lobby does not answer with a list: the listener hears nothing, so the server keeps the last bans
	reader.Fetch();
	lobby.Answer(503, "");
	EXPECT_EQ(heard.size(), 1u);

	// A lifted ban is not in the next list
	reader.Fetch();
	lobby.Answer(200, R"({"bans":[{"account":{"id":7,"name":"Cheat"},"reason":""}]})");
	ASSERT_EQ(heard.size(), 2u);
	EXPECT_EQ(heard.back(), std::set<int64_t> {7});
}
