/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <string>
#include <vector>

#include <LobbyApiJson.h>
#include <gtest/gtest.h>

#include "Common/BuildVersion.h"
#include "Gui/MultiplayerFrontEndRules.h"
#include "Online/GameBuildId.h"
#include "Server/LobbyStatus.h"

using namespace openblack::server;
using namespace openblack::server::lobby_status;

namespace
{

LockstepHost::Seat SeatOf(LockstepHost::SeatState state, std::string account)
{
	LockstepHost::Seat seat;
	seat.state = state;
	seat.account = std::move(account);
	return seat;
}

const Room k_Room {.hostname = "Room", .land = "TwoGods.txt", .maxGods = 2, .version = "0.1.0"};

} // namespace

TEST(LobbyStatus, PhaseFollowsTheStartAndTheStop)
{
	EXPECT_EQ(PhaseOf(false, false), Phase::Staging);
	EXPECT_EQ(PhaseOf(true, false), Phase::Playing);
	EXPECT_EQ(PhaseOf(false, true), Phase::Exiting);
	EXPECT_EQ(PhaseOf(true, true), Phase::Exiting);
}

TEST(LobbyStatus, AccountIdIsAPositiveDecimalNumber)
{
	EXPECT_EQ(AccountId("42"), 42);
	EXPECT_FALSE(AccountId("").has_value());
	EXPECT_FALSE(AccountId("0").has_value());
	EXPECT_FALSE(AccountId("-3").has_value());
	EXPECT_FALSE(AccountId("12a").has_value());
	EXPECT_FALSE(AccountId("99999999999999999999999").has_value());
}

TEST(LobbyStatus, ReportHasTheGodsThatHaveASeat)
{
	const std::vector seats {
	    SeatOf(LockstepHost::SeatState::Taken, "7"),   SeatOf(LockstepHost::SeatState::Open, ""),
	    SeatOf(LockstepHost::SeatState::Away, "9"),    SeatOf(LockstepHost::SeatState::Closed, "11"),
	    SeatOf(LockstepHost::SeatState::Taken, "bad"),
	};
	const auto status = MakeStatus(k_Room, Phase::Staging, seats);
	EXPECT_EQ(status.gamemode, "openstaging");
	EXPECT_EQ(status.maxplayers, 2);
	ASSERT_EQ(status.players.size(), 2U);
	EXPECT_EQ(status.players[0].account, 7);
	EXPECT_EQ(status.players[1].account, 9);
	EXPECT_EQ(status.players[0].role, "god");
	EXPECT_FALSE(status.players[0].clan.has_value());
	// The god of an away seat keeps its seat, and the report shows that its player is not connected
	EXPECT_FALSE(status.players[0].away);
	EXPECT_TRUE(status.players[1].away);
}

TEST(LobbyStatus, TheGodsChangeWhenAPlayerGoesOrComesBack)
{
	std::vector seats {SeatOf(LockstepHost::SeatState::Taken, "7"), SeatOf(LockstepHost::SeatState::Taken, "9")};
	const auto connected = StatusPlayers(seats);
	seats[1].state = LockstepHost::SeatState::Away;
	const auto away = StatusPlayers(seats);
	EXPECT_NE(away, connected);
	seats[1].state = LockstepHost::SeatState::Taken;
	EXPECT_EQ(StatusPlayers(seats), connected);
	seats[1].state = LockstepHost::SeatState::Closed;
	const auto left = StatusPlayers(seats);
	EXPECT_NE(left, connected);
	ASSERT_EQ(left.size(), 1U);
	EXPECT_EQ(left[0].account, 7);
}

TEST(LobbyStatus, ReportKeepsTheLimitsOfTheLobby)
{
	const Room room {.hostname = "Room", .land = "", .maxGods = 99, .version = std::string(80, 'v')};
	const auto status = MakeStatus(room, Phase::Playing, {});
	EXPECT_EQ(status.gamemode, "closedplaying");
	EXPECT_EQ(status.maxplayers, 32);
	EXPECT_EQ(status.gamever.size(), openblack::lobby_api::k_MostGameVersionCharacters);
	// For an older lobby, the report has the first 16 characters
	EXPECT_EQ(MakeStatus(room, Phase::Playing, {}, openblack::lobby_api::k_OldMostGameVersionCharacters).gamever,
	          std::string(16, 'v'));
	EXPECT_EQ(MakeStatus(k_Room, Phase::Exiting, {}).gamemode, "exiting");
}

