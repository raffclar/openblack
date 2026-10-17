/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Graphics/CreatureReflection.h"
#include "Graphics/MaterialStages.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
/// A creature that the game mirrors: the player's, low, on a near block in view, not fizzing
creature_reflection::Situation Mirrored()
{
	return {
	    .primaryCreature = true, .onBlock = true, .blockInView = true, .blockDistance = 100.0f, .height = 1.0f, .fizz = 0.0f};
}
} // namespace

TEST(CreatureReflection, TheColoursAreTheGames)
{
	EXPECT_EQ(creature_reflection::Colour(), glm::vec3(160.0f / 255.0f, 160.0f / 255.0f, 208.0f / 255.0f));
	EXPECT_EQ(creature_reflection::Added(), glm::vec3(0.0f, 0.0f, 48.0f / 255.0f));
}

TEST(CreatureReflection, OnlyThePrimaryCreatureIsMirrored)
{
	EXPECT_TRUE(creature_reflection::Shown(Mirrored()));
	auto other = Mirrored();
	other.primaryCreature = false;
	EXPECT_FALSE(creature_reflection::Shown(other));
}

TEST(CreatureReflection, TheBlockMustBeInViewAndNear)
{
	auto situation = Mirrored();
	situation.onBlock = false;
	EXPECT_FALSE(creature_reflection::Shown(situation));
	situation = Mirrored();
	situation.blockInView = false;
	EXPECT_FALSE(creature_reflection::Shown(situation));
	// 700 itself is near enough
	situation = Mirrored();
	situation.blockDistance = 700.0f;
	EXPECT_TRUE(creature_reflection::Shown(situation));
	situation.blockDistance = 700.0001f;
	EXPECT_FALSE(creature_reflection::Shown(situation));
}

TEST(CreatureReflection, TheBodyMustStandLowerThanSix)
{
	auto situation = Mirrored();
	situation.height = 5.9999f;
	EXPECT_TRUE(creature_reflection::Shown(situation));
	situation.height = 6.0f;
	EXPECT_FALSE(creature_reflection::Shown(situation));
	situation.height = -3.0f;
	EXPECT_TRUE(creature_reflection::Shown(situation));
}

TEST(CreatureReflection, AFifthFizzedOutStopsTheReflection)
{
	auto situation = Mirrored();
	situation.fizz = 0.19f;
	EXPECT_TRUE(creature_reflection::Shown(situation));
	situation.fizz = 0.2f;
	EXPECT_FALSE(creature_reflection::Shown(situation));
}

TEST(CreatureReflection, TheBlockUnderAPoint)
{
	EXPECT_EQ(creature_reflection::BlockUnder({1605.0f, 0.0f, 165.0f}), glm::ivec2(10, 1));
	// A tenth of a point a little west of the map truncates to the first cell
	EXPECT_EQ(creature_reflection::BlockUnder({-5.0f, 0.0f, 0.0f}), glm::ivec2(0, 0));
	EXPECT_FALSE(creature_reflection::BlockUnder({-10.0f, 0.0f, 0.0f}).has_value());
	EXPECT_EQ(creature_reflection::BlockUnder({5119.0f, 0.0f, 0.0f}), glm::ivec2(31, 0));
	EXPECT_FALSE(creature_reflection::BlockUnder({5120.0f, 0.0f, 0.0f}).has_value());
}

TEST(CreatureReflection, TheEyeballAddsTheEnvironmentMap)
{
	// The eyeball's material flags are 0x44: the map is added, and the texture repeats
	EXPECT_EQ(material_stages::EnvironmentStageOf(0x44), material_stages::EnvironmentStage::Added);
	EXPECT_TRUE(material_stages::TextureRepeats(0x44, false));
}
