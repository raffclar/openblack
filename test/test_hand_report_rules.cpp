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
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "Hand/HandReportRules.h"
#include "Hand/OwnHandReport.h"
#include "Hand/ViewReportRules.h"
#include "Online/GameOrigin.h"

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

// The desktop game reads the ticket from a file, so that it is not on the command line. A line end after it is not part
// of the ticket.
TEST(HandReportRules, TheTicketFileGivesTheTicketWithNoSpaceAroundIt)
{
	EXPECT_EQ(online::TicketFileText("v4.public.abc"), "v4.public.abc");
	EXPECT_EQ(online::TicketFileText("  v4.public.abc\r\n"), "v4.public.abc");
	EXPECT_FALSE(online::TicketFileText("").has_value());
	EXPECT_FALSE(online::TicketFileText(" \r\n").has_value());
	EXPECT_FALSE(online::TicketFileText("v4.public.a\nv4.public.b").has_value());
	EXPECT_FALSE(online::TicketFileText("v4.public.a b").has_value());
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
	auto turned = last;
	turned.cameraTurned = true;
	EXPECT_TRUE(view_report::ChangedEnough(last, turned));
}

TEST(ViewReportRules, ACameraTurnCountsForTheRestOfItsGameTurn)
{
	// A turn in a frame keeps the flag for the later frames of the same game turn
	EXPECT_TRUE(view_report::CameraTurnedInTurn(false, true, true));
	EXPECT_TRUE(view_report::CameraTurnedInTurn(true, true, false));
	EXPECT_FALSE(view_report::CameraTurnedInTurn(false, true, false));
	// A new game turn starts again from the frame
	EXPECT_FALSE(view_report::CameraTurnedInTurn(true, false, false));
	EXPECT_TRUE(view_report::CameraTurnedInTurn(true, false, true));
}

TEST(ViewReportRules, AFieldChangeSendsAReportWithASmallCameraMove)
{
	const auto last = RestingView();
	auto now = last;
	now.focus.x += 0.001f;
	now.cameraArrived = false;
	EXPECT_TRUE(view_report::ChangedEnough(last, now));
}

namespace
{
/// The inputs of the pass of a turn with the hand and the camera of Resting
OwnInputs RestingInputs(OwnStart start)
{
	const auto resting = Resting();
	return {
	    .handPoint = start.handPoint,
	    .picked = entt::entity {entt::null},
	    .pickPoint = resting.collidePoint,
	    .camera = resting.camera,
	    .focus = resting.focus,
	    .spellNeedsReport = [] { return false; },
	    .origin = std::nullopt,
	};
}
} // namespace

TEST(OwnHandReport, ThePassGoesOnceATurnAndNeedsAHandWithAPlace)
{
	OwnReporter reporter;
	const auto hand = static_cast<entt::entity>(4);
	// A hand with no place starts no pass, and the turn is done
	EXPECT_FALSE(reporter.Start(1, hand, std::nullopt).has_value());
	EXPECT_FALSE(reporter.Start(1, hand, Resting().handPoint).has_value());
	// The next turn starts the pass with a new hand
	const auto start = reporter.Start(2, hand, Resting().handPoint);
	ASSERT_TRUE(start.has_value());
	EXPECT_TRUE(start->newHand);
	EXPECT_EQ(start->handPoint, Resting().handPoint);
	EXPECT_FALSE(reporter.Start(2, hand, Resting().handPoint).has_value());
	// The same hand is not new at a later turn
	const auto later = reporter.Start(3, hand, Resting().handPoint);
	ASSERT_TRUE(later.has_value());
	EXPECT_FALSE(later->newHand);
}