TEST(LobbyStatus, ReportGoesToTheJsonOfTheLobby)
{
	const std::vector seats {SeatOf(LockstepHost::SeatState::Taken, "7")};
	const auto text = openblack::lobby_api::ToJson(MakeStatus(k_Room, Phase::Staging, seats));
	EXPECT_NE(text.find(R"("hostname":"Room")"), std::string::npos);
	EXPECT_NE(text.find(R"("gamemode":"openstaging")"), std::string::npos);
	EXPECT_NE(text.find(R"("maxplayers":2)"), std::string::npos);
	EXPECT_NE(text.find(R"("players":[{"account":7,"away":false,"clan":null,"role":"god"}])"), std::string::npos);
}

TEST(LobbyStatus, TheAnswerGivesTheListingAndTheTimeOfTheNextReport)
{
	const auto reply =
	    openblack::lobby_api::ParseServerStatusReply(R"({"next_status_in":15,"listed":true,"clans":[{"account":7,"clan":3}]})");
	ASSERT_TRUE(reply.has_value());
	EXPECT_EQ(reply->nextStatusIn, 15);
	EXPECT_TRUE(reply->listed);
	ASSERT_EQ(reply->clans.size(), 1);
	EXPECT_EQ(reply->clans[0].account, 7);
	EXPECT_EQ(reply->clans[0].clan, 3);
	EXPECT_FALSE(openblack::lobby_api::ParseServerStatusReply(R"({"listed":true,"clans":[]})").has_value());
	EXPECT_FALSE(openblack::lobby_api::ParseServerStatusReply("{}").has_value());
}

TEST(LobbyStatus, AnOlderLobbyThatRefusesTheLengthOfTheVersionGetsTheCutVersion)
{
	// An older lobby answers 422 with the field "gamever" in the errors
	constexpr std::string_view k_Refusal = R"({"type":"about:blank","title":"Unprocessable Content","status":422,)"
	                                       R"("detail":"The request has fields that are not valid.","instance":"",)"
	                                       R"("code":"invalid","errors":{"gamever":["Too long."]}})";
	EXPECT_TRUE(RefusedTheVersionLength(422, k_Refusal));
	// Another field, another status or a body that is not a problem do not cut the version
	EXPECT_FALSE(RefusedTheVersionLength(422, R"({"status":422,"code":"invalid","errors":{"hostname":["Too long."]}})"));
	EXPECT_FALSE(RefusedTheVersionLength(400, k_Refusal));
	EXPECT_FALSE(RefusedTheVersionLength(422, "not json"));
	EXPECT_FALSE(RefusedTheVersionLength(422, R"({"status":422,"code":"invalid"})"));
}

TEST(LobbyStatus, TheGameListComparesTheVersionThatTheServerReports)
{
	// The game server reports its build id whole, and the game list of a client of the same build shows no
	// "(Wrong Version)" for it
	const auto own = openblack::online::build_id::Own();
	const Room room {.hostname = "Room", .land = "", .maxGods = 2, .version = own};
	const auto status = MakeStatus(room, Phase::Staging, {});
	EXPECT_LE(own.size(), openblack::lobby_api::k_MostGameVersionCharacters);
	EXPECT_EQ(status.gamever, own);

	openblack::lobby_api::ServerEntry listed;
	listed.hostname = status.hostname;
	listed.numplayers = 1;
	listed.maxplayers = status.maxplayers;
	listed.gamemode = status.gamemode;
	listed.gamever = status.gamever;
	const openblack::gui::multiplayer::GameLineTexts texts {.playing = u" - PLAYING",
	                                                        .wrongVersion = u" (Wrong Version)",
	                                                        .needsPassword = u"(Needs Password)",
	                                                        .locked = u"Locked"};
	using openblack::gui::multiplayer::LobbyKind;
	EXPECT_EQ(GameLine(listed, texts, own, LobbyKind::Internet), u"Room (1/2)");
	// An older lobby keeps the first 16 characters of the build id: the line shows no "(Wrong Version)" either
	listed.gamever = MakeStatus(room, Phase::Staging, {}, openblack::lobby_api::k_OldMostGameVersionCharacters).gamever;
	EXPECT_EQ(GameLine(listed, texts, own, LobbyKind::Internet), u"Room (1/2)");
	// A server of another build shows it, and so does a server of the same version and another commit
	listed.gamever = "0.0.1";
	EXPECT_EQ(GameLine(listed, texts, own, LobbyKind::Internet), u"Room (1/2) (Wrong Version)");
	listed.gamever = std::string(openblack::build_version::Semantic());
	EXPECT_EQ(GameLine(listed, texts, own, LobbyKind::Internet), u"Room (1/2) (Wrong Version)");
}
