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

#include "Creature/CreatureEyes.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureRoute.h"
#include "Creature/CreatureScriptAgendas.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreaturePartner.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
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
	void CallOffForPlan(entt::entity creature) override { calledOffForPlan.push_back(creature); }
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
	std::vector<entt::entity> calledOffForPlan;
};

/// Magic that only records the creatures that let go of their miracle
class CastReleasingMagic final: public MagicSystemInterface
{
public:
	entt::entity CastAtPoint(MagicType, PlayerNames, glm::vec3, const magic::SpellCastData&,
	                         const particles::ProcessInfo&) override
	{
		return entt::null;
	}
	entt::entity CastOnObject(MagicType, PlayerNames, entt::entity, const magic::SpellCastData&,
	                          const particles::ProcessInfo&) override
	{
		return entt::null;
	}
	void CloseDown(entt::entity) override {}
	[[nodiscard]] bool CanCastAt(MagicType, PlayerNames, glm::vec3) override { return false; }
	[[nodiscard]] bool MeetsKindRule(MagicType, glm::vec3, bool) override { return false; }
	bool SpellEvent(entt::entity, const particles::SpellEventInfo&) override { return false; }
	void PayForSpell(entt::entity, float) override {}
	entt::entity CreateDispenser(glm::vec3, MagicType, float) override { return entt::null; }
	void SetDispenserPeriod(entt::entity, float) override {}
	entt::entity CreateOneOffSeed(glm::vec3, SpellSeedType, int, float) override { return entt::null; }
	entt::entity CreateOneOffSeedFor(glm::vec3, MagicType) override { return entt::null; }
	bool Remove(entt::entity) override { return false; }
	entt::entity GiveSeedToHand(PlayerNames, SpellSeedType, int, float) override { return entt::null; }
	entt::entity SummonSeed(PlayerNames, SpellSeedType, int) override { return entt::null; }
	entt::entity SummonSeedAtSite(entt::entity, SpellSeedType, int) override { return entt::null; }
	void DiscardHeldSeed() override {}
	void DiscardHeldSeedOf(PlayerNames) override {}
	void DeleteSeed(entt::entity) override {}
	void UpdateHand(const HandFrame&, const input_steps::Steps&) override {}
	bool TapFireBall(entt::entity) override { return false; }
	bool TapGlobe(entt::entity, PlayerNames) override { return false; }
	bool PressFireBall(uint32_t, uint32_t) override { return false; }
	void HoldFireBall(uint32_t, uint32_t) override {}
	void ReleaseFireBall(uint32_t, uint32_t) override {}
	[[nodiscard]] std::optional<entt::entity> OrbUnderHand() const override { return std::nullopt; }
	bool PressAction() override { return false; }
	void ReleaseAction() override {}
	[[nodiscard]] bool IsHandBusy() const override { return false; }
	[[nodiscard]] bool IsHandBusyOf(PlayerNames) const override { return false; }
	[[nodiscard]] magic::PourPose GetHandPour(float) const override { return {}; }
	[[nodiscard]] HandCastState GetHandCastState() const override { return {}; }
	[[nodiscard]] std::optional<entt::entity> GetHeldSeed() const override { return std::nullopt; }
	[[nodiscard]] std::optional<entt::entity> GetHeldSeedOf(PlayerNames) const override { return std::nullopt; }
	void SetIconSeedPowerUp(entt::entity, int) override {}
	entt::entity TakeIconSeed(entt::entity, PlayerNames, SpellSeedType, int, float) override { return entt::null; }
	[[nodiscard]] HandResult GetLastHandResult() const override { return {}; }
	bool SendSpellEvent(entt::entity, const particles::SpellEventInfo&) override { return false; }
	float ForcePayForSpell(entt::entity, float) override { return 0.0f; }
	[[nodiscard]] float SpellStrength(entt::entity) override { return 0.0f; }
	void ProcessTurn() override {}
	void Update(float) override {}
	void Reset() override {}
	void SetIgnoreInfluence(bool) override {}
	void DriveHand(std::optional<HandFrame>) override {}
	[[nodiscard]] std::optional<HandFrame> GetDrivenHand() const override { return std::nullopt; }
	[[nodiscard]] bool IsIgnoringInfluence() const override { return false; }
	[[nodiscard]] std::vector<SpellInfo> GetSpells() const override { return {}; }
	void RainOnFire(const glm::vec3&) override {}
	[[nodiscard]] std::vector<DispenserInfo> GetDispensers() const override { return {}; }
	void ReleaseCreatureCast(entt::entity creature) override { released.push_back(creature); }

