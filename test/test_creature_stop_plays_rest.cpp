/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>

#include <gtest/gtest.h>

#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureObjectActions.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureCarried.h"
#include "ECS/Registry.h"
#include "ECS/ThingMapPlace.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace actions = openblack::creature_object_actions;
using creature_mind::ObjectOrder;
using creature_mind::Step;

namespace
{
constexpr float k_Turn = 0.1f;
constexpr uint32_t k_Thing = 4;

uint32_t First(uint32_t /*range*/)
{
	return 0;
}

[[nodiscard]] creature_mind::Senses Hands(creature_mind::HandsState hands, bool touched)
{
	return {.seconds = k_Turn, .bodyBusy = true, .hands = hands, .handsTouched = touched};
}

[[nodiscard]] Step Stomp()
{
	return {.kind = Step::Kind::Object,
	        .effect = creature_mind::Effect::Stomped,
	        .order = {.kind = ObjectOrder::Kind::Destroy, .object = k_Thing}};
}

[[nodiscard]] Step PickUp()
{
	return {.kind = Step::Kind::Object, .order = {.kind = ObjectOrder::Kind::PickUp, .object = k_Thing}};
}

/// A creature that holds a thing in its hand, with the action of its hands
class CreatureCarriedTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		creature = registry.Create();
		thing = registry.Create();
		registry.Assign<CreatureHeldObject>(creature, CreatureHeldObject {.object = thing});
		registry.Assign<HeldByCreature>(thing, HeldByCreature {.creature = creature});
	}

	void Act(actions::Kind kind, actions::Status status, bool released)
	{
		auto& action = registry.Assign<CreatureObjectAction>(creature);
		action.kind = kind;
		action.status = status;
		action.eventDone = released;
	}

	Registry registry;
	entt::entity creature {entt::null};
	entt::entity thing {entt::null};
};
} // namespace

// While the creature prepares a throw, it carries nothing. Before the throw and after the release, it carries what is in
// its hand. Other actions never change what it carries.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureCarriedRules, APreparedThrowCarriesNothing)
{
	EXPECT_TRUE(actions::CarriesHeld(std::nullopt, actions::Status::Running, false));
	EXPECT_FALSE(actions::CarriesHeld(actions::Kind::Throw, actions::Status::Running, false));
	EXPECT_TRUE(actions::CarriesHeld(actions::Kind::Throw, actions::Status::Running, true));
	EXPECT_TRUE(actions::CarriesHeld(actions::Kind::Throw, actions::Status::Done, false));
	EXPECT_TRUE(actions::CarriesHeld(actions::Kind::Throw, actions::Status::Failed, false));
	EXPECT_TRUE(actions::CarriesHeld(actions::Kind::Eat, actions::Status::Running, false));
	EXPECT_TRUE(actions::CarriesHeld(actions::Kind::Keep, actions::Status::Running, false));
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureCarriedTest, TheCreatureCarriesWhatItsHandHolds)
{
	EXPECT_EQ(creature_carried::CarriedBy(registry, creature), thing);
	EXPECT_EQ(creature_carried::CarrierOf(registry, thing), creature);
	EXPECT_TRUE(creature_carried::Carries(registry, creature, thing));
	Act(actions::Kind::Keep, actions::Status::Running, false);
	EXPECT_EQ(creature_carried::CarriedBy(registry, creature), thing);
}

// The hand keeps the thing until the release, but the creature and the thing see no carry
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureCarriedTest, APreparedThrowCarriesNothingUntilTheRelease)
{
	Act(actions::Kind::Throw, actions::Status::Running, false);
	EXPECT_FALSE(creature_carried::CarriedBy(registry, creature).has_value());
	EXPECT_FALSE(creature_carried::CarrierOf(registry, thing).has_value());
	EXPECT_FALSE(creature_carried::Carries(registry, creature, thing));
	registry.Get<CreatureObjectAction>(creature).eventDone = true;
	EXPECT_EQ(creature_carried::CarriedBy(registry, creature), thing);
}

// The script test "in creature hand" reads the hand. It sees the thing also while the creature prepares a throw.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureCarriedTest, TheHandHoldsTheThingWhileTheCreaturePreparesAThrow)
{
	EXPECT_EQ(creature_carried::InHandOf(registry, creature), thing);
	Act(actions::Kind::Throw, actions::Status::Running, false);
	EXPECT_FALSE(creature_carried::CarriedBy(registry, creature).has_value());
	EXPECT_EQ(creature_carried::InHandOf(registry, creature), thing);
	registry.Remove<CreatureHeldObject>(creature);
	EXPECT_FALSE(creature_carried::InHandOf(registry, creature).has_value());
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureCarriedTest, AnEmptyHandCarriesNothing)
{
	registry.Remove<CreatureHeldObject>(creature);
	EXPECT_FALSE(creature_carried::CarriedBy(registry, creature).has_value());
	EXPECT_FALSE(creature_carried::CarrierOf(registry, thing).has_value());
	const auto other = registry.Create();
	EXPECT_FALSE(creature_carried::CarrierOf(registry, other).has_value());
}

