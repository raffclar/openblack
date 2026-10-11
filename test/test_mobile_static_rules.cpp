/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/MobileStaticRules.h"

using namespace openblack;
using namespace openblack::ecs;

TEST(MobileStaticRules, RowsOfTheRockClassAreRocks)
{
	// The land's boulders: a boulder kind whose row is of the rock class, made by the land's script
	EXPECT_TRUE(mobile_static_rules::IsScriptRock(MobileStaticInfo {25}, MobileStaticInfo::Rock));
	// The everlasting pile's rocks
	EXPECT_TRUE(mobile_static_rules::IsScriptRock(MobileStaticInfo::Rock, MobileStaticInfo::Rock));
}

TEST(MobileStaticRules, OtherRowsAreNotRocks)
{
	// A fence, a singing stone or a kind of no class stays a mobile static, whatever the script asked for
	EXPECT_FALSE(mobile_static_rules::IsScriptRock(MobileStaticInfo::CeltFenceShort, MobileStaticInfo::CeltFenceShort));
	EXPECT_FALSE(mobile_static_rules::IsScriptRock(MobileStaticInfo::SingingStone_1, MobileStaticInfo::SingingStone_1));
	EXPECT_FALSE(mobile_static_rules::IsScriptRock(MobileStaticInfo {25}, MobileStaticInfo::None));
	EXPECT_FALSE(mobile_static_rules::IsScriptRock(MobileStaticInfo::Rock, MobileStaticInfo::DeadTree));
}

TEST(MobileStaticRules, ABonfireIsNeverARock)
{
	EXPECT_FALSE(mobile_static_rules::IsScriptRock(MobileStaticInfo::Bonfire, MobileStaticInfo::Rock));
	EXPECT_FALSE(mobile_static_rules::IsScriptRock(MobileStaticInfo::Bonfire, MobileStaticInfo::Bonfire));
}
