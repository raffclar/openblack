/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <chrono>
#include <optional>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureRoute.h"
#include "Creature/CreatureScriptAgendas.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "Locator.h"

#define LOCATOR_IMPLEMENTATIONS

#include "ECS/Systems/Implementations/CreatureMindSystem.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// The hands of a creature that walks to a thing to pick it up. They record each stop.
class WalkingHands final: public CreatureObjectActionSystemInterface
{
public:
	void ProcessTurn() override {}
	void ActionTurn(entt::entity) override {}
	void AfterPoseTurn(entt::entity) override {}
	void UpdateHeld() override {}
	void ShowHeldAtTurnEnd() override {}
	bool PickUp(entt::entity, entt::entity) override { return false; }
	bool PutDown(entt::entity) override { return false; }
	bool Discard(entt::entity) override { return false; }
	bool DiscardWith(entt::entity, size_t) override { return false; }
	bool EatHeld(entt::entity) override { return false; }
	bool EatHeldOnly(entt::entity) override { return false; }
	bool Keep(entt::entity, size_t) override { return false; }
	bool Throw(entt::entity, const glm::vec3&) override { return false; }
	bool ThrowTaking(entt::entity, const glm::vec3&, float) override { return false; }
	bool ThrowAimed(entt::entity, const glm::vec3&, float) override { return false; }
	bool Destroy(entt::entity, entt::entity) override { return false; }
	bool Kick(entt::entity, entt::entity) override { return false; }
	bool PointAt(entt::entity, const glm::vec3&) override { return false; }
	bool Catch(entt::entity, entt::entity) override { return false; }
	void Cancel(entt::entity creature) override
	{
		cancelled.push_back(creature);
		state = State::Idle;
	}
	void Drop(entt::entity) override {}
	[[nodiscard]] State GetState(entt::entity) const override { return state; }
	[[nodiscard]] bool IsCatching(entt::entity) const override { return false; }
	[[nodiscard]] bool PlaysOnAlone(entt::entity) const override { return playsOn; }
	[[nodiscard]] std::optional<float> GetProgress(entt::entity) const override { return std::nullopt; }
	[[nodiscard]] std::optional<entt::entity> GetHeld(entt::entity) const override { return std::nullopt; }
	[[nodiscard]] std::optional<float> FoodValueOf(entt::entity) const override { return std::nullopt; }
	[[nodiscard]] bool CanPickUp(entt::entity, entt::entity) const override { return false; }
	[[nodiscard]] bool CanEat(entt::entity, entt::entity) const override { return false; }
	[[nodiscard]] bool CanEatHeld(entt::entity) const override { return false; }
	[[nodiscard]] bool CanDestroy(entt::entity) const override { return false; }

	State state {State::Busy};
	bool playsOn {false};
	std::vector<entt::entity> cancelled;
};

/// The legs of the creature. They record each stop of a walk.
class StoppingLegs final: public CreatureLocomotionSystemInterface
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
	void Stop(entt::entity creature) override { stopped.push_back(creature); }
	void StopAtOnce(entt::entity creature) override { stopped.push_back(creature); }
	void ResetPace(entt::entity) override {}
	void Place(entt::entity, glm::vec3) override {}
	void PlaceWithoutWalking(entt::entity, glm::vec3) override {}
	[[nodiscard]] bool IsMoving(entt::entity) const override { return false; }
	[[nodiscard]] float KeptFraction(entt::entity) const override { return 0.0f; }
	[[nodiscard]] const creature_route::WalkableLand& GetWalkableLand() override
	{
		throw std::logic_error("a script that takes control does not ask for the walkable land");
	}
	[[nodiscard]] std::vector<creature_route::Circle> ThingCirclesIn(entt::entity, glm::ivec2) override { return {}; }

	std::vector<entt::entity> stopped;
};

/// A creature that walks to a thing to pick it up when a script takes control of it
/// Records each creature whose fight is called off, and does nothing else
class CalledOffFights final: public CreatureFightSystemInterface
{
public:
	void ProcessTurn() override {}
	void Reset() override {}
	void Update(std::chrono::duration<float, std::milli> /*gameTime*/) override {}
	StartResult StartFight(entt::entity /*creature*/, entt::entity /*opponent*/) override { return StartResult::Busy; }
	void AbortFight(entt::entity /*creature*/) override {}
	void CallOff(entt::entity creature) override { calledOff.push_back(creature); }
	[[nodiscard]] bool IsFighting(entt::entity /*creature*/) const override { return true; }
	[[nodiscard]] std::optional<entt::entity> OpponentOf(entt::entity /*creature*/) const override { return std::nullopt; }
	bool QueueMove(entt::entity /*creature*/, const creature_fight::Move& /*move*/, bool /*replace*/) override { return false; }
	void ReleaseCharge(entt::entity /*creature*/, float /*heldMs*/) override {}
	void SetAutoFighting(entt::entity /*creature*/, bool /*autoFight*/) override {}
	[[nodiscard]] bool IsAutoFighting(entt::entity /*creature*/) const override { return false; }
	[[nodiscard]] std::optional<entt::entity> PlayersFighter() const override { return std::nullopt; }
	bool Press(const glm::vec3& /*rayOrigin*/, const glm::vec3& /*rayDirection*/, creature_fight::Button /*button*/,
	           uint32_t /*milliseconds*/, uint32_t /*turn*/) override
	{
		return false;
	}
	void Release(uint32_t /*milliseconds*/, uint32_t /*turn*/) override {}
	[[nodiscard]] bool IsPressed() const override { return false; }
	[[nodiscard]] std::optional<creature_fight::Tip> HandTip(std::optional<entt::entity> /*under*/) const override
	{
		return std::nullopt;
	}
	bool GestureSpecialMove() override { return false; }
	bool GestureSpell(MagicType /*type*/) override { return false; }
	void KnockOut(entt::entity /*creature*/) override {}
	void KillPermanently(entt::entity /*creature*/) override {}
	void Resurrect(entt::entity /*creature*/) override {}
	[[nodiscard]] bool IsKnockedOut(entt::entity /*creature*/) const override { return false; }
	[[nodiscard]] std::optional<creature_fight_hud::Values> GetPanel() const override { return std::nullopt; }
	void SetCameraWatches(bool /*enabled*/) override {}
	[[nodiscard]] bool GetCameraWatches() const override { return false; }
	[[nodiscard]] bool IsCameraOnFight() const override { return false; }

