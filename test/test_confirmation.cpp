/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The confirmation "yes" for the angle / pitch of START_ANGLE_SOUND 285 / 348 (docs/bw1-notes/intro.md):
// the camera's smoothing of the angle and the per-turn step that picks the sample.

#include <cmath>

#include <limits>

#include <gtest/gtest.h>

#include "Audio/Services/Confirmation.h"

namespace confirmation = openblack::audio::confirmation;

namespace
{
int RunStep(confirmation::State& state, uint32_t turn, float draw, uint32_t pick, int* draws = nullptr)
{
	return confirmation::Step(
	    state, turn,
	    [draw, draws](float) {
		    if (draws != nullptr)
		    {
			    ++*draws;
		    }
		    return draw;
	    },
	    [pick](int32_t n) { return pick % static_cast<uint32_t>(n); });
}
} // namespace

TEST(Confirmation, Smoothing)
{
	confirmation::StartAngleSound(true); // the angle = 0
	confirmation::FeedAngle(1.0f, 0.5f); // (2 - 0) x 0.4 + 0
	EXPECT_FLOAT_EQ(confirmation::Angle(), 0.8f);
	confirmation::FeedAngle(0.0f, 0.5f); // no turn this frame: (0 - 0.8) x 0.4 + 0.8
	EXPECT_FLOAT_EQ(confirmation::Angle(), 0.48f);
	confirmation::StartAngleSound(false);
	EXPECT_FALSE(confirmation::Get().active);
}

TEST(Confirmation, Step)
{
	float value = 0.0f;
	confirmation::State state;
	EXPECT_EQ(RunStep(state, 1000, 0.0f, 0), 0); // not started
	confirmation::Start(state, &value, confirmation::k_AngleRange);
	// at the range: "better" once more than 100 turns went by
	value = 7.0f;
	EXPECT_EQ(RunStep(state, 100, 0.0f, 0), 0);
	EXPECT_EQ(RunStep(state, 101, 0.0f, 0), confirmation::k_Better);
	EXPECT_EQ(state.lastSay, 101u);
	EXPECT_EQ(state.lastBetter, 101u);
	EXPECT_EQ(RunStep(state, 150, 0.0f, 0), 0);
	// half the range: "yes" when more than 20 turns since the last and |v| beats the draw, drawn only then
	value = 3.5f;
	int draws = 0;
	EXPECT_EQ(RunStep(state, 121, 0.0f, 0, &draws), 0); // 20 turns: no draw
	EXPECT_EQ(draws, 0);
	EXPECT_EQ(RunStep(state, 122, 0.6f, 0, &draws), 0); // 0.5 <= 0.6
	EXPECT_EQ(draws, 1);
	EXPECT_EQ(RunStep(state, 122, 0.4f, 13, &draws), confirmation::k_Yes + 13);
	EXPECT_EQ(state.lastSay, 122u);
	// turning the other way says "yes" too (the original's |v|)
	value = -3.5f;
	EXPECT_EQ(RunStep(state, 200, 0.1f, 2), confirmation::k_Yes + 2);
	// past the range on either side is the range
	value = -70.0f;
	EXPECT_EQ(RunStep(state, 300, 0.0f, 0), confirmation::k_Better);
	// a NaN compares as below -1 (as in the original's float compare): "better"
	value = std::numeric_limits<float>::quiet_NaN();
	EXPECT_EQ(RunStep(state, 500, 0.0f, 0), confirmation::k_Better);
	// still
	value = 0.0f;
	EXPECT_EQ(RunStep(state, 900, 0.0f, 0), 0);
}
