/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <gtest/gtest.h>

#include "ECS/ScriptHighlight.h"
#include "ECS/Systems/HandTap.h"

// ScriptHighlight's pure rules, against the values of the original game.

using namespace openblack;
namespace sh = openblack::ecs::script_highlight;

TEST(ScriptHighlight, DidYouKnowIsRowOne)
{
	// the did-you-know is info row 1
	EXPECT_FALSE(sh::IsDidYouKnowInfo(0));
	EXPECT_TRUE(sh::IsDidYouKnowInfo(1));
	EXPECT_FALSE(sh::IsDidYouKnowInfo(2));
	EXPECT_FALSE(sh::IsDidYouKnowInfo(3));
}

TEST(ScriptHighlight, SpriteAlphaByRow)
{
	// the sprite's alpha by row
	EXPECT_EQ(sh::SpriteAlpha(0), 0x00);
	EXPECT_EQ(sh::SpriteAlpha(1), 0x32);
	EXPECT_EQ(sh::SpriteAlpha(2), 0x96);
	EXPECT_EQ(sh::SpriteAlpha(3), 0x64);
	EXPECT_EQ(sh::SpriteAlpha(4), 0x00);
}

TEST(ScriptHighlight, ScrollsGrowWithTheCameraDistance)
{
	// the whole metres bound to [10, 30], x 1 / 30
	EXPECT_FLOAT_EQ(sh::DistanceScale(1.0f, 3.0f, false), 10.0f * 0.0333333f);
	EXPECT_FLOAT_EQ(sh::DistanceScale(1.0f, 20.9f, false), 20.0f * 0.0333333f);
	EXPECT_FLOAT_EQ(sh::DistanceScale(2.0f, 500.0f, false), 30.0f * 2.0f * 0.0333333f);
	// a did-you-know keeps its scale
	EXPECT_FLOAT_EQ(sh::DistanceScale(1.5f, 500.0f, true), 1.5f);
}

TEST(ScriptHighlight, SpinWrapsByTruncation)
{
	// pi / 1000 a game ms; minus ftol(a / 2 pi) turns
	EXPECT_NEAR(sh::SpinAngle(0.0f, 100), 0.314159f, 1e-5f);
	EXPECT_NEAR(sh::SpinAngle(6.2f, 100), 6.2f + 0.314159f - 6.28319f, 1e-4f);
	// a negative angle is not brought into [0, 2 pi) (ftol truncates towards 0)
	EXPECT_NEAR(sh::SpinAngle(-1.0f, 0), -1.0f, 1e-6f);
}

TEST(ScriptHighlight, ChallengeIdsThatSave)
{
	// the challenges whose click saves the game
	for (uint32_t id = 0; id < 0x50; ++id)
	{
		const bool expected = id == 0x38 || id == 0x3B || id == 0x3C || id == 0x3D;
		EXPECT_EQ(sh::SavesGameWhenClicked(id), expected) << id;
	}
}

TEST(ScriptHighlight, ValidToTap)
{
	EXPECT_TRUE(sh::ValidToTap(true, false, 4143));
	EXPECT_FALSE(sh::ValidToTap(true, true, 0));
	EXPECT_FALSE(sh::ValidToTap(false, false, 53));
	EXPECT_TRUE(sh::ValidToTap(false, true, 53));
	EXPECT_FALSE(sh::ValidToTap(false, true, 0));
}

TEST(ScriptHighlight, TappedOutsideTheInfluence)
{
	namespace tap = openblack::ecs::hand_tap;
	// a highlight needs no influence: a valid one is tapped wherever the hand is
	EXPECT_TRUE(tap::SendsTap(false, false, true, false));
	EXPECT_TRUE(tap::SendsTap(true, false, true, false));
	// every other class needs the hand in the influence
	EXPECT_FALSE(tap::SendsTap(false, true, true, false));
	EXPECT_TRUE(tap::SendsTap(true, true, true, false));
	// never when it is not valid to tap or it is flagged cannot be picked up
	EXPECT_FALSE(tap::SendsTap(true, false, false, false));
	EXPECT_FALSE(tap::SendsTap(false, false, true, true));
	EXPECT_FALSE(tap::SendsTap(true, true, true, true));
}

