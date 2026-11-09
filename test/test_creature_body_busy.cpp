/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureBodyActivity.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureRoute.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "Locator.h"

#define LOCATOR_IMPLEMENTATIONS

#include "ECS/Systems/Implementations/CreatureBodyBusy.h"
#include "ECS/Systems/Implementations/CreatureObjectActionSystem.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using creature_body::Activity;
using creature_object_actions::Kind;
using creature_object_actions::Status;
using Phase = CreatureObjectAction::Phase;

namespace
{
/// Legs that only tell whether they move
class Legs final: public CreatureLocomotionSystemInterface
{
public:
	void ProcessTurn() override {}
	void Update(float) override {}
	void ShowTurnEnd(entt::entity) override {}
	MoveResult MoveTo(entt::entity, glm::vec2, Pace, float, float) override { return MoveResult::Busy; }
	MoveResult LeadTo(entt::entity, glm::vec2, float, float) override { return MoveResult::Busy; }
	MoveResult MoveToObject(entt::entity, entt::entity, Pace, float) override { return MoveResult::Busy; }
	MoveResult Follow(entt::entity, entt::entity, float, Pace) override { return MoveResult::Busy; }
	MoveResult FleeFrom(entt::entity, glm::vec2, Pace) override { return MoveResult::Busy; }
	bool TurnToFace(entt::entity, glm::vec2) override { return false; }
	bool TurnToHeading(entt::entity, GameHeading) override { return false; }
	bool TurnToFaceAtOnce(entt::entity, glm::vec2) override { return false; }
	bool TurnToHeadingAtOnce(entt::entity, GameHeading) override { return false; }
	void Stop(entt::entity) override {}
	void StopAtOnce(entt::entity) override {}
	void ResetPace(entt::entity) override {}
	void Place(entt::entity, glm::vec3) override {}
	void PlaceWithoutWalking(entt::entity, glm::vec3) override {}
	[[nodiscard]] bool IsMoving(entt::entity) const override { return moving; }
	[[nodiscard]] float KeptFraction(entt::entity) const override { return 0.0f; }
	[[nodiscard]] const creature_route::WalkableLand& GetWalkableLand() override
	{
		throw std::logic_error("the body test does not ask for the walkable land");
	}
	[[nodiscard]] std::vector<creature_route::Circle> ThingCirclesIn(entt::entity, glm::ivec2) override { return {}; }

	bool moving {false};
};

/// A creature with an animated body, its legs and its hands
class CreatureBodyBusyTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<Registry>();
		creature = Locator::entitiesRegistry::value().Create();
		Locator::entitiesRegistry::value().Assign<CreatureAnimation>(creature);
	}
	void TearDown() override { Locator::entitiesRegistry::reset(); }

	void Give(CreatureObjectAction action)
	{
		Locator::entitiesRegistry::value().AssignOrReplace<CreatureObjectAction>(creature, action);
	}
	[[nodiscard]] Activity Now() const
	{
		return creature_body_busy::ActivityOf(Locator::entitiesRegistry::value(), creature, &legs, &hands);
	}
	[[nodiscard]] bool Busy() const { return creature_body::PerformsBodyAction(Now()); }

	Legs legs;
	CreatureObjectActionSystem hands;
	entt::entity creature {entt::null};
};
} // namespace

// The body is free only when it stands. Each of the three parts makes it busy alone.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureBodyActivity, TheBodyIsFreeOnlyWhenItStands)
{
	using creature_body::ActsBesidesMoving;
	using creature_body::PerformsBodyAction;
	EXPECT_FALSE(PerformsBodyAction({}));
	EXPECT_TRUE(PerformsBodyAction({.bodyPlays = true}));
	EXPECT_TRUE(PerformsBodyAction({.objectActs = true}));
	EXPECT_TRUE(PerformsBodyAction({.moving = true}));
	EXPECT_FALSE(ActsBesidesMoving({.moving = true}));
	EXPECT_TRUE(ActsBesidesMoving({.bodyPlays = true, .moving = true}));
	EXPECT_TRUE(ActsBesidesMoving({.objectActs = true, .moving = true}));
}