	std::vector<entt::entity> calledOff;
};

class CreatureScriptControlTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<Registry>();
		hands = &static_cast<WalkingHands&>(Locator::creatureObjectActionSystem::emplace<WalkingHands>());
		legs = &static_cast<StoppingLegs&>(Locator::creatureLocomotionSystem::emplace<StoppingLegs>());
		auto& registry = Locator::entitiesRegistry::value();
		creature = registry.Create();
		registry.Assign<CreatureMindState>(creature);
		registry.Assign<CreatureAnimation>(creature);
	}
	void TearDown() override
	{
		Locator::creatureLocomotionSystem::reset();
		Locator::creatureObjectActionSystem::reset();
		Locator::entitiesRegistry::reset();
	}

	CreatureMindSystem minds;
	WalkingHands* hands {nullptr};
	StoppingLegs* legs {nullptr};
	entt::entity creature {entt::null};
};
} // namespace

// The script makes the creature face a point. The walk of the hands to their thing stops first, so that it cannot start
// the walk again on the next turn and keep the body busy for ever.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureScriptControlTest, AScriptStopsTheWalkOfTheHandsToTheirThing)
{
	ASSERT_TRUE(minds.CarryOutForScript(creature, creature_mind::FaceForScript({10.0f, 0.0f, 10.0f})));
	EXPECT_EQ(hands->cancelled, (std::vector<entt::entity> {creature}));
	EXPECT_EQ(legs->stopped, (std::vector<entt::entity> {creature}));
	EXPECT_TRUE(Locator::entitiesRegistry::value().AllOf<ScriptControlled>(creature));
}

// Hands that do nothing have nothing to stop
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureScriptControlTest, AScriptLeavesIdleHandsAlone)
{
	hands->state = CreatureObjectActionSystemInterface::State::Idle;
	ASSERT_TRUE(minds.CarryOutForScript(creature, creature_mind::FaceForScript({10.0f, 0.0f, 10.0f})));
	EXPECT_TRUE(hands->cancelled.empty());
}

// A let-go that plays its clip continues to its moment when a script takes the creature
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureScriptControlTest, AScriptLeavesALetGoToPlayOn)
{
	hands->playsOn = true;
	ASSERT_TRUE(minds.CarryOutForScript(creature, creature_mind::FaceForScript({10.0f, 0.0f, 10.0f})));
	EXPECT_TRUE(hands->cancelled.empty());
	EXPECT_TRUE(Locator::entitiesRegistry::value().AllOf<ScriptControlled>(creature));
}

// The hand lets go of a stroked or slapped creature. The walk of the hands stops, but a let-go continues to its moment.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureScriptControlTest, FeedbackStopsTheHandsButNotALetGo)
{
	minds.ReceiveFeedback(creature, 0.0f);
	EXPECT_EQ(hands->cancelled, (std::vector<entt::entity> {creature}));
	hands->cancelled.clear();
	hands->state = CreatureObjectActionSystemInterface::State::Busy;
	hands->playsOn = true;
	minds.ReceiveFeedback(creature, 0.0f);
	EXPECT_TRUE(hands->cancelled.empty());
}

// A script that lets go of the creature, the order of a computer god and a gift that the hand calls off make the
// creature give up its action. A fight that it is in ends for both creatures, as a fight called off does.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureScriptControlTest, AGiveUpOfTheActionCallsOffTheFightOfTheCreature)
{
	auto& fights = static_cast<CalledOffFights&>(Locator::creatureFightSystem::emplace<CalledOffFights>());
	auto& registry = Locator::entitiesRegistry::value();
	auto& mind = registry.Get<CreatureMindState>(creature);
	mind.idle.agenda.resize(2);
	// With no fight, only the action stops
	minds.GiveUpAction(creature);
	EXPECT_TRUE(fights.calledOff.empty());
	EXPECT_EQ(mind.idle.step, mind.idle.agenda.size());
	registry.Assign<CreatureFighting>(creature);
	mind.idle.step = 0;
	minds.GiveUpAction(creature);
	EXPECT_EQ(fights.calledOff, (std::vector<entt::entity> {creature}));
	EXPECT_EQ(registry.Get<const CreatureMindState>(creature).idle.step, 2u);
	Locator::creatureFightSystem::reset();
}
