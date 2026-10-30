/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <limits>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "Hand/HandReportRules.h"
#include "Hand/ViewReportRules.h"

using namespace openblack;
using namespace openblack::hand_report;

namespace
{
/// A report with a hand over the land of a network map, and a camera that looks down at it
player_commands::HandReport Resting()
{
	return {
	    .collidePoint = {1985.0f, 99.0f, 2355.0f},
	    .handPoint = {1990.0f, 101.0f, 2360.0f},
	    .camera = {2024.62256f, 194.08386f, 2271.5874f},
	    .focus = {1988.58362f, 99.86731f, 2358.64062f},
	};
}
} // namespace

TEST(HandReportRules, AnAxisMovesOnlyWhenItMovesByMoreThanTheLimit)
{
	const float limit = 0.01f;
	EXPECT_FALSE(PointMoved({0.0f, 0.0f, 0.0f}, {limit, 0.0f, 0.0f}));
	EXPECT_FALSE(PointMoved({0.0f, 0.0f, 0.0f}, {0.0f, -limit, 0.0f}));
	const float over = std::nextafter(limit, 1.0f);
	EXPECT_TRUE(PointMoved({0.0f, 0.0f, 0.0f}, {over, 0.0f, 0.0f}));
	EXPECT_TRUE(PointMoved({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -over}));
	EXPECT_TRUE(PointMoved({5.0f, 5.0f, 5.0f}, {5.0f, 5.0f, 6.0f}));
}

TEST(HandReportRules, TheLimitIsTheFloatOfAHundredthAsADouble)
{
	EXPECT_EQ(k_MoveLimit, 0.009999999776482582);
}

TEST(HandReportRules, AnAxisThatIsNotANumberDoesNotMove)
{
	const float nan = std::numeric_limits<float>::quiet_NaN();
	EXPECT_FALSE(PointMoved({0.0f, 0.0f, 0.0f}, {nan, 0.0f, 0.0f}));
	EXPECT_FALSE(PointMoved({nan, nan, nan}, {1.0f, 1.0f, 1.0f}));
}

TEST(HandReportRules, TheCollidePointMovesOnlyIntoAnotherMapCell)
{
	// A map cell is ten units across
	EXPECT_FALSE(CollidePointMoved({12.0f, 0.0f, 5.0f}, {19.99f, 500.0f, 9.9f}));
	EXPECT_TRUE(CollidePointMoved({12.0f, 0.0f, 5.0f}, {20.0f, 0.0f, 5.0f}));
	EXPECT_TRUE(CollidePointMoved({12.0f, 0.0f, 5.0f}, {12.0f, 0.0f, 10.0f}));
	EXPECT_TRUE(CollidePointMoved({0.5f, 0.0f, 0.5f}, {-0.5f, 0.0f, 0.5f}));
}

TEST(HandReportRules, TheMapCellIsTheHighWordOfTheFixedPoint)
{
	EXPECT_EQ(MapCell({10.0f, 0.0f, 9.999f}), (std::array<uint16_t, 2> {1, 0}));
	EXPECT_EQ(MapCell({5119.0f, 0.0f, 25.0f}), (std::array<uint16_t, 2> {511, 2}));
	EXPECT_EQ(MapCell({-0.5f, 0.0f, 0.0f}), (std::array<uint16_t, 2> {0xFFFF, 0}));
}

TEST(HandReportRules, ACameraThatMovesByVerySmallValuesSendsNoReport)
{
	const auto last = Resting();
	auto now = last;
	// The focus of a resting camera moved by less than two thousandths at each turn
	now.focus += glm::vec3(0.000122f, -0.001648f, 0.001465f);
	now.handPoint += glm::vec3(0.004f, 0.0f, -0.004f);
	now.collidePoint += glm::vec3(0.5f, 3.0f, 0.5f);
	EXPECT_EQ(MovedParts(last, now), Parts::None);
	EXPECT_FALSE(ToSend(last, now).has_value());
}

TEST(HandReportRules, BothPartsGoWhenBothMoved)
{
	const auto last = Resting();
	auto now = last;
	now.handPoint.x += 1.0f;
	now.camera.y += 1.0f;
	EXPECT_EQ(MovedParts(last, now), Parts::All);
	EXPECT_EQ(ToSend(last, now), now);
}