// The body starts an action on a thing when it plays its animations, or when it turns on the spot to reach, to throw
// or to point
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureBodyActivity, TheBodyStartsAnActionOnAThingWithItsAnimationsOrItsTurn)
{
	using creature_object_actions::BodyStarted;
	for (const auto kind : {Kind::PickUp, Kind::PutDown, Kind::Discard, Kind::Eat, Kind::Keep, Kind::Throw, Kind::Destroy,
	                        Kind::Point, Kind::Catch, Kind::TakeFromHand, Kind::Kick})
	{
		EXPECT_TRUE(BodyStarted(kind, true, false)) << creature_object_actions::Name(kind);
		EXPECT_FALSE(BodyStarted(kind, false, false)) << creature_object_actions::Name(kind);
	}
	for (const auto kind : {Kind::PickUp, Kind::Destroy, Kind::Throw, Kind::Point})
	{
		EXPECT_TRUE(BodyStarted(kind, false, true)) << creature_object_actions::Name(kind);
	}
	for (const auto kind : {Kind::Eat, Kind::Kick, Kind::Catch, Kind::TakeFromHand})
	{
		EXPECT_FALSE(BodyStarted(kind, false, true)) << creature_object_actions::Name(kind);
	}
}

// A creature that stands, with nothing in its hands to do, is free
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureBodyBusyTest, AStandingCreatureIsFree)
{
	EXPECT_FALSE(Busy());
}

// An action of the body, as a sit or a faint, makes it busy
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureBodyBusyTest, AnActionOfTheBodyMakesItBusy)
{
	auto& animation = Locator::entitiesRegistry::value().Get<CreatureAnimation>(creature);
	animation.body = *creature_layers::PlaySequence(animation.body, 1, 2, 3);
	EXPECT_TRUE(Now().bodyPlays);
	EXPECT_TRUE(Busy());
}

// A walk, a wait for a route and a turn on the spot make it busy
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureBodyBusyTest, AMoveMakesItBusy)
{
	legs.moving = true;
	EXPECT_TRUE(Busy());
	EXPECT_FALSE(creature_body::ActsBesidesMoving(Now()));
}

// Each action on a thing that the body plays makes it busy, a catch and a point included
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureBodyBusyTest, EachActionOnAThingThatPlaysMakesItBusy)
{
	for (const auto kind : {Kind::PickUp, Kind::PutDown, Kind::Discard, Kind::Eat, Kind::Keep, Kind::Throw, Kind::Destroy,
	                        Kind::Point, Kind::Catch, Kind::TakeFromHand, Kind::Kick})
	{
		Give({.kind = kind, .phase = Phase::Playing, .status = Status::Running});
		EXPECT_TRUE(Busy()) << creature_object_actions::Name(kind);
		Give({.kind = kind, .phase = Phase::Playing, .status = Status::Contact});
		EXPECT_TRUE(Busy()) << creature_object_actions::Name(kind);
	}
}

// A point that holds its arm while its plan goes on is a body action. It does not play on alone, but the body is busy.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureBodyBusyTest, APointThatHoldsItsArmMakesItBusy)
{
	Give({.kind = Kind::Point, .phase = Phase::Playing, .status = Status::Running});
	EXPECT_FALSE(hands.PlaysOnAlone(creature));
	EXPECT_TRUE(hands.BodyActs(creature));
}

// A turn on the spot to reach a thing is a part of the pick-up: the body is busy from the turn until the reach ends
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureBodyBusyTest, ATurnToReachIsAPartOfThePickUp)
{
	Give({.kind = Kind::PickUp, .phase = Phase::Approach, .status = Status::Running, .turnedToReach = true});
	EXPECT_TRUE(Busy());
	Give({.kind = Kind::PickUp, .phase = Phase::Approach, .status = Status::Running});
	EXPECT_FALSE(Busy());
}

// An action that ended or failed does not make the body busy
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureBodyBusyTest, AnActionThatEndedLeavesTheBodyFree)
{
	Give({.kind = Kind::Eat, .phase = Phase::Playing, .status = Status::Done});
	EXPECT_FALSE(Busy());
	Give({.kind = Kind::Throw, .phase = Phase::Playing, .status = Status::Failed});
	EXPECT_FALSE(Busy());
}

// With no legs, no hands and no animated body, nothing is busy
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureBodyBusyTest, NoPartsMeanAFreeBody)
{
	Give({.kind = Kind::Eat, .phase = Phase::Playing, .status = Status::Running});
	legs.moving = true;
	Locator::entitiesRegistry::value().Remove<CreatureAnimation>(creature);
	EXPECT_FALSE(creature_body::PerformsBodyAction(
	    creature_body_busy::ActivityOf(Locator::entitiesRegistry::value(), creature, nullptr, nullptr)));
}
