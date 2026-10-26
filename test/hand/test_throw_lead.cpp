/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Hand/ThrowLead.h"

using namespace openblack;
using namespace openblack::hand_grab::throw_lead;

namespace
{
void ExpectNear(glm::vec3 actual, glm::vec3 expected)
{
	EXPECT_NEAR(actual.x, expected.x, 1.0e-4f);
	EXPECT_NEAR(actual.y, expected.y, 1.0e-4f);
	EXPECT_NEAR(actual.z, expected.z, 1.0e-4f);
}

player_commands::HandReport ReportAt(float x)
{
	return {.collidePoint = {x, 0.0f, 0.0f}, .handPoint = {x, 1.0f, 0.0f}, .camera = {}, .focus = {}};
}
} // namespace

// One turn of lead for each full 100 ms, at most five
TEST(ThrowLead, TheLeadIsOneTurnForEachHundredMillisecondsAtMostFive)
{
	EXPECT_EQ(LeadTurns(0), 0);
	EXPECT_EQ(LeadTurns(99), 0);
	EXPECT_EQ(LeadTurns(100), 1);
	EXPECT_EQ(LeadTurns(199), 1);
	EXPECT_EQ(LeadTurns(499), 4);
	EXPECT_EQ(LeadTurns(500), 5);
	EXPECT_EQ(LeadTurns(2000), 5);
	// A send time after the turn's time wraps to a very large latency
	EXPECT_EQ(LeadTurns(0xFFFFFFF0U), 5);
}

// The latency is the time from the send of a report to the turn that does it, from the last report done
TEST(ThrowLead, TheLatencyComesFromTheLastReportThatATurnDid)
{
	ReportLatency latency;
	EXPECT_EQ(latency.Milliseconds(), 0U);
	latency.Sent(ReportAt(1.0f), 1000);
	latency.Sent(ReportAt(2.0f), 1100);
	// Until a turn does a report, the latency stays
	EXPECT_EQ(latency.Milliseconds(), 0U);
	latency.Done(ReportAt(1.0f), 1120);
	EXPECT_EQ(latency.Milliseconds(), 120U);
	latency.Done(ReportAt(2.0f), 1350);
	EXPECT_EQ(latency.Milliseconds(), 250U);
	// A report that this computer did not send changes nothing
	latency.Done(ReportAt(3.0f), 9000);
	EXPECT_EQ(latency.Milliseconds(), 250U);
}

// A report that never came to a turn is passed over by the next report that a turn does
TEST(ThrowLead, AReportThatNeverCameToATurnIsPassedOver)
{
	ReportLatency latency;
	latency.Sent(ReportAt(1.0f), 1000);
	latency.Sent(ReportAt(2.0f), 1100);
	latency.Sent(ReportAt(3.0f), 1200);
	latency.Done(ReportAt(2.0f), 1150);
	EXPECT_EQ(latency.Milliseconds(), 50U);
	// The first report does not wait now
	latency.Done(ReportAt(1.0f), 1300);
	EXPECT_EQ(latency.Milliseconds(), 50U);
	latency.Done(ReportAt(3.0f), 1210);
	EXPECT_EQ(latency.Milliseconds(), 10U);
}

// The clock's count wraps, and the latency wraps with it
TEST(ThrowLead, TheLatencyWrapsWithTheClock)
{
	ReportLatency latency;
	latency.Sent(ReportAt(1.0f), 0xFFFFFFC0U);
	latency.Done(ReportAt(1.0f), 0x40U);
	EXPECT_EQ(latency.Milliseconds(), 0x80U);
}

// When more reports wait than the places, the oldest are lost
TEST(ThrowLead, TheOldestSendTimesGoWhenAllThePlacesAreFull)
{
	ReportLatency latency;
	for (uint32_t i = 0; i <= k_ReportStamps; ++i)
	{
		latency.Sent(ReportAt(static_cast<float>(i)), 1000 + i);
	}
	latency.Done(ReportAt(0.0f), 5000);
	EXPECT_EQ(latency.Milliseconds(), 0U);
	latency.Done(ReportAt(1.0f), 5000);
	EXPECT_EQ(latency.Milliseconds(), 3999U);
}