	std::vector<entt::entity> released;
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

// A script that lets go of the creature and a gift that the hand calls off make the creature give up its action. A fight that
// it is in ends for both creatures, as a fight called off does.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureScriptControlTest, AGiveUpOfTheActionCallsOffTheFightOfTheCreature)
{
	auto& fights = static_cast<CalledOffFights&>(Locator::creatureFightSystem::emplace<CalledOffFights>());
	auto& registry = Locator::entitiesRegistry::value();
	auto& mind = registry.Get<CreatureMindState>(creature);
	mind.idle.agenda.resize(2);
	// With no fight, only the action stops
	minds.GiveUpAction(creature);
	EXPECT_TRUE(fights.calledOffForPlan.empty());
	EXPECT_EQ(mind.idle.step, mind.idle.agenda.size());
	registry.Assign<CreatureFighting>(creature);
	mind.idle.step = 0;
	minds.GiveUpAction(creature);
	// The give-up also ends the fight of the body again after a fight that is over
	EXPECT_EQ(fights.calledOffForPlan, (std::vector<entt::entity> {creature}));
	EXPECT_TRUE(fights.calledOff.empty());
	EXPECT_EQ(registry.Get<const CreatureMindState>(creature).idle.step, 2u);
	Locator::creatureFightSystem::reset();
}

// The order of a computer god that makes a desire dominant stops all that the creature does as a failure. The fight
// ends for both creatures. The walk, the hands, the miracle, the eyes and the agenda stop. The partner of a joint action
// is free.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureScriptControlTest, AStopAndGiveUpStopsTheWalkTheHandsAndTheJointAction)
{
	auto& fights = static_cast<CalledOffFights&>(Locator::creatureFightSystem::emplace<CalledOffFights>());
	auto& magic = static_cast<CastReleasingMagic&>(Locator::magicSystem::emplace<CastReleasingMagic>());
	auto& registry = Locator::entitiesRegistry::value();
	registry.Assign<CreatureFighting>(creature);
	auto& eyes = registry.Assign<CreatureEyes>(creature);
	eyes.mode = creature_eyes::Mode::Wide;
	const auto partner = registry.Create();
	registry.Assign<CreatureMindState>(partner);
	registry.Assign<CreaturePartner>(creature, CreaturePartner {.partner = partner});
	registry.Assign<CreaturePartner>(partner, CreaturePartner {.partner = creature});
	auto& mind = registry.Get<CreatureMindState>(creature);
	mind.idle.agenda.resize(2);
	mind.planActive = true;
	mind.planForced = true;

	minds.StopAndGiveUpAction(creature);

	EXPECT_EQ(fights.calledOffForPlan, (std::vector<entt::entity> {creature}));
	EXPECT_TRUE(fights.calledOff.empty());
	EXPECT_EQ(legs->stopped, (std::vector<entt::entity> {creature}));
	EXPECT_EQ(hands->cancelled, (std::vector<entt::entity> {creature}));
	// The partner also fails, so it lets go of its miracle after the creature
	EXPECT_EQ(magic.released, (std::vector<entt::entity> {creature, partner}));
	EXPECT_EQ(eyes.mode, creature_eyes::Mode::Calm);
	const auto& stopped = registry.Get<const CreatureMindState>(creature);
	EXPECT_TRUE(stopped.idle.agenda.empty());
	EXPECT_FALSE(stopped.planActive);
	EXPECT_FALSE(stopped.planForced);
	EXPECT_FALSE(registry.AllOf<CreaturePartner>(creature));
	EXPECT_FALSE(registry.AllOf<CreaturePartner>(partner));
	Locator::magicSystem::reset();
	Locator::creatureFightSystem::reset();
}

// The plain give-up leaves the walk and the hands as they are
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureScriptControlTest, AGiveUpLeavesTheWalkAndTheHands)
{
	minds.GiveUpAction(creature);
	EXPECT_TRUE(legs->stopped.empty());
	EXPECT_TRUE(hands->cancelled.empty());
}

// The feedback of the player stops the fight of the creature, and its body ends the fight again after a fight that is
// over
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureScriptControlTest, FeedbackEndsTheFightOfTheBodyAgain)
{
	auto& fights = static_cast<CalledOffFights&>(Locator::creatureFightSystem::emplace<CalledOffFights>());
	auto& registry = Locator::entitiesRegistry::value();
	registry.Assign<CreatureFighting>(creature);
	minds.ReceiveFeedback(creature, 0.0f);
	EXPECT_EQ(fights.calledOffForPlan, (std::vector<entt::entity> {creature}));
	EXPECT_TRUE(fights.calledOff.empty());
	Locator::creatureFightSystem::reset();
}

// The feedback of the player stops what the creature does. The creature lets go of the miracle that it holds, and its
// eyes go calm, also when the feedback is too slight to count.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureScriptControlTest, FeedbackLetsGoOfTheMiracleAndCalmsTheEyes)
{
	auto& magic = static_cast<CastReleasingMagic&>(Locator::magicSystem::emplace<CastReleasingMagic>());
	auto& registry = Locator::entitiesRegistry::value();
	auto& eyes = registry.Assign<CreatureEyes>(creature);
	eyes.mode = creature_eyes::Mode::Wide;
	eyes.blink.intervalMs = 1;
	minds.ReceiveFeedback(creature, 0.0f);
	EXPECT_EQ(magic.released, (std::vector<entt::entity> {creature}));
	EXPECT_EQ(eyes.mode, creature_eyes::Mode::Calm);
	EXPECT_EQ(eyes.blink.intervalMs, creature_eyes::k_BlinkIntervalMs);
	eyes.mode = creature_eyes::Mode::Wide;
	minds.ReceiveFeedback(creature, -0.5f);
	EXPECT_EQ(magic.released, (std::vector<entt::entity> {creature, creature}));
	EXPECT_EQ(eyes.mode, creature_eyes::Mode::Calm);
	Locator::magicSystem::reset();
}

// Slight feedback gives the creature an order to look at the camera of its player. The creature is free to change the
// plan for a plan more than twice as pressing.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureSlightFeedback, IsAnOrderToLookAtTheCameraThatTheCreatureCanChange)
{
	constexpr auto k_Look = creature_feedback::k_SlightFeedbackLook;
	EXPECT_EQ(k_Look.action, "LookAtCamera");
	EXPECT_TRUE(k_Look.ordered);
	EXPECT_FALSE(k_Look.forced);
	EXPECT_FLOAT_EQ(k_Look.priority, 1.0f);
}
