/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/Components/Town.h"
#include "ECS/Registry.h"
#include "ECS/WorldObjects.h"
#include "Locator.h"
#include "ScriptHeaders/ScriptPropertyRules.h"

using namespace openblack;

namespace
{
class TownOwnerProperty: public ::testing::Test
{
protected:
	void SetUp() override { Locator::entitiesRegistry::emplace<ecs::Registry>(); }
	void TearDown() override { Locator::entitiesRegistry::reset(); }
};
} // namespace

TEST_F(TownOwnerProperty, ATownIsItsOwnersAndScriptsSeeItsPlayerNumber)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto town = registry.Create();
	auto& component = registry.Assign<ecs::components::Town>(town, 4u, PlayerNames::NEUTRAL);
	EXPECT_EQ(ecs::world_objects::PlayerOf(town), PlayerNames::NEUTRAL);
	EXPECT_FLOAT_EQ(script::property_rules::PlayerProperty(ecs::world_objects::PlayerOf(town), false), 0.0f);
	// Won by player one, a script asking the town's player is answered 1
	component.owner = PlayerNames::PLAYER_ONE;
	EXPECT_EQ(ecs::world_objects::PlayerOf(town), PlayerNames::PLAYER_ONE);
	EXPECT_FLOAT_EQ(script::property_rules::PlayerProperty(ecs::world_objects::PlayerOf(town), false), 1.0f);
}
