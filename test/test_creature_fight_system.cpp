/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/CreatureFightSystem.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace fight = openblack::creature_fight;

namespace
{
/// Two creatures duelling in an arena on flat land, neither with a body to click on
class CreatureFightSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<Registry>();
		auto& registry = Locator::entitiesRegistry::value();
		first = MakeCreature(registry, PlayerNames::PLAYER_ONE, {-15.0f, 0.0f, 0.0f});
		second = MakeCreature(registry, PlayerNames::PLAYER_TWO, {15.0f, 0.0f, 0.0f});
		for (const auto& [self, other] : {std::pair(first, second), std::pair(second, first)})
		{
			registry.Assign<CreatureFighting>(self, CreatureFighting {
			                                            .stage = CreatureFighting::Stage::Duel,
			                                            .opponent = other,
			                                            .arena = {.centre = {0.0f, 0.0f}, .radius = 40.0f},
			                                            .madeArena = self == first,
			                                        });
		}
	}
	void TearDown() override { Locator::entitiesRegistry::reset(); }

	static entt::entity MakeCreature(Registry& registry, PlayerNames owner, glm::vec3 at)
	{
		const auto creature = registry.Create();
		registry.Assign<Transform>(creature, at, glm::mat3(1.0f), glm::vec3(1.0f));
		auto& body = registry.Assign<Creature>(creature);
		body.owner = owner;
		registry.Assign<CreatureAnimation>(creature);
		registry.Assign<CreatureLocomotion>(creature);
		return creature;
	}

	/// A press looking down at a point on the ground
	bool PressGround(glm::vec2 point, fight::Button button, uint32_t milliseconds = 0, uint32_t turn = 0)
	{
		return fights.Press({point.x, 100.0f, point.y}, {0.0f, -1.0f, 0.0f}, button, milliseconds, turn);
	}

	[[nodiscard]] const fight::MoveQueue& QueueOf(entt::entity creature) const
	{
		return Locator::entitiesRegistry::value().Get<const CreatureFighting>(creature).fighter.queue;
	}

	ecs::systems::CreatureFightSystem fights;
	entt::entity first {entt::null};
	entt::entity second {entt::null};
};
} // namespace

TEST_F(CreatureFightSystemTest, ThePlayersFighterIsTheirFirstCreature)
{
	EXPECT_EQ(fights.PlayersFighter(), first);
	// A creature the player got later doesn't take its place
	auto& registry = Locator::entitiesRegistry::value();
	MakeCreature(registry, PlayerNames::PLAYER_ONE, {100.0f, 0.0f, 0.0f});
	EXPECT_EQ(fights.PlayersFighter(), first);
	EXPECT_EQ(fights.HandTip(first), fight::Tip::Block);
	EXPECT_EQ(fights.HandTip(second), fight::Tip::Attack);
	EXPECT_EQ(fights.HandTip(std::nullopt), fight::Tip::Manoeuvre);
}

TEST_F(CreatureFightSystemTest, TheMoveButtonReplacesAndTheActionButtonQueues)
{
	EXPECT_TRUE(PressGround({-15.0f, 10.0f}, fight::Button::Action));
	fights.Release(0, 0);
	EXPECT_TRUE(PressGround({-15.0f, -10.0f}, fight::Button::Action));
	fights.Release(0, 0);
	EXPECT_EQ(QueueOf(first).Size(), 2u);
	EXPECT_TRUE(PressGround({-25.0f, 0.0f}, fight::Button::Move));
	fights.Release(0, 0);
	EXPECT_EQ(QueueOf(first).Size(), 1u);
	// The ground beyond the arena isn't the fight's
	EXPECT_FALSE(PressGround({45.0f, 0.0f}, fight::Button::Action));
	EXPECT_EQ(QueueOf(first).Size(), 1u);
}

TEST_F(CreatureFightSystemTest, ANewLandForgetsThePressAndTheView)
{
	EXPECT_TRUE(PressGround({-15.0f, 10.0f}, fight::Button::Move, 100, 3));
	EXPECT_TRUE(fights.IsPressed());
	fights.SetFightExit(false);
	fights.Reset();
	EXPECT_FALSE(fights.IsPressed());
	EXPECT_FALSE(fights.IsCameraOnFight());
	EXPECT_TRUE(fights.GetFightExit());
	// The creatures of the old land are gone; letting go of the button touches nothing
	Locator::entitiesRegistry::value().Destroy(first);
	fights.Release(400, 5);
	EXPECT_FALSE(fights.IsPressed());
}
