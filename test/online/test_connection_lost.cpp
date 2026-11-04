/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Online/ConnectionLostNotice.h"

using openblack::online::ConnectionLostFacts;
using openblack::online::ConnectionLostNotice;
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
