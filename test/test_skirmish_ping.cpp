/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Systems/LobbyClientSystemInterface.h"
#include "Online/SkirmishPing.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{

/// A lobby that counts the messages of a decided skirmish and keeps the callback of a resume of the session
class FakeSender final: public LobbyClientSystemInterface
{
public:
	void Update() override {}
	[[nodiscard]] bool IsConfigured() const override { return configured; }
	[[nodiscard]] LobbySessionState GetState() const override { return state; }
	[[nodiscard]] std::optional<lobby_api::AccountRef> GetAccount() const override { return std::nullopt; }
	[[nodiscard]] bool IsPushOpen() const override { return false; }
	[[nodiscard]] bool IsEmailRequired() const override { return false; }
	void Login(std::string, std::string, LobbyCallback<lobby_api::AccountRef>) override {}
	void ResumeSession(LobbyCallback<lobby_api::AccountRef> callback) override
	{
		++resumes;
		resume = std::move(callback);
	}
	void Logout(bool, LobbyCallback<LobbyDone>) override {}
	void GetMe(LobbyCallback<lobby_api::Me>) override {}
	void GetServers(LobbyCallback<lobby_api::ServerList>) override {}
	[[nodiscard]] std::span<const lobby_api::ServerEntry> GetKnownServers() const override { return {}; }
	void WatchServers(bool) override {}
	void WatchChannel(std::string, bool) override {}
	void RequestJoinTicket(std::string, game_socket::Role, LobbyCallback<lobby_api::JoinTicket>) override {}
	void SendChatLine(std::string, std::string, LobbyCallback<LobbyDone>) override {}
	void UseClan(std::string, std::optional<int64_t>, LobbyCallback<lobby_api::UsedClan>) override {}
	void CheckAccountName(std::string, LobbyCallback<lobby_api::AccountNameCheck>) override {}
	void GetMyClans(LobbyCallback<lobby_api::ClanList>) override {}
	void GetClan(int64_t, LobbyCallback<lobby_api::ClanDetails>) override {}
	void GetClanLogo(int64_t, LobbyCallback<std::vector<uint8_t>>) override {}
	void GetPlayerRanking(int32_t, LobbyCallback<lobby_api::RankingPage>) override {}
	void GetClanRanking(int32_t, LobbyCallback<lobby_api::RankingPage>) override {}
	void GetPlayer(int64_t, LobbyCallback<lobby_api::PlayerDetails>) override {}
	void GetWeather(lobby_api::Date, LobbyCallback<lobby_api::RealWeather>) override {}
	void UploadCreature(lobby_api::CreatureUpload, LobbyCallback<lobby_api::CreatureUploadReply>) override {}
	void SendSkirmishPing(LobbyCallback<LobbyDone> callback) override
	{
		++pings;
		// The lobby fails each call when it cannot get the message
		if (failPings)
		{
			callback(LobbyResult<LobbyDone> {.value = std::nullopt, .error = {.kind = LobbyErrorKind::Network}});
			return;
		}
		callback(LobbyResult<LobbyDone> {.value = LobbyDone {}, .error = {}});
	}
	uint32_t AddEventListener(LobbyEventListener) override { return 0; }
	void RemoveEventListener(uint32_t) override {}

	/// The lobby answers the resume of the session
	void AnswerResume(bool ok)
	{
		ASSERT_TRUE(resume);
		auto callback = std::move(resume);
		if (ok)
		{
			state = LobbySessionState::LoggedIn;
			callback(LobbyResult<lobby_api::AccountRef> {.value = lobby_api::AccountRef {.id = 7, .name = "abc"}, .error = {}});
			return;
		}
		state = LobbySessionState::LoggedOut;
		callback(LobbyResult<lobby_api::AccountRef> {.value = std::nullopt, .error = {.kind = LobbyErrorKind::NotLoggedIn}});
	}

	bool configured {true};
	bool failPings {false};
	LobbySessionState state {LobbySessionState::LoggedIn};
	int pings {0};
	int resumes {0};
	LobbyCallback<lobby_api::AccountRef> resume;
};

} // namespace

TEST(SkirmishPing, APlayerWithASessionSendsOneMessageForEachEventThatDecides)
{
	FakeSender lobby;
	online::SendSkirmishPings(lobby, 1);
	EXPECT_EQ(lobby.pings, 1);
	online::SendSkirmishPings(lobby, 2);
	EXPECT_EQ(lobby.pings, 3);
	EXPECT_EQ(lobby.resumes, 0);
}

TEST(SkirmishPing, NoEventThatDecidesSendsNothing)
{
	FakeSender lobby;
	lobby.state = LobbySessionState::LoggedOut;
	online::SendSkirmishPings(lobby, 0);
	EXPECT_EQ(lobby.pings, 0);
	EXPECT_EQ(lobby.resumes, 0);
}

TEST(SkirmishPing, AGameWithNoLobbySendsNothing)
{
	FakeSender lobby;
	lobby.configured = false;
	online::SendSkirmishPings(lobby, 1);
	EXPECT_EQ(lobby.pings, 0);
	EXPECT_EQ(lobby.resumes, 0);
}

TEST(SkirmishPing, AKeptSessionContinuesBeforeTheMessage)
{
	FakeSender lobby;
	lobby.state = LobbySessionState::LoggedOut;
	online::SendSkirmishPings(lobby, 1);
	EXPECT_EQ(lobby.resumes, 1);
	EXPECT_EQ(lobby.pings, 0);
	lobby.AnswerResume(true);
	EXPECT_EQ(lobby.pings, 1);
}

TEST(SkirmishPing, APlayerWithNoSessionSendsNothing)
{
	FakeSender lobby;
	lobby.state = LobbySessionState::LoggedOut;
	online::SendSkirmishPings(lobby, 1);
	lobby.AnswerResume(false);
	EXPECT_EQ(lobby.pings, 0);
}

TEST(SkirmishPing, AFailedMessageIsNotSentAgain)
{
	FakeSender lobby;
	lobby.failPings = true;
	online::SendSkirmishPings(lobby, 1);
	EXPECT_EQ(lobby.pings, 1);
	EXPECT_EQ(lobby.resumes, 0);
}
