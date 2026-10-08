/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The scripts' hit object: what the physics' turn end records for GET_HIT_OBJECT and GET_OBJECT_WHICH_HIT.

#include <gtest/gtest.h>

#include "ECS/ScriptHitObject.h"

using namespace openblack::ecs;

TEST(ScriptHitObject, eachSideKeptOnlyWhileAvailable)
{
	script_hit::ScriptHitObjects objects;
	const auto rock = static_cast<entt::entity>(7);
	const auto villager = static_cast<entt::entity>(9);
	script_hit::Record(objects, villager, true, rock, true);
	EXPECT_EQ(objects.hit, villager);
	EXPECT_EQ(objects.hitter, rock);
	// a hit thing that has just gone clears its side only
	script_hit::Record(objects, villager, false, rock, true);
	EXPECT_EQ(objects.hit, entt::entity {entt::null});
	EXPECT_EQ(objects.hitter, rock);
	// clearing: both none
	script_hit::Record(objects, entt::null, true, entt::null, true);
	EXPECT_EQ(objects.hit, entt::entity {entt::null});
	EXPECT_EQ(objects.hitter, entt::entity {entt::null});
}