// A tool run fixes the latency, and the measure comes back when it ends
TEST(ThrowLead, AFixedLatencyStandsForTheMeasure)
{
	ReportLatency latency;
	latency.Sent(ReportAt(1.0f), 1000);
	latency.Done(ReportAt(1.0f), 1100);
	latency.Fix(320);
	EXPECT_EQ(latency.Milliseconds(), 320U);
	latency.Sent(ReportAt(2.0f), 2000);
	latency.Done(ReportAt(2.0f), 2010);
	EXPECT_EQ(latency.Milliseconds(), 320U);
	latency.Fix(std::nullopt);
	EXPECT_EQ(latency.Milliseconds(), 10U);
}

// The thing goes from the place of each turn to the next by the part of the turn, then stays at the last place
TEST(ThrowLead, TheDrawnPlaceFollowsTheFlightTurnByTurn)
{
	Flight flight {
	    .places =
	        {
	            {.axes = glm::mat3(1.0f), .centre = {0.0f, 10.0f, 0.0f}},
	            {.axes = glm::mat3(1.0f), .centre = {4.0f, 12.0f, 0.0f}},
	            {.axes = glm::mat3(1.0f), .centre = {8.0f, 11.0f, 0.0f}},
	        },
	    .scale = 2.0f,
	    .centreOfMass = {0.0f, 0.5f, 0.0f},
	    .sentTurn = 40,
	};
	// The centre of mass is one unit above the origin at a size of 2
	const auto at = [&flight](uint32_t turn, float fraction) { return DrawnPose(flight, turn, fraction).origin; };
	ExpectNear(at(40, 0.0f), glm::vec3(0.0f, 9.0f, 0.0f));
	ExpectNear(at(40, 0.5f), glm::vec3(2.0f, 10.0f, 0.0f));
	ExpectNear(at(41, 0.25f), glm::vec3(5.0f, 10.75f, 0.0f));
	ExpectNear(at(42, 0.5f), glm::vec3(8.0f, 10.0f, 0.0f));
	ExpectNear(at(50, 0.9f), glm::vec3(8.0f, 10.0f, 0.0f));
}

// A start with no lead has one place: the thing stays there
TEST(ThrowLead, AThrowWithNoLeadStaysAtItsStart)
{
	const Flight flight {.places = {{.axes = glm::mat3(1.0f), .centre = {3.0f, 4.0f, 5.0f}}}, .sentTurn = 7};
	ExpectNear(DrawnPose(flight, 7, 0.6f).origin, glm::vec3(3.0f, 4.0f, 5.0f));
	ExpectNear(DrawnPose(flight, 8, 0.1f).origin, glm::vec3(3.0f, 4.0f, 5.0f));
}

// Between two turns the axes are blended, then made unit axes again
TEST(ThrowLead, TheBlendedAxesAreUnitAxes)
{
	glm::mat3 turned(0.0f);
	turned[0] = {0.0f, 0.0f, -1.0f};
	turned[1] = {0.0f, 1.0f, 0.0f};
	turned[2] = {1.0f, 0.0f, 0.0f};
	const Flight flight {
	    .places = {{.axes = glm::mat3(1.0f), .centre = {}}, {.axes = turned, .centre = {}}},
	    .sentTurn = 0,
	};
	const auto axes = DrawnPose(flight, 0, 0.5f).axes;
	for (int i = 0; i < 3; ++i)
	{
		EXPECT_NEAR(glm::length(axes[i]), 1.0f, 1.0e-3f);
	}
	EXPECT_NEAR(axes[0].x, 0.7071f, 1.0e-3f);
	EXPECT_NEAR(axes[0].z, -0.7071f, 1.0e-3f);
}
