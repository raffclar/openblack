/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>

#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "Graphics/ThrowLeadShadow.h"

using namespace openblack::graphics;
using throw_lead_shadow::Step;

TEST(ThrowLeadShadow, LeadIsCastFromStraightAboveOntoTheHeightOfTheThing)
{
	const auto cast = throw_lead_shadow::LeadCast({1500.0f, 82.5f, 1200.0f}, 40.0f);
	EXPECT_FLOAT_EQ(cast.light.x, 1500.0f);
	EXPECT_FLOAT_EQ(cast.light.y, 82.5f + 15000.0f);
	EXPECT_FLOAT_EQ(cast.light.z, 1200.0f);
	EXPECT_FLOAT_EQ(cast.level, 82.5f);
	EXPECT_FLOAT_EQ(cast.ground, 40.0f);
}

TEST(ThrowLeadShadow, KeptShadowKeepsItsLightAndLevel)
{
	const auto last = throw_lead_shadow::LeadCast({10.0f, 60.0f, 20.0f}, 30.0f);
	const auto kept = throw_lead_shadow::KeptCast(last, 12.0f);
	EXPECT_EQ(kept.light, last.light);
	EXPECT_FLOAT_EQ(kept.level, last.level);
}

TEST(ThrowLeadShadow, KeptShadowTakesTheLandUnderTheThingNow)
{
	const auto last = throw_lead_shadow::LeadCast({10.0f, 60.0f, 20.0f}, 30.0f);
	EXPECT_FLOAT_EQ(throw_lead_shadow::KeptCast(last, 12.0f).ground, 12.0f);
}

TEST(ThrowLeadShadow, KeptShadowOfAThingThatIsGoneKeepsItsLastLand)
{
	const auto last = throw_lead_shadow::LeadCast({10.0f, 60.0f, 20.0f}, 30.0f);
	const auto moved = throw_lead_shadow::KeptCast(last, 18.0f);
	EXPECT_FLOAT_EQ(throw_lead_shadow::KeptCast(moved, std::nullopt).ground, 18.0f);
}

TEST(ThrowLeadShadow, LeadMakesTheShadowAgain)
{
	EXPECT_EQ(throw_lead_shadow::StepOf(true, 255, false), Step::Lead);
	EXPECT_EQ(throw_lead_shadow::StepOf(true, 1, true), Step::Lead);
}

TEST(ThrowLeadShadow, LeadTooFarFromTheCameraRemovesTheShadow)
{
	EXPECT_EQ(throw_lead_shadow::StepOf(true, 0, true), Step::None);
	EXPECT_EQ(throw_lead_shadow::StepOf(true, 0, false), Step::None);
}

TEST(ThrowLeadShadow, ShadowStaysAfterTheLead)
{
	EXPECT_EQ(throw_lead_shadow::StepOf(false, 0, true), Step::Kept);
	EXPECT_EQ(throw_lead_shadow::StepOf(false, 255, true), Step::Kept);
}

TEST(ThrowLeadShadow, NoShadowBeforeTheFirstThrow)
{
	EXPECT_EQ(throw_lead_shadow::StepOf(false, 255, false), Step::None);
}

TEST(ThrowLeadShadow, HandShadowTakesTheThingWhereTheFrameBeforeDrewItOnTheLead)
{
	const auto now = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 20.0f, 30.0f));
	const auto before = glm::translate(glm::mat4(1.0f), glm::vec3(8.0f, 19.0f, 29.0f));
	EXPECT_EQ(throw_lead_shadow::HandShadowModel(now, true, before), before);
}

TEST(ThrowLeadShadow, HandShadowTakesTheThingWhereItIsNowOffTheLead)
{
	const auto now = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 20.0f, 30.0f));
	const auto before = glm::translate(glm::mat4(1.0f), glm::vec3(8.0f, 19.0f, 29.0f));
	EXPECT_EQ(throw_lead_shadow::HandShadowModel(now, false, before), now);
}

TEST(ThrowLeadShadow, HandShadowWithNoFrameBeforeTakesTheThingWhereItIsNow)
{
	const auto now = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 20.0f, 30.0f));
	EXPECT_EQ(throw_lead_shadow::HandShadowModel(now, true, std::nullopt), now);
}
