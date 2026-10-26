/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The ways in to the multiplayer front end that the browser page gives: the Join Online Game button over the game and
// the -MULTIPLAYER switch on the first start

#include <gtest/gtest.h>

#include "Gui/JoinOnlineEntry.h"

using namespace openblack::join_online_entry;

TEST(JoinOnlineEntry, TheFrontEndOpensInPlay)
{
	const auto decision = Decide({});
	EXPECT_EQ(decision.outcome, Outcome::Open);
	EXPECT_FALSE(decision.holdLand);
	EXPECT_FALSE(decision.askQuestionAfter);
}

TEST(JoinOnlineEntry, ACutSceneStopsBehindTheFrontEnd)
{
	// Escape does not open the menu while a script has the cinema bars in, but the page's button opens the front end
	const auto decision = Decide({.scriptHoldsCinemaBars = true});
	EXPECT_EQ(decision.outcome, Outcome::Open);
	EXPECT_TRUE(decision.holdLand);
}

TEST(JoinOnlineEntry, TheStartQuestionWaitsBehindTheFrontEnd)
{
	const auto decision = Decide({.startQuestionOpen = true});
	EXPECT_EQ(decision.outcome, Outcome::Open);
	EXPECT_TRUE(decision.askQuestionAfter);
	EXPECT_FALSE(decision.holdLand);
}

TEST(JoinOnlineEntry, ABoxToAnswerMakesTheRequestWait)
{
	EXPECT_EQ(Decide({.boxToAnswer = true}).outcome, Outcome::Wait);
	EXPECT_EQ(Decide({.boxToAnswer = true, .scriptHoldsCinemaBars = true}).outcome, Outcome::Wait);
}

TEST(JoinOnlineEntry, AnOpenFrontEndOrANetworkGameIgnoresTheRequest)
{
	EXPECT_EQ(Decide({.frontEndOpen = true}).outcome, Outcome::Ignore);
	EXPECT_EQ(Decide({.networkGame = true}).outcome, Outcome::Ignore);
	// The network game wins over a box that waits
	EXPECT_EQ(Decide({.networkGame = true, .boxToAnswer = true}).outcome, Outcome::Ignore);
}

TEST(JoinOnlineEntry, MultiplayerWithAProfileOpensTheFrontEnd)
{
	for (const bool browser : {false, true})
	{
		EXPECT_TRUE(FrontEndOnStart({.multiplayerSwitch = true, .hadProfile = true, .hasProfile = true, .browser = browser}));
	}
}

TEST(JoinOnlineEntry, AFirstVisitInTheBrowserGoesOnAfterTheName)
{
	EXPECT_TRUE(FrontEndOnStart({.multiplayerSwitch = true, .hadProfile = false, .hasProfile = true, .browser = true}));
	// The desktop game starts a new game, as with no switch (deviation 118)
	EXPECT_FALSE(FrontEndOnStart({.multiplayerSwitch = true, .hadProfile = false, .hasProfile = true, .browser = false}));
	// The program closed during the new player's box: no profile
	EXPECT_FALSE(FrontEndOnStart({.multiplayerSwitch = true, .hadProfile = false, .hasProfile = false, .browser = true}));
}

TEST(JoinOnlineEntry, NoSwitchNoFrontEnd)
{
	EXPECT_FALSE(FrontEndOnStart({.multiplayerSwitch = false, .hadProfile = true, .hasProfile = true, .browser = true}));
}