TEST(ScriptHighlight, TapToolTip)
{
	EXPECT_EQ(sh::OverwriteTapToolTip(ObjectType::ScriptHighlight), 0u);
	EXPECT_EQ(sh::OverwriteTapToolTip(ObjectType::Creature), 0xEF2u);
}

TEST(ScriptHighlight, PulseOfProcessHighlights)
{
	// the highlights' pulse with the 100 ms turn: 0.5 rad a turn
	sh::Pulse pulse;
	sh::StepPulse(pulse, 100);
	EXPECT_NEAR(pulse.phase, 0.5f, 1e-6f);
	EXPECT_FLOAT_EQ(pulse.previous, 0.0f);
	EXPECT_NEAR(pulse.value, (1.0f - std::cos(0.5f)) * 0.5f, 1e-6f);
	const float first = pulse.value;
	sh::StepPulse(pulse, 100);
	EXPECT_FLOAT_EQ(pulse.previous, first);
	// the phase loses 2 pi once it is above
	sh::Pulse wrap {6.2f, 0.0f, 0.0f};
	sh::StepPulse(wrap, 100);
	EXPECT_NEAR(wrap.phase, 6.7f - 6.28319f, 1e-5f);
}

TEST(ScriptHighlight, ActivePulseAndGlow)
{
	// lerp, bound to [0, 1], x 0.6 + 0.4
	const sh::Pulse pulse {0.0f, 1.0f, 0.0f};
	EXPECT_NEAR(sh::ActivePulse(pulse, 0.5f), 0.7f, 1e-6f);
	EXPECT_NEAR(sh::ActivePulse(pulse, 0.0f), 0.4f, 1e-6f);
	const sh::Pulse over {0.0f, 3.0f, 0.0f};
	EXPECT_NEAR(sh::ActivePulse(over, 0.9f), 1.0f, 1e-6f);
	// silver's fixed colour, the others 0x50 alpha only at a pulse of 1
	EXPECT_EQ(sh::ActiveGlowArgb(2, 0.7f), 0x14B4DCFFu);
	EXPECT_EQ(sh::ActiveGlowArgb(3, 0.7f), 0x00FFFF00u);
	EXPECT_EQ(sh::ActiveGlowArgb(3, 1.0f), 0x50FFFF00u);
	EXPECT_EQ(sh::ActiveGlowArgb(0, 1.0f), 0x50FFFF00u);
}

TEST(ScriptHighlight, DidYouKnowReadLists)
{
	sh::did_you_know_read::Clear();
	EXPECT_FALSE(sh::did_you_know_read::IsRead(4143, DykCategory::Misc));
	sh::did_you_know_read::MarkRead(4143, DykCategory::Misc);
	EXPECT_TRUE(sh::did_you_know_read::IsRead(4143, DykCategory::Misc));
	EXPECT_FALSE(sh::did_you_know_read::IsRead(4143, DykCategory::Creature)); // one list a category
	EXPECT_EQ(sh::did_you_know_read::Total(), 1u);
	// at most 48 a category
	for (uint32_t text = 0; text < 60; ++text)
	{
		sh::did_you_know_read::MarkRead(10000 + text, DykCategory::Navigation);
	}
	EXPECT_EQ(sh::did_you_know_read::Total(), 1u + 48u);
	EXPECT_FALSE(sh::did_you_know_read::IsRead(10055, DykCategory::Navigation));
	// past DYK_CATEGORY 4: never read
	EXPECT_FALSE(sh::did_you_know_read::IsRead(4143, static_cast<DykCategory>(7)));
	sh::did_you_know_read::Clear();
	EXPECT_EQ(sh::did_you_know_read::Total(), 0u);
}