TEST(OwnHandReport, TheFirstPassSendsTheFullReportAndThenWaitsOneTurn)
{
	OwnReporter reporter;
	const auto hand = static_cast<entt::entity>(4);
	auto start = reporter.Start(1, hand, Resting().handPoint);
	ASSERT_TRUE(start.has_value());
	const auto first = reporter.Finish(RestingInputs(*start));
	ASSERT_TRUE(first.report.has_value());
	EXPECT_EQ(*first.report, Resting());
	EXPECT_EQ(reporter.TurnsToWait(), 1);
	// The next turn waits, also when the hand moved, and asks the miracles
	start = reporter.Start(2, hand, Resting().handPoint + glm::vec3(5.0f, 0.0f, 0.0f));
	ASSERT_TRUE(start.has_value());
	auto inputs = RestingInputs(*start);
	int asked = 0;
	inputs.spellNeedsReport = [&asked] {
		++asked;
		return false;
	};
	EXPECT_FALSE(reporter.Finish(inputs).report.has_value());
	EXPECT_EQ(asked, 1);
	EXPECT_EQ(reporter.TurnsToWait(), 0);
	// The turn after that sends only the hand part that moved, and does not ask the miracles
	start = reporter.Start(3, hand, Resting().handPoint + glm::vec3(5.0f, 0.0f, 0.0f));
	ASSERT_TRUE(start.has_value());
	inputs = RestingInputs(*start);
	inputs.spellNeedsReport = [&asked] {
		++asked;
		return false;
	};
	const auto moved = reporter.Finish(inputs);
	ASSERT_TRUE(moved.report.has_value());
	EXPECT_EQ(moved.report->handPoint, Resting().handPoint + glm::vec3(5.0f, 0.0f, 0.0f));
	EXPECT_EQ(asked, 1);
}

TEST(OwnHandReport, AMiracleThatFollowsTheHandStopsTheWait)
{
	OwnReporter reporter;
	const auto hand = static_cast<entt::entity>(4);
	auto start = reporter.Start(1, hand, Resting().handPoint);
	ASSERT_TRUE(start.has_value());
	std::ignore = reporter.Finish(RestingInputs(*start));
	start = reporter.Start(2, hand, Resting().handPoint + glm::vec3(5.0f, 0.0f, 0.0f));
	ASSERT_TRUE(start.has_value());
	auto inputs = RestingInputs(*start);
	inputs.spellNeedsReport = [] { return true; };
	EXPECT_TRUE(reporter.Finish(inputs).report.has_value());
}

TEST(OwnHandReport, TheThingUnderTheCursorGoesOnlyWhenItChanges)
{
	OwnReporter reporter;
	const auto hand = static_cast<entt::entity>(4);
	const auto creature = static_cast<entt::entity>(9);
	auto start = reporter.Start(1, hand, Resting().handPoint);
	ASSERT_TRUE(start.has_value());
	auto inputs = RestingInputs(*start);
	// No thing at the first pass is the same as the start: nothing goes
	EXPECT_FALSE(reporter.Finish(inputs).pick.has_value());
	start = reporter.Start(2, hand, Resting().handPoint);
	ASSERT_TRUE(start.has_value());
	inputs = RestingInputs(*start);
	inputs.picked = creature;
	const auto picked = reporter.Finish(inputs);
	ASSERT_TRUE(picked.pick.has_value());
	EXPECT_EQ(picked.pick->object, creature);
	start = reporter.Start(3, hand, Resting().handPoint);
	ASSERT_TRUE(start.has_value());
	inputs = RestingInputs(*start);
	inputs.picked = creature;
	EXPECT_FALSE(reporter.Finish(inputs).pick.has_value());
	// With no picks at all, nothing goes
	start = reporter.Start(4, hand, Resting().handPoint);
	ASSERT_TRUE(start.has_value());
	inputs = RestingInputs(*start);
	inputs.picked.reset();
	EXPECT_FALSE(reporter.Finish(inputs).pick.has_value());
}

TEST(OwnHandReport, ANewHandStartsWithNoReport)
{
	OwnReporter reporter;
	auto start = reporter.Start(1, static_cast<entt::entity>(4), Resting().handPoint);
	ASSERT_TRUE(start.has_value());
	auto inputs = RestingInputs(*start);
	inputs.picked = static_cast<entt::entity>(9);
	std::ignore = reporter.Finish(inputs);
	// Another hand: the full report and the thing under the cursor go again, after the wait of the old hand
	start = reporter.Start(2, static_cast<entt::entity>(5), Resting().handPoint);
	ASSERT_TRUE(start.has_value());
	EXPECT_TRUE(start->newHand);
	inputs = RestingInputs(*start);
	inputs.picked = static_cast<entt::entity>(9);
	const auto again = reporter.Finish(inputs);
	EXPECT_TRUE(again.pick.has_value());
	EXPECT_FALSE(again.report.has_value());
	EXPECT_EQ(reporter.TurnsToWait(), 0);
	start = reporter.Start(3, static_cast<entt::entity>(5), Resting().handPoint);
	ASSERT_TRUE(start.has_value());
	const auto full = reporter.Finish(RestingInputs(*start));
	ASSERT_TRUE(full.report.has_value());
	EXPECT_EQ(*full.report, Resting());
}