// At each turn of the creature, the thing that it carries takes the place of the creature, not the place of its hand.
// The thing keeps its own turn.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureCarriedTest, TheCarriedThingTakesThePlaceOfTheCreature)
{
	const glm::mat3 turned {0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f};
	registry.Assign<Transform>(creature, glm::vec3(1234.5f, 17.25f, 2345.75f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Transform>(thing, glm::vec3(1200.0f, 20.0f, 2300.0f), turned, glm::vec3(0.5f));
	creature_carried::FollowCarrier(registry, creature);
	const auto& at = registry.Get<const Transform>(thing);
	EXPECT_EQ(at.position, glm::vec3(1234.5f, 17.25f, 2345.75f));
	EXPECT_EQ(at.rotation, turned);
	EXPECT_EQ(at.scale, glm::vec3(0.5f));
	const auto place = thing_map_place::Of(registry, thing);
	const auto carrierPlace = thing_map_place::Of(registry, creature);
	EXPECT_EQ(place.x, carrierPlace.x);
	EXPECT_EQ(place.z, carrierPlace.z);
}

// While the creature prepares a throw it carries nothing, so the thing in its hand stays where it is. An empty hand moves
// nothing.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureCarriedTest, AThingThatTheCreatureDoesNotCarryStaysWhereItIs)
{
	const glm::vec3 where {1200.0f, 20.0f, 2300.0f};
	registry.Assign<Transform>(creature, glm::vec3(1234.5f, 17.25f, 2345.75f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Transform>(thing, where, glm::mat3(1.0f), glm::vec3(1.0f));
	Act(actions::Kind::Throw, actions::Status::Running, false);
	creature_carried::FollowCarrier(registry, creature);
	EXPECT_EQ(registry.Get<const Transform>(thing).position, where);
	registry.Remove<CreatureObjectAction>(creature);
	registry.Remove<CreatureHeldObject>(creature);
	creature_carried::FollowCarrier(registry, creature);
	EXPECT_EQ(registry.Get<const Transform>(thing).position, where);
}

// After the blow, the stomp crushes the thing on each turn until the body has played the stomp. Only a last step
// counts the kill.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureStopPlaysRest, AStompCrushesOnEachTurnAfterTheBlow)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Eat, {Stomp(), PickUp()});
	auto commands = creature_mind::Think(mind, {.seconds = k_Turn}, First);
	ASSERT_TRUE(commands.object.has_value());
	EXPECT_EQ(commands.object->kind, ObjectOrder::Kind::Destroy);
	// Before the blow, no crush
	commands = creature_mind::Think(mind, Hands(creature_mind::HandsState::Busy, false), First);
	EXPECT_FALSE(commands.stomp.has_value());
	for (int turn = 0; turn < 3; ++turn)
	{
		commands = creature_mind::Think(mind, Hands(creature_mind::HandsState::Busy, true), First);
		ASSERT_TRUE(commands.stomp.has_value());
		EXPECT_EQ(commands.stomp->object, k_Thing);
		EXPECT_FALSE(commands.stomp->lastStep);
		EXPECT_FALSE(commands.noLookPoint);
	}
	// The body has played the stomp: no more crush
	commands = creature_mind::Think(mind, Hands(creature_mind::HandsState::Done, false), First);
	EXPECT_FALSE(commands.stomp.has_value());
	EXPECT_EQ(mind.step, 1u);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureStopPlaysRest, AStompThatIsTheLastStepCountsTheKill)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Told, {Stomp()});
	ASSERT_TRUE(creature_mind::Think(mind, {.seconds = k_Turn}, First).object.has_value());
	const auto commands = creature_mind::Think(mind, Hands(creature_mind::HandsState::Busy, true), First);
	ASSERT_TRUE(commands.stomp.has_value());
	EXPECT_TRUE(commands.stomp->lastStep);
}

// After the hold of a pick-up, the head looks at no point until the body has played the pick-up
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureStopPlaysRest, APickUpLooksAtNoPointAfterTheHold)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Told, {PickUp()});
	ASSERT_TRUE(creature_mind::Think(mind, {.seconds = k_Turn}, First).object.has_value());
	auto commands = creature_mind::Think(mind, Hands(creature_mind::HandsState::Busy, false), First);
	EXPECT_FALSE(commands.noLookPoint);
	commands = creature_mind::Think(mind, Hands(creature_mind::HandsState::Busy, true), First);
	EXPECT_TRUE(commands.noLookPoint);
	EXPECT_FALSE(commands.stomp.has_value());
	// A plan that stops ends the step, and the look point comes back
	creature_mind::Plan(mind, creature_mind::Activity::Told, {Stomp()});
	commands = creature_mind::Think(mind, {.seconds = k_Turn}, First);
	EXPECT_FALSE(commands.noLookPoint);
}
