/******************************************************************************
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

#include <MindFile.h>
#include <gtest/gtest.h>

#include "ECS/Systems/ClanCreatureSystemInterface.h"
#include "ECS/Systems/LobbyClientSystemInterface.h"

// Enable this define because the test makes the system itself
#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/ClanCreatureSystem.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{

/// A lobby that keeps the clan creature calls, and answers when the test says
class FakeLobby final: public LobbyClientSystemInterface
{
public:
	void Update() override {}
	[[nodiscard]] bool IsConfigured() const override { return true; }
	[[nodiscard]] LobbySessionState GetState() const override { return LobbySessionState::LoggedIn; }
	[[nodiscard]] std::optional<lobby_api::AccountRef> GetAccount() const override { return std::nullopt; }
	[[nodiscard]] bool IsPushOpen() const override { return false; }
	[[nodiscard]] bool IsEmailRequired() const override { return false; }
	void Login(std::string, std::string, LobbyCallback<lobby_api::AccountRef>) override {}
	void ResumeSession(LobbyCallback<lobby_api::AccountRef>) override {}
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
	void GetClanCreature(int64_t clanId, LobbyCallback<lobby_api::ClanCreatureReply> callback) override
	{
		asked.push_back(clanId);
		pending.push_back(std::move(callback));
	}
	void PutClanCreature(int64_t clanId, lobby_api::ClanCreature creature, LobbyCallback<LobbyDone> callback) override
	{
		sent.emplace_back(clanId, std::move(creature));
		callback({.value = LobbyDone {}, .error = {}});
	}
	void GetPlayerRanking(int32_t, LobbyCallback<lobby_api::RankingPage>) override {}
	void GetClanRanking(int32_t, LobbyCallback<lobby_api::RankingPage>) override {}
	void GetPlayer(int64_t, LobbyCallback<lobby_api::PlayerDetails>) override {}
	void GetPlayerDetailLines(int64_t, std::string, LobbyCallback<lobby_api::PlayerDetailLines>) override {}
	void GetWeather(lobby_api::Date, LobbyCallback<lobby_api::RealWeather>) override {}
	void UploadCreature(lobby_api::CreatureUpload, LobbyCallback<lobby_api::CreatureUploadReply>) override {}
	void SendSkirmishPing(LobbyCallback<LobbyDone>) override {}
	uint32_t AddEventListener(LobbyEventListener) override { return 0; }
	void RemoveEventListener(uint32_t) override {}

	/// Answers the call at the index
	void Answer(size_t call, LobbyResult<lobby_api::ClanCreatureReply> result)
	{
		auto callback = std::move(pending.at(call));
		callback(std::move(result));
	}

	std::vector<int64_t> asked;
	std::vector<LobbyCallback<lobby_api::ClanCreatureReply>> pending;
	std::vector<std::pair<int64_t, lobby_api::ClanCreature>> sent;
};

struct ClanCreatureTest: public ::testing::Test
{
	FakeLobby lobby;
	ClanCreatureSystem system {[this]() -> LobbyClientSystemInterface* { return &lobby; }};
	const std::vector<uint8_t> mind = creaturemind::Write(creaturemind::MindFileData {});
	const std::vector<uint8_t> userData {7, 8, 9};

	[[nodiscard]] lobby_api::ClanCreatureReply Stored() const
	{
		return {.kind = lobby_api::ClanCreatureKind::Stored,
		        .creature = lobby_api::ClanCreature {.alignment = -0.5f, .mind = mind, .physique = userData},
		        .make = std::nullopt};
	}

	static LobbyResult<lobby_api::ClanCreatureReply> Ok(lobby_api::ClanCreatureReply reply)
	{
		return {.value = std::move(reply), .error = {}};
	}
};

} // namespace

TEST_F(ClanCreatureTest, AClanAsksTheLobbyForItsCreature)
{
	system.UseClan(4);
	EXPECT_EQ(lobby.asked, std::vector<int64_t> {4});
	EXPECT_EQ(system.UsedClan(), 4);
	// No answer yet: the god brings the creature of the player's profile
	EXPECT_FALSE(system.StartCreature().has_value());
}

TEST_F(ClanCreatureTest, TheSameClanAgainKeepsTheAnswer)
{
	system.UseClan(4);
	lobby.Answer(0, Ok(Stored()));
	system.UseClan(4);
	EXPECT_EQ(lobby.asked.size(), 1u);
	EXPECT_TRUE(system.StartCreature().has_value());
}

TEST_F(ClanCreatureTest, NoClanAsksForNothing)
{
	system.UseClan(std::nullopt);
	EXPECT_TRUE(lobby.asked.empty());
	EXPECT_FALSE(system.StartCreature().has_value());
}

TEST_F(ClanCreatureTest, TheStoredCreatureStartsWithItsFileAndAlignment)
{
	system.UseClan(4);
	lobby.Answer(0, Ok(Stored()));
	const auto start = system.StartCreature();
	ASSERT_TRUE(start.has_value());
	EXPECT_EQ(start->file, mind);
	EXPECT_EQ(start->alignment, -0.5f);
}

TEST_F(ClanCreatureTest, ANewCreatureStartsWithTheOrderAndNoAlignment)
{
	system.UseClan(4);
	lobby.Answer(0, Ok({.kind = lobby_api::ClanCreatureKind::Create,
	                    .creature = std::nullopt,
	                    .make = lobby_api::NewClanCreature {.type = 2, .name = "Stripes"}}));
	const auto start = system.StartCreature();
	ASSERT_TRUE(start.has_value());
	const std::string text = "CLANCREATECREATURE02Stripes";
	std::vector<uint8_t> order(text.begin(), text.end());
	order.push_back(0);
	EXPECT_EQ(start->file, order);
	EXPECT_EQ(start->alignment, 0.0f);
}

TEST_F(ClanCreatureTest, AClanWithNoCreatureStartsWithTheProfileCreature)
{
	system.UseClan(4);
	lobby.Answer(0, Ok({.kind = lobby_api::ClanCreatureKind::None, .creature = std::nullopt, .make = std::nullopt}));
	EXPECT_FALSE(system.StartCreature().has_value());
}

TEST_F(ClanCreatureTest, AFailedCallKeepsNothing)
{
	system.UseClan(4);
	lobby.Answer(0, {.value = std::nullopt, .error = {.kind = LobbyErrorKind::Network}});
	EXPECT_FALSE(system.StartCreature().has_value());
}

TEST_F(ClanCreatureTest, TheAnswerForAnEarlierClanIsOld)
{
	system.UseClan(4);
	system.UseClan(5);
	lobby.Answer(0, Ok(Stored()));
	EXPECT_FALSE(system.StartCreature().has_value());
	lobby.Answer(1, Ok(Stored()));
	EXPECT_TRUE(system.StartCreature().has_value());
	// No clan forgets the answer
	system.UseClan(std::nullopt);
	EXPECT_FALSE(system.StartCreature().has_value());
}

TEST_F(ClanCreatureTest, TheEndSendsTheKeptFileWithTheGodsAlignmentAndTheUserData)
{
	system.UseClan(4);
	lobby.Answer(0, Ok(Stored()));
	auto now = mind;
	system.KeepCreatureFile(now);
	EXPECT_EQ(system.SendAtGameEnd(0.25f), ClanUpload::Sent);
	ASSERT_EQ(lobby.sent.size(), 1u);
	EXPECT_EQ(lobby.sent[0].first, 4);
	EXPECT_EQ(lobby.sent[0].second.alignment, 0.25f);
	EXPECT_EQ(lobby.sent[0].second.mind, mind);
	EXPECT_EQ(lobby.sent[0].second.physique, userData);
	// Each end box sends again
	EXPECT_EQ(system.SendAtGameEnd(0.5f), ClanUpload::Sent);
	EXPECT_EQ(lobby.sent.size(), 2u);
}

TEST_F(ClanCreatureTest, TheEndSendsNothingWithNoClanOrNoFile)
{
	system.KeepCreatureFile(mind);
	EXPECT_EQ(system.SendAtGameEnd(0.0f), ClanUpload::NoClan);
	ClanCreatureSystem fresh {[this]() -> LobbyClientSystemInterface* { return &lobby; }};
	fresh.UseClan(4);
	lobby.Answer(0, Ok(Stored()));
	EXPECT_EQ(fresh.SendAtGameEnd(0.0f), ClanUpload::NoCreatureFile);
	fresh.KeepCreatureFile({1, 2, 3});
	EXPECT_EQ(fresh.SendAtGameEnd(0.0f), ClanUpload::BadCreatureFile);
	EXPECT_TRUE(lobby.sent.empty());
}

TEST_F(ClanCreatureTest, TheKeptFileStaysFromAnEarlierGame)
{
	// The file is kept for the run: a later clan game that keeps no new file sends the file of the earlier game
	system.KeepCreatureFile(mind);
	system.UseClan(4);
	lobby.Answer(0, Ok(Stored()));
	EXPECT_EQ(system.SendAtGameEnd(0.0f), ClanUpload::Sent);
	EXPECT_EQ(lobby.sent.at(0).second.mind, mind);
}

TEST_F(ClanCreatureTest, ANewCreatureHasNoUserDataToSendYet)
{
	system.UseClan(4);
	lobby.Answer(0, Ok({.kind = lobby_api::ClanCreatureKind::Create,
	                    .creature = std::nullopt,
	                    .make = lobby_api::NewClanCreature {.type = 2, .name = "Stripes"}}));
	system.KeepCreatureFile(mind);
	EXPECT_EQ(system.SendAtGameEnd(0.0f), ClanUpload::NoUserData);
	EXPECT_TRUE(lobby.sent.empty());
}
