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
}

TEST(LobbyStatus, ReportKeepsTheLimitsOfTheLobby)
{
	const Room room {.hostname = "Room", .land = "", .maxGods = 99, .version = "0.1.0-really-long-version-text"};
	const auto status = MakeStatus(room, Phase::Playing, {});
	EXPECT_EQ(status.gamemode, "closedplaying");
	EXPECT_EQ(status.maxplayers, 32);
	EXPECT_EQ(status.gamever.size(), k_MostVersionCharacters);
	EXPECT_EQ(MakeStatus(k_Room, Phase::Exiting, {}).gamemode, "exiting");
}

TEST(LobbyStatus, ReportGoesToTheJsonOfTheLobby)
{
	const std::vector seats {SeatOf(LockstepHost::SeatState::Taken, "7")};
	const auto text = openblack::lobby_api::ToJson(MakeStatus(k_Room, Phase::Staging, seats));
	EXPECT_NE(text.find(R"("hostname":"Room")"), std::string::npos);
	EXPECT_NE(text.find(R"("gamemode":"openstaging")"), std::string::npos);
	EXPECT_NE(text.find(R"("maxplayers":2)"), std::string::npos);
	EXPECT_NE(text.find(R"("players":[{"account":7,"clan":null,"role":"god"}])"), std::string::npos);
}

TEST(LobbyStatus, TheGameListComparesTheVersionThatTheServerReports)
{
	// The game server reports its semantic version whole, and the game list of a client of the same build shows no
	// "(Wrong Version)" for it
	const Room room {
	    .hostname = "Room", .land = "", .maxGods = 2, .version = std::string(openblack::build_version::Semantic())};
	const auto status = MakeStatus(room, Phase::Staging, {});
	EXPECT_LE(openblack::build_version::Semantic().size(), k_MostVersionCharacters);
	EXPECT_EQ(status.gamever, openblack::build_version::Semantic());

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
	EXPECT_EQ(GameLine(listed, texts, openblack::build_version::Semantic(), LobbyKind::Internet), u"Room (1/2)");
	// A server of another build shows it
	listed.gamever = "0.0.1";
	EXPECT_EQ(GameLine(listed, texts, openblack::build_version::Semantic(), LobbyKind::Internet),
	          u"Room (1/2) (Wrong Version)");
}
