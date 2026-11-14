/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <string>

#include <gtest/gtest.h>

#include "Online/ConnectionLostNotice.h"

using openblack::online::ConnectionLostFacts;
using openblack::online::ConnectionLostNotice;
using openblack::online::DataRefusalText;
using openblack::online::k_MostDataRefusalPathLetters;
using openblack::online::LostTheConnection;
using openblack::online::NetworkSession;

namespace
{

/// A started game whose session ended in this way
ConnectionLostFacts EndedWith(NetworkSession::End end)
{
	return {.hasSession = true, .started = true, .state = NetworkSession::State::Ended, .end = end, .toolRun = false};
}

} // namespace

TEST(ConnectionLost, OnlyAnEndThatThePlayerDidNotChooseLosesTheConnection)
{
	EXPECT_TRUE(LostTheConnection(NetworkSession::End::Lost));
	EXPECT_TRUE(LostTheConnection(NetworkSession::End::Refused));
	EXPECT_TRUE(LostTheConnection(NetworkSession::End::NoConnection));
	EXPECT_TRUE(LostTheConnection(NetworkSession::End::ServerLimit));
	EXPECT_FALSE(LostTheConnection(NetworkSession::End::None));
	EXPECT_FALSE(LostTheConnection(NetworkSession::End::Left));
	// The out-of-sync box asks its own question
	EXPECT_FALSE(LostTheConnection(NetworkSession::End::OutOfSync));
}

TEST(ConnectionLost, TheBoxShowsOnceWhenAStartedGameLosesTheConnection)
{
	ConnectionLostNotice notice;
	EXPECT_TRUE(notice.Update(EndedWith(NetworkSession::End::Lost)));
	EXPECT_TRUE(notice.Told());
	EXPECT_FALSE(notice.Update(EndedWith(NetworkSession::End::Lost)));
	EXPECT_TRUE(notice.Told());
}

TEST(ConnectionLost, TheBoxWaitsForTheLastTryToComeBack)
{
	ConnectionLostNotice notice;
	auto facts = EndedWith(NetworkSession::End::None);
	facts.state = NetworkSession::State::Reconnecting;
	EXPECT_FALSE(notice.Update(facts));
	facts.state = NetworkSession::State::Playing;
	EXPECT_FALSE(notice.Update(facts));
	EXPECT_TRUE(notice.Update(EndedWith(NetworkSession::End::Lost)));
}

TEST(ConnectionLost, NoBoxBeforeTheStartForAPlayerWhoLeavesOrInARunOfTheTools)
{
	ConnectionLostNotice notice;
	auto beforeStart = EndedWith(NetworkSession::End::Lost);
	beforeStart.started = false;
	EXPECT_FALSE(notice.Update(beforeStart));
	EXPECT_FALSE(notice.Update(EndedWith(NetworkSession::End::Left)));
	EXPECT_FALSE(notice.Update(EndedWith(NetworkSession::End::OutOfSync)));
	auto toolRun = EndedWith(NetworkSession::End::Lost);
	toolRun.toolRun = true;
	EXPECT_FALSE(notice.Update(toolRun));
	EXPECT_FALSE(notice.Told());
}

TEST(ConnectionLost, TheNextNetworkSessionCanShowTheBoxAgain)
{
	ConnectionLostNotice notice;
	EXPECT_TRUE(notice.Update(EndedWith(NetworkSession::End::Refused)));
	// The network game ends and the front end opens: no session
	EXPECT_FALSE(notice.Update({}));
	EXPECT_FALSE(notice.Told());
	EXPECT_TRUE(notice.Update(EndedWith(NetworkSession::End::Lost)));
}

TEST(ConnectionLost, AKickOfTheLobbysHostShowsTheBoxOfTheKick)
{
	EXPECT_TRUE(openblack::online::KickedFromGame(NetworkSession::End::Refused, "kicked"));
	EXPECT_FALSE(openblack::online::KickedFromGame(NetworkSession::End::Refused, "resume"));
	EXPECT_FALSE(openblack::online::KickedFromGame(NetworkSession::End::Lost, "kicked"));
}

TEST(ConnectionLost, ARefusalForTheGameDataNamesTheFile)
{
	EXPECT_EQ(DataRefusalText(NetworkSession::End::Refused, "data", "Scripts/info.dat"),
	          "Your game data is not the server's: Scripts/info.dat");
}

TEST(ConnectionLost, OtherRefusalsAndEndsHaveNoTextOfTheGameData)
{
	EXPECT_FALSE(DataRefusalText(NetworkSession::End::Refused, "kicked", "Scripts/info.dat").has_value());
	EXPECT_FALSE(DataRefusalText(NetworkSession::End::Refused, "full", {}).has_value());
	EXPECT_FALSE(DataRefusalText(NetworkSession::End::Lost, "data", "Scripts/info.dat").has_value());
	// An older server sends no path: the usual box shows
	EXPECT_FALSE(DataRefusalText(NetworkSession::End::Refused, "data", {}).has_value());
}

TEST(ConnectionLost, TheTextOfTheGameDataKeepsOnlyPrintableLettersOfThePath)
{
	EXPECT_EQ(DataRefusalText(NetworkSession::End::Refused, "data", "Data/\x01Land\n1.lnd\x7f"),
	          "Your game data is not the server's: Data/Land1.lnd");
	EXPECT_FALSE(DataRefusalText(NetworkSession::End::Refused, "data", "\x01\x02").has_value());
	const auto text = DataRefusalText(NetworkSession::End::Refused, "data", std::string(1000, 'a'));
	ASSERT_TRUE(text.has_value());
	EXPECT_EQ(*text, "Your game data is not the server's: " + std::string(k_MostDataRefusalPathLetters, 'a'));
}

TEST(ConnectionLost, AJoinFromTheCommandLineShowsTheBoxOfTheGameDataBeforeTheStart)
{
	ConnectionLostFacts facts {.hasSession = true,
	                           .started = false,
	                           .state = NetworkSession::State::Ended,
	                           .end = NetworkSession::End::Refused,
	                           .toolRun = false,
	                           .joinedFromCommandLine = true,
	                           .dataRefused = true};
	ConnectionLostNotice notice;
	EXPECT_TRUE(notice.Update(facts));
	EXPECT_FALSE(notice.Update(facts));
	// The front end tells a player who joined from it, and a run of the tools stops with no box
	ConnectionLostNotice frontEnd;
	facts.joinedFromCommandLine = false;
	EXPECT_FALSE(frontEnd.Update(facts));
	ConnectionLostNotice tools;
	facts.joinedFromCommandLine = true;
	facts.toolRun = true;
	EXPECT_FALSE(tools.Update(facts));
	// Another refusal before the start shows no box
	ConnectionLostNotice other;
	facts.toolRun = false;
	facts.dataRefused = false;
	EXPECT_FALSE(other.Update(facts));
}