TEST(HandReportRules, TheHandPartGoesWithTheCameraOfTheLastReport)
{
	const auto last = Resting();
	auto now = last;
	now.collidePoint.x += 10.0f;
	now.camera.y += 0.005f;
	EXPECT_EQ(MovedParts(last, now), Parts::Hand);
	const auto sent = ToSend(last, now);
	ASSERT_TRUE(sent.has_value());
	EXPECT_EQ(sent->collidePoint, now.collidePoint);
	EXPECT_EQ(sent->handPoint, now.handPoint);
	EXPECT_EQ(sent->camera, last.camera);
	EXPECT_EQ(sent->focus, last.focus);
}

TEST(HandReportRules, TheCameraPartGoesWithTheHandOfTheLastReport)
{
	const auto last = Resting();
	auto now = last;
	now.focus.z += 0.5f;
	now.handPoint.y += 0.005f;
	EXPECT_EQ(MovedParts(last, now), Parts::Camera);
	const auto sent = ToSend(last, now);
	ASSERT_TRUE(sent.has_value());
	EXPECT_EQ(sent->collidePoint, last.collidePoint);
	EXPECT_EQ(sent->handPoint, last.handPoint);
	EXPECT_EQ(sent->camera, now.camera);
	EXPECT_EQ(sent->focus, now.focus);
}

TEST(HandReportRules, TheFirstReportGoesFromTheMiddleOfTheAxes)
{
	const player_commands::HandReport none {};
	EXPECT_EQ(ToSend(none, Resting()), Resting());
	EXPECT_FALSE(ToSend(none, none).has_value());
}

TEST(HandReportRules, TheReportWaitsThreeTurnsInAGameOfTheInternetLobby)
{
	EXPECT_EQ(TurnsToWaitAfterSend(online::GameOrigin::InternetLobby), 3);
	EXPECT_EQ(TurnsToWaitAfterSend(online::GameOrigin::Lan), 1);
	EXPECT_EQ(TurnsToWaitAfterSend(std::nullopt), 1);
}

TEST(HandReportRules, AJoinWithATicketOfTheLobbyIsAGameOfTheInternetLobby)
{
	EXPECT_EQ(online::OriginOfJoin("ticket"), online::GameOrigin::InternetLobby);
	EXPECT_EQ(online::OriginOfJoin(""), online::GameOrigin::Lan);
}

TEST(HandReportRules, AfterASendTheReportTestsAgainAfterTheWait)
{
	for (const auto origin : {std::optional(online::GameOrigin::InternetLobby), std::optional(online::GameOrigin::Lan),
	                          std::optional<online::GameOrigin>()})
	{
		// The camera moves at each turn, so each turn that tests also sends
		uint8_t wait = 0;
		std::vector<bool> tests;
		for (int turn = 0; turn < 8; ++turn)
		{
			const bool waits = WaitsThisTurn(wait, false);
			tests.push_back(!waits);
			wait = TurnsToWaitAfter(wait, waits, !waits, origin);
		}
		const auto gap = TurnsToWaitAfterSend(origin);
		for (size_t turn = 0; turn < tests.size(); ++turn)
		{
			EXPECT_EQ(tests[turn], turn % (gap + 1U) == 0) << "turn " << turn;
		}
	}
}

TEST(HandReportRules, ATurnThatSendsNothingKeepsTheWait)
{
	EXPECT_EQ(TurnsToWaitAfter(0, false, false, online::GameOrigin::InternetLobby), 0);
	EXPECT_FALSE(WaitsThisTurn(0, false));
	// A miracle that stops the wait in a turn that sends nothing leaves the turns to wait as they were
	EXPECT_EQ(TurnsToWaitAfter(2, false, false, online::GameOrigin::InternetLobby), 2);
	EXPECT_EQ(TurnsToWaitAfter(2, true, false, online::GameOrigin::InternetLobby), 1);
}

TEST(HandReportRules, AMiracleThatFollowsTheHandStopsTheWait)
{
	EXPECT_TRUE(WaitsThisTurn(3, false));
	EXPECT_FALSE(WaitsThisTurn(3, true));
	EXPECT_EQ(TurnsToWaitAfter(3, false, true, online::GameOrigin::InternetLobby), 3);
}

TEST(HandReportRules, AMiracleCastInTheHandNeedsAReportEachTurn)
{
	const ReportedSpell held {
	    .castFromHand = true,
	    .closedDown = false,
	    .humanCasting = true,
	    .ownPlayer = true,
	    .flockMakesAnimals = false,
	};
	EXPECT_TRUE(NeedsReportEachTurn(held));
	auto closed = held;
	closed.closedDown = true;
	EXPECT_FALSE(NeedsReportEachTurn(closed));
	auto notInHand = held;
	notInHand.castFromHand = false;
	EXPECT_FALSE(NeedsReportEachTurn(notInHand));
	auto otherPlayer = held;
	otherPlayer.ownPlayer = false;
	EXPECT_FALSE(NeedsReportEachTurn(otherPlayer));
	auto script = held;
	script.humanCasting = false;
	EXPECT_FALSE(NeedsReportEachTurn(script));
}

TEST(HandReportRules, AFlockNeedsAReportEachTurnWhileItMakesAnimals)
{
	const ReportedSpell flock {
	    .castFromHand = false,
	    .closedDown = true,
	    .humanCasting = true,
	    .ownPlayer = true,
	    .flockMakesAnimals = true,
	};
	// The flock rule does not look at the close down
	EXPECT_TRUE(NeedsReportEachTurn(flock));
	auto done = flock;
	done.flockMakesAnimals = false;
	EXPECT_FALSE(NeedsReportEachTurn(done));
	auto otherPlayer = flock;
	otherPlayer.ownPlayer = false;
	EXPECT_FALSE(NeedsReportEachTurn(otherPlayer));
	auto computer = flock;
	computer.humanCasting = false;
	EXPECT_FALSE(NeedsReportEachTurn(computer));
}

namespace
{
/// A view report of a camera that rests over the land of a network map
player_commands::ViewReport RestingView()
{
	return {
	    .eye = {2024.62256f, 194.08386f, 2271.5874f},
	    .focus = {1988.58362f, 99.86731f, 2358.64062f},
	    .lens = 1.0f,
	    .nearClip = 1.0f,
	    .screenWidth = 1280,
	    .screenHeight = 1024,
	    .aspect = 1.25f,
	    .inclusionDistance = 30.0f,
	    .cameraOrders = 2,
	    .cameraArrived = true,
	    .gripsLand = false,
	    .otherButton = false,
	};
}
} // namespace

TEST(ViewReportRules, AViewThatDidNotChangeSendsNoReport)
{
	EXPECT_FALSE(view_report::ChangedEnough(RestingView(), RestingView()));
}

TEST(ViewReportRules, AnEyeOrAFocusThatMovesByVerySmallValuesSendsNoReport)
{
	const auto last = RestingView();
	auto now = last;
	// The focus of a resting camera moved by less than two thousandths at each turn
	now.focus += glm::vec3(0.000122f, -0.001648f, 0.001465f);
	now.eye += glm::vec3(0.0f, 0.009f, -0.009f);
	EXPECT_FALSE(view_report::ChangedEnough(last, now));
}

TEST(ViewReportRules, TheEyeOrTheFocusSendsAReportWhenItMovesByMoreThanTheLimit)
{
	const auto last = RestingView();
	const float over = std::nextafter(0.01f, 1.0f);
	auto eye = last;
	eye.eye.y = last.eye.y + 0.5f;
	EXPECT_TRUE(view_report::ChangedEnough(last, eye));
	auto focus = last;
	focus.focus = glm::vec3(0.0f);
	auto from = focus;
	from.focus.z = over;
	EXPECT_TRUE(view_report::ChangedEnough(focus, from));
	from.focus.z = 0.01f;
	EXPECT_FALSE(view_report::ChangedEnough(focus, from));
}

TEST(ViewReportRules, TheSmallMovesAddFromTheLastReportThatWent)
{
	// The rule compares with the last report that went, so small steps that add up to more than the limit send it
	const auto last = RestingView();
	auto now = last;
	for (int step = 0; step < 6; ++step)
	{
		now.focus.x += 0.002f;
	}
	EXPECT_TRUE(view_report::ChangedEnough(last, now));
}

TEST(ViewReportRules, AChangeOfADifferentFieldSendsAReport)
{
	const auto last = RestingView();
	std::vector<player_commands::ViewReport> changes(9, last);
	changes[0].lens = 1.0001f;
	changes[1].nearClip = 2.0f;
	changes[2].screenWidth = 1920;
	changes[3].screenHeight = 1080;
	changes[4].aspect = 16.0f / 9.0f;
	changes[5].inclusionDistance = 30.001f;
	changes[6].cameraOrders = 3;
	changes[7].cameraArrived = false;
	changes[8].gripsLand = true;
	for (const auto& now : changes)
	{
		EXPECT_TRUE(view_report::ChangedEnough(last, now));
	}
	auto other = last;
	other.otherButton = true;
	EXPECT_TRUE(view_report::ChangedEnough(last, other));
}

TEST(ViewReportRules, AFieldChangeSendsAReportWithASmallCameraMove)
{
	const auto last = RestingView();
	auto now = last;
	now.focus.x += 0.001f;
	now.cameraArrived = false;
	EXPECT_TRUE(view_report::ChangedEnough(last, now));
}
