/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <MindFile.h>
#include <gtest/gtest.h>

#include "Camera/Camera.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureRoute.h"
#include "Creature/CreatureTurnToFace.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureArena.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreaturePartner.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/GuidanceSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Locator.h"

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/CreatureFightSystem.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace fight = openblack::creature_fight;

namespace
{
/// Keeps the reactions made, and does nothing else
class RecordedReactions final: public ecs::systems::ReactionSystemInterface
{
public:
	uint32_t Create(const Source& source) override
	{
		made.push_back(source);
		return static_cast<uint32_t>(made.size());
	}
	void Move(uint32_t, const glm::vec3&, const glm::vec3&, float) override {}
	[[nodiscard]] std::optional<Active> Find(uint32_t) const override { return std::nullopt; }
	void RemoveFrom(entt::entity) override {}
	void RemoveFrom(entt::entity, Reaction) override {}
	void Remove(uint32_t) override {}
	void SetInitiator(uint32_t, entt::entity) override {}
	[[nodiscard]] bool IsActive(uint32_t) const override { return false; }
	void StopReacting(entt::entity) override {}
	[[nodiscard]] bool HasReaction(entt::entity) const override { return false; }
	void ProcessTurn() override {}
	void Reset() override {}
	void SetLandBalance(Balance /*which*/, float /*balance*/) override {}
	[[nodiscard]] float GetLandBalance(Balance /*which*/) const override { return 1.0f; }
	[[nodiscard]] std::span<const Active> GetReactions() const override { return {}; }
	[[nodiscard]] std::vector<Active> ReactionsAt(const glm::vec3&) const override { return {}; }

	std::vector<Source> made;
};

/// A mind that keeps the plans that it is made to carry out, and does nothing else
class ForcedMinds final: public ecs::systems::CreatureMindSystemInterface
{
public:
	void ProcessTurn() override {}
	void PlanTurn() override {}
	void LearnTurn() override {}
	void LoadMind(entt::entity, std::shared_ptr<const creaturemind::MindFileData>) override {}
	[[nodiscard]] std::optional<creaturemind::MindFileData> SaveMind(entt::entity) const override { return std::nullopt; }
	void ClearLearning(entt::entity) override {}
	void SeeSkill(const glm::vec3&, size_t) override {}
	void SeeMiracle(const glm::vec3&, size_t) override {}
	void PlayerDid(size_t, const glm::vec3&, std::optional<entt::entity>, std::optional<PlayerNames>) override {}
	[[nodiscard]] const creature_mind_tables::Tables* GetTables() override { return nullptr; }
	bool PlayAction(entt::entity, size_t) override { return false; }
	bool PlayGesture(entt::entity, size_t) override { return false; }
	void PullFace(entt::entity, size_t) override {}
	std::optional<creature_face::Request> ShowFeeling(entt::entity, creature_face::Cue) override { return std::nullopt; }
	bool SitDown(entt::entity) override { return false; }
	void StandUp(entt::entity) override {}
	void ReceiveFeedback(entt::entity, float) override {}
	bool ForceAction(entt::entity, size_t, bool, std::optional<creature_face::Request>, float) override { return false; }
	bool Sleep(entt::entity) override { return false; }
	bool Eat(entt::entity, std::optional<entt::entity>) override { return false; }
	bool Drink(entt::entity) override { return false; }
	bool Poo(entt::entity creature) override
	{
		pooedDirectly = creature;
		return true;
	}
	bool Puke(entt::entity) override { return false; }
	bool Faint(entt::entity) override { return false; }
	void Wake(entt::entity) override {}
	void FoughtFight(entt::entity, bool) override {}
	void ChangeDesires(entt::entity, const std::function<void(creature_desires::Desires&)>&) override {}
	void SetAgendaPriority(entt::entity, float) override {}
	bool ForceActivity(entt::entity creature, const ForcedActivity& plan) override
	{
		forced.emplace_back(creature, plan.desire, std::string(plan.action), plan.actionObject, plan.activityObject);
		return true;
	}

	struct Forced
	{
		Forced(entt::entity who, creature_desires::Desire wanted, std::string done, entt::entity on,
		       std::optional<entt::entity> about)
		    : creature(who)
		    , desire(wanted)
		    , action(std::move(done))
		    , object(on)
		    , activityObject(about)
		{
		}
		entt::entity creature;
		creature_desires::Desire desire;
		std::string action;
		entt::entity object;
		std::optional<entt::entity> activityObject;
	};
	[[nodiscard]] bool IsLyingDead(entt::entity) override
	{
		++deathChecks;
		return false;
	}
	creature_mind::HandsCheck CheckHandsBeforeAction(entt::entity creature) override
	{
		if (auto found = handsOf.find(creature); found != handsOf.end())
		{
			auto& [checks, done] = found->second;
			++done;
			return done <= checks.size() ? checks.at(done - 1) : creature_mind::HandsCheck::Free;
		}
		++handChecks;
		if (handChecks <= hands.size())
		{
			return hands.at(handChecks - 1);
		}
		return creature_mind::HandsCheck::Free;
	}
	std::vector<Forced> forced;
	std::optional<entt::entity> pooedDirectly;
	/// What the checks of the hands before a faint do in turn, then Free
	std::vector<creature_mind::HandsCheck> hands;
	size_t handChecks {0};
	/// What the checks of the hands of one creature do in turn, then Free, and the count of its checks
	std::map<entt::entity, std::pair<std::vector<creature_mind::HandsCheck>, size_t>> handsOf;
	/// How many times a creature asked to fight was checked for death, one time for each question
	size_t deathChecks {0};
};

/// Legs that do nothing but count the creatures that they stop at once
class StoppingLegs final: public ecs::systems::CreatureLocomotionSystemInterface
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
	void StopAtOnce(entt::entity creature) override { stopped.push_back(creature); }
	void ResetPace(entt::entity) override {}
	void Place(entt::entity, glm::vec3) override {}
	void PlaceWithoutWalking(entt::entity, glm::vec3) override {}
	[[nodiscard]] bool IsMoving(entt::entity) const override { return false; }
	[[nodiscard]] float KeptFraction(entt::entity) const override { return 0.0f; }
	[[nodiscard]] const creature_route::WalkableLand& GetWalkableLand() override
	{
		throw std::logic_error("the fights do not ask for the walkable land");
	}
	[[nodiscard]] std::vector<creature_route::Circle> ThingCirclesIn(entt::entity, glm::ivec2) override { return {}; }

	std::vector<entt::entity> stopped;
};

/// A hand that holds the creature it is told to hold, and does nothing else
class HoldingHand final: public ecs::systems::CreatureHandSystemInterface
{
public:
	bool Grab() override { return false; }
	[[nodiscard]] bool MayHold(entt::entity) const override { return true; }
	[[nodiscard]] creature_hand::InteractTip InteractTipOver(entt::entity, bool) const override
	{
		return creature_hand::InteractTip::None;
	}
	[[nodiscard]] bool IsClick() const override { return false; }
	std::optional<HandPose> Update(const glm::vec3&, const glm::vec3&, glm::vec2, float, const input_steps::Steps&) override
	{
		return std::nullopt;
	}
	void Release() override {}
	bool Stroke(entt::entity, creature_feedback::BodyPart) override { return false; }
	bool Slap(entt::entity, float, bool, bool) override { return false; }
	[[nodiscard]] std::optional<entt::entity> GetCreature() const override { return held; }
	[[nodiscard]] bool IsLocked(entt::entity creature) const override { return held == creature; }
	[[nodiscard]] bool IsHeldByCommand() const override { return false; }
	[[nodiscard]] std::optional<entt::entity> CreatureUnderCursor() const override { return std::nullopt; }
	[[nodiscard]] float GetFeedbackSum() const override { return 0.0f; }
	[[nodiscard]] float GetLastFeedbackSum() const override { return 0.0f; }
	bool TakeFocusTip() override { return false; }

	std::optional<entt::entity> held;
};

/// The game's random numbers: they give the numbers in the list in turn, then 0, and count the draws of each range
class ListedGameRandom final: public GameRandomInterface
{
public:
	uint32_t SyncedRand(uint32_t range, DrawSite /*site*/) override
	{
		ranges.push_back(range);
		if (next < numbers.size())
		{
			return numbers.at(next++);
		}
		return 0;
	}
	float SyncedFloatRand(float, DrawSite) override { return 0.0f; }
	uint32_t LocalRand(int32_t) override { return 0; }
	float LocalFloatRand(float) override { return 0.0f; }
	int32_t CrtRand() override { return 0; }
	void CrtSrand(uint32_t) override {}
	[[nodiscard]] GameRandomSeeds GetSeeds() const override { return {}; }
	void SetSeeds(GameRandomSeeds) override {}
	[[nodiscard]] ParticleRandomStream GetParticleStream() const override { return ParticleRandomStream::None; }
	void SetParticleStream(ParticleRandomStream) override {}

	std::vector<uint32_t> numbers;
	size_t next {0};
	std::vector<uint32_t> ranges;
};

/// Two creatures in a duel in an arena on flat land, neither with a body to click on. The camera shows their fight.
/// The hands of a network game: the players that have hands (the human gods), and the player of this computer
class FakeHands final: public ecs::systems::HandSystemInterface
{
public:
	bool Initialize() noexcept override { return true; }
	[[nodiscard]] std::array<entt::entity, 2> GetPlayerHands() const noexcept override
	{
		return GetHandsOf(PlayerNames::PLAYER_ONE).value_or(std::array {entt::entity {entt::null}, entt::entity {entt::null}});
	}
	[[nodiscard]] std::array<std::optional<glm::vec3>, 2> GetPlayerHandPositions() const noexcept override { return {}; }
	void UpdateAlignmentMorph(std::optional<map_coords::MapCoords> /*picked*/, bool /*holdPoint*/) override {}
	[[nodiscard]] std::optional<std::array<entt::entity, 2>> GetHandsOf(PlayerNames player) const noexcept override
	{
		const auto found = hands.find(player);
		return found != hands.end() ? std::optional(found->second) : std::nullopt;
	}
	[[nodiscard]] PlayerNames GetOwnPlayer() const noexcept override { return own; }

	std::map<PlayerNames, std::array<entt::entity, 2>> hands;
	PlayerNames own {PlayerNames::PLAYER_ONE};
};

/// A clock that only tells if the game is a network game
class FakeTime final: public ecs::systems::TimeSystemInterface
{
public:
	void Start() override {}
	void Update() override {}
	void StartDraw() override {}
	[[nodiscard]] std::chrono::milliseconds GetElapsedTime() const override { return {}; }
	void StartGameClock(bool /*paused*/) override {}
	void RestartTurns() override {}
	void ResetTimerToTurn() override {}
	void ResumeAtTurn(uint32_t /*turn*/, int32_t /*remainderMs*/) override {}
	[[nodiscard]] int32_t GetTurnRemainderMs() const override { return 0; }
	[[nodiscard]] bool IsTurnDue() override { return false; }
	void StartTurn() override {}
	void UpdateFrame() override {}
	void StepInput(std::chrono::microseconds /*stepTime*/) override {}
	[[nodiscard]] const input_steps::Steps& GetInputSteps() const override { return inputSteps; }
	void PinDate(std::optional<int64_t> /*pinnedDate*/) override {}
	[[nodiscard]] uint32_t GetTurn() const override { return 0; }
	[[nodiscard]] float GetTurnFraction() const override { return 0.0f; }
	[[nodiscard]] std::chrono::milliseconds GetFrameGameTime() const override { return {}; }
	[[nodiscard]] std::chrono::milliseconds GetFrameRealTime() const override { return {}; }
	void SetPaused(bool /*paused*/) override {}
	[[nodiscard]] bool IsPaused() const override { return false; }
	void SetPlayerPause(bool /*paused*/) override {}
	void SetHeld(bool /*held*/) override {}
	[[nodiscard]] bool IsHeld() const override { return false; }
	void SetNetworkGame(bool value) override { networkGame = value; }
	[[nodiscard]] bool IsNetworkGame() const override { return networkGame; }
	void SetSpeed(float /*speed*/) override {}
	[[nodiscard]] float GetSpeed() const override { return 1.0f; }
	[[nodiscard]] uint32_t GetTicks() const override { return 0; }
	[[nodiscard]] int64_t GetUnixTime() const override { return 0; }
	void RestartClock(std::optional<int64_t> /*unixTime*/) override {}
	[[nodiscard]] std::optional<int64_t> GetPinnedDate() const override { return std::nullopt; }

	bool networkGame {false};
	input_steps::Steps inputSteps {};
};

class CreatureFightSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<Registry>();
		auto& registry = Locator::entitiesRegistry::value();
		first = MakeCreature(registry, PlayerNames::PLAYER_ONE, {-15.0f, 0.0f, 0.0f});
		second = MakeCreature(registry, PlayerNames::PLAYER_TWO, {15.0f, 0.0f, 0.0f});
		// The arena has appeared
		const auto arena = registry.Create();
		for (const auto& [self, other] : {std::pair(first, second), std::pair(second, first)})
		{
			registry.Assign<CreatureFighting>(self, CreatureFighting {
			                                            .stage = CreatureFighting::Stage::Duel,
			                                            .opponent = other,
			                                            .arena = {.centre = {0.0f, 0.0f}, .radius = 40.0f},
			                                            .madeArena = self == first,
			                                            .arenaEntity = arena,
			                                        });
		}
		ShowTheFight({0.0f, 0.0f, 0.0f});
	}
	void TearDown() override
	{
		fights.Reset();
		Locator::handSystem::reset();
		Locator::time::reset();
		Locator::camera::reset();
		Locator::entitiesRegistry::reset();
	}

	/// The world camera looks at the arena. A double click sends it to a place, and the update after it starts.
	void ShowTheFight(const glm::vec3& place)
	{
		if (!Locator::camera::has_value())
		{
			auto& camera = Locator::camera::emplace<Camera>();
			camera.SetOrigin({0.0f, 30.0f, 40.0f});
			camera.SetFocus({0.0f, 0.0f, 0.0f});
		}
		fights.FlewTo(place);
		fights.Update(std::chrono::duration<float, std::milli>(0.0f));
	}

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

TEST_F(CreatureFightSystemTest, TheTurnGivesTheSentMovesToTheSendingPlayersCreature)
{
	// A press on the opponent: the blow goes to the sender's creature and waits for its charge
	fights.DoMove(PlayerNames::PLAYER_ONE, second, fight::AttackMove(fight::Band::High), false);
	ASSERT_EQ(QueueOf(first).Size(), 1u);
	EXPECT_FALSE(QueueOf(first).Moves()[0].chargeMs.has_value());
	// A press on a thing that is gone by the turn gives nothing
	fights.DoMove(PlayerNames::PLAYER_ONE, entt::null, fight::BlockMove(), false);
	EXPECT_EQ(QueueOf(first).Size(), 1u);
	// The other player's press goes to the other player's creature
	fights.DoMove(PlayerNames::PLAYER_TWO, first, fight::BlockMove(), false);
	EXPECT_EQ(QueueOf(second).Size(), 1u);
	EXPECT_EQ(QueueOf(first).Size(), 1u);
	// The let go charges the blow that waits
	fights.DoCharge(PlayerNames::PLAYER_ONE, 400.0f);
	ASSERT_TRUE(QueueOf(first).Moves()[0].chargeMs.has_value());
	EXPECT_EQ(*QueueOf(first).Moves()[0].chargeMs, 400.0f);
}

TEST_F(CreatureFightSystemTest, TheTurnStepsFromWhereTheCreatureStandsThen)
{
	// The press was made with the creature at its place; by the turn it stands past the point, so it steps back
	auto& registry = Locator::entitiesRegistry::value();
	registry.Get<Transform>(first).position = {-5.0f, 0.0f, 0.0f};
	fights.DoGroundPress(PlayerNames::PLAYER_ONE, first, {-10.0f, 0.0f, 0.0f}, true);
	registry.Get<Transform>(first).position = {-15.0f, 0.0f, 0.0f};
	fights.DoGroundPress(PlayerNames::PLAYER_ONE, first, {-10.0f, 0.0f, 0.0f}, false);
	ASSERT_EQ(QueueOf(first).Size(), 2u);
	EXPECT_NE(QueueOf(first).Moves()[0].move, QueueOf(first).Moves()[1].move);
	// Only the sender's own creature steps
	fights.DoGroundPress(PlayerNames::PLAYER_TWO, first, {-10.0f, 0.0f, 0.0f}, false);
	EXPECT_EQ(QueueOf(first).Size(), 2u);
}

TEST_F(CreatureFightSystemTest, ADoubleClickIntoTheArenaShowsTheFightAtOnce)
{
	EXPECT_TRUE(fights.IsCameraOnFight());
	EXPECT_TRUE(fights.DirectsFight());
	fights.Reset();
	EXPECT_FALSE(fights.IsCameraOnFight());
	// A place out of the arena does not show the fight
	ShowTheFight({45.0f, 0.0f, 0.0f});
	EXPECT_FALSE(fights.IsCameraOnFight());
	// A double click that a new land forgot does not show the fight
	fights.FlewTo({0.0f, 0.0f, 0.0f});
	fights.Reset();
	fights.Update(std::chrono::duration<float, std::milli>(0.0f));
	EXPECT_FALSE(fights.IsCameraOnFight());
}

TEST_F(CreatureFightSystemTest, ADoubleClickMeasuresFromWhereTheCameraWasBeforeItsFlight)
{
	fights.Reset();
	auto& camera = Locator::camera::value();
	// The camera is far from the arena when the player double clicks into it
	camera.SetOrigin({0.0f, 30.0f, 2000.0f});
	camera.SetFocus({0.0f, 0.0f, 1900.0f});
	fights.FlewTo({0.0f, 0.0f, 0.0f});
	// The flight brings the camera near before the next update, but the fight is not shown
	camera.SetOrigin({0.0f, 30.0f, 40.0f});
	camera.SetFocus({0.0f, 0.0f, 0.0f});
	fights.Update(std::chrono::duration<float, std::milli>(0.0f));
	EXPECT_FALSE(fights.IsCameraOnFight());
	// A fight of the player's creature under a script's control is shown from any distance
	Locator::entitiesRegistry::value().Assign<ScriptControlled>(second);
	camera.SetOrigin({0.0f, 30.0f, 2000.0f});
	camera.SetFocus({0.0f, 0.0f, 1900.0f});
	ShowTheFight({0.0f, 0.0f, 0.0f});
	EXPECT_TRUE(fights.IsCameraOnFight());
}

TEST_F(CreatureFightSystemTest, WithoutTheFightViewTheHandDoesNotDirectTheFight)
{
	fights.Reset();
	EXPECT_FALSE(fights.DirectsFight());
	EXPECT_FALSE(PressGround({-15.0f, 10.0f}, fight::Button::Action));
	EXPECT_TRUE(QueueOf(first).Empty());
	EXPECT_EQ(fights.HandTip(second), std::nullopt);
	// The duel still has the player's fighter, for the gestures
	EXPECT_EQ(fights.PlayersFighter(), first);
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

TEST_F(CreatureFightSystemTest, AScriptHandsAFighterToTheComputerOrToNobody)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Get<CreatureFighting>(first).fighter.control = fight::Control::Player;
	EXPECT_TRUE(fights.IsAutoFighting(first));
	fights.SetAutoFighting(first, false);
	EXPECT_EQ(registry.Get<const CreatureFighting>(first).fighter.control, fight::Control::None);
	EXPECT_FALSE(fights.IsAutoFighting(first));
	// Moves queued for it don't hand it back to anyone
	EXPECT_TRUE(fights.QueueMove(first, fight::AttackMove(fight::Band::Low), false));
	EXPECT_FALSE(fights.IsAutoFighting(first));
	EXPECT_EQ(fights.QueuedBlows(first), 1u);
	fights.SetAutoFighting(first, true);
	EXPECT_EQ(registry.Get<const CreatureFighting>(first).fighter.control, fight::Control::Computer);
	EXPECT_TRUE(fights.IsAutoFighting(first));
	// A creature that never fought has nobody choosing its moves yet
	const auto newcomer = MakeCreature(registry, PlayerNames::PLAYER_ONE, {100.0f, 0.0f, 0.0f});
	EXPECT_FALSE(fights.IsAutoFighting(newcomer));
	fights.SetAutoFighting(newcomer, true);
	EXPECT_TRUE(fights.IsAutoFighting(newcomer));
}

TEST_F(CreatureFightSystemTest, ScriptsAskWhatAFighterIsDoing)
{
	auto& registry = Locator::entitiesRegistry::value();
	fight::Enter(registry.Get<CreatureFighting>(first).fighter, fight::State::Stance);
	EXPECT_EQ(fights.CurrentFightAction(first), fight::FightAction::Stance);
	const auto newcomer = MakeCreature(registry, PlayerNames::PLAYER_ONE, {100.0f, 0.0f, 0.0f});
	EXPECT_EQ(fights.CurrentFightAction(newcomer), fight::FightAction::Other);
	registry.Assign<CreatureKnockedOut>(newcomer);
	EXPECT_EQ(fights.CurrentFightAction(newcomer), fight::FightAction::Fainted);
}

TEST_F(CreatureFightSystemTest, AKnockOutThatNoBlowMadeGivesNoSightOfAFightWon)
{
	Locator::reactionSystem::emplace<RecordedReactions>();
	auto& reactions = static_cast<RecordedReactions&>(Locator::reactionSystem::value());
	// A miracle or a loss of life knocks the loser out, not a blow. The faint alone makes a reaction, as to a death, on
	// the turn that it starts on the body.
	fights.KnockOut(second);
	EXPECT_TRUE(reactions.made.empty());
	fights.ProcessTurn();
	ASSERT_EQ(reactions.made.size(), 1u);
	EXPECT_EQ(reactions.made.front().type, Reaction::ReactToDeath);
	EXPECT_EQ(reactions.made.front().initiator, second);
	EXPECT_TRUE(reactions.made.front().playerless);
	Locator::reactionSystem::reset();
}

TEST_F(CreatureFightSystemTest, TheWinnerIsMadeToShowOffAtTheLoser)
{
	auto& minds = static_cast<ForcedMinds&>(Locator::creatureMindSystem::emplace<ForcedMinds>());
	Locator::entitiesRegistry::value().Get<Creature>(first).alignment = -0.5f;
	fights.KnockOut(second);
	ASSERT_EQ(minds.forced.size(), 1u);
	EXPECT_EQ(minds.forced[0].creature, first);
	EXPECT_EQ(minds.forced[0].desire, creature_desires::Desire::ObeyPlayer);
	EXPECT_EQ(minds.forced[0].action, "ShowImpressiveAnimation");
	EXPECT_EQ(minds.forced[0].object, second);
	EXPECT_EQ(minds.forced[0].activityObject, second);
	EXPECT_FALSE(minds.pooedDirectly.has_value());
	Locator::creatureMindSystem::reset();
}

TEST_F(CreatureFightSystemTest, AnEvilWinnerIsMadeToPooOnTheLoser)
{
	auto& minds = static_cast<ForcedMinds&>(Locator::creatureMindSystem::emplace<ForcedMinds>());
	Locator::entitiesRegistry::value().Get<Creature>(first).alignment = -0.51f;
	fights.KnockOut(second);
	ASSERT_EQ(minds.forced.size(), 1u);
	EXPECT_EQ(minds.forced[0].action, "Poo");
	EXPECT_EQ(minds.forced[0].object, second);
	// The mind carries out the plan's own agenda: the fight never makes the creature poo by itself
	auto& registry = Locator::entitiesRegistry::value();
	for (int turn = 0; turn < 600 && registry.AllOf<CreatureFighting>(first); ++turn)
	{
		fights.Update(std::chrono::duration<float, std::milli>(100.0f));
		fights.ProcessTurn();
	}
	EXPECT_FALSE(registry.AllOf<CreatureFighting>(first));
	EXPECT_FALSE(minds.pooedDirectly.has_value());
	Locator::creatureMindSystem::reset();
}

TEST_F(CreatureFightSystemTest, ScriptsSeeAFightOnlyWhileItsDuelLasts)
{
	auto& registry = Locator::entitiesRegistry::value();
	EXPECT_TRUE(fights.IsDuelling(first));
	registry.Get<CreatureFighting>(first).stage = CreatureFighting::Stage::Taunt;
	EXPECT_FALSE(fights.IsDuelling(first));
	EXPECT_TRUE(fights.IsFighting(first));
	registry.Get<CreatureFighting>(first).stage = CreatureFighting::Stage::Duel;
	fights.KnockOut(second);
	// The winner finishes its move in the fight, but its duel is over
	EXPECT_FALSE(fights.IsDuelling(first));
	EXPECT_FALSE(fights.IsDuelling(second));
}

TEST_F(CreatureFightSystemTest, AScriptTakesACreatureFromItsDuelForBoth)
{
	fights.TakeForScript(first);
	EXPECT_FALSE(fights.IsDuelling(first));
	EXPECT_FALSE(fights.IsDuelling(second));
	// Neither won: neither is knocked out
	EXPECT_FALSE(fights.IsKnockedOut(first));
	EXPECT_FALSE(fights.IsKnockedOut(second));
}

TEST_F(CreatureFightSystemTest, AScriptTakesAKnockedOutCreatureBeforeItIsTakenHome)
{
	auto& registry = Locator::entitiesRegistry::value();
	fights.KnockOut(second);
	ASSERT_TRUE(fights.IsKnockedOut(second));
	const auto where = registry.Get<const Transform>(second).position;
	fights.TakeForScript(second);
	EXPECT_FALSE(fights.IsKnockedOut(second));
	// It is not taken home later
	for (int turn = 0; turn < 600; ++turn)
	{
		fights.ProcessTurn();
	}
	EXPECT_EQ(registry.Get<const Transform>(second).position, where);
}

TEST_F(CreatureFightSystemTest, AScriptDoesNotRaiseACreatureKilledForGood)
{
	fights.Withdraw(second);
	fights.KillPermanently(second);
	ASSERT_TRUE(fights.IsKnockedOut(second));
	fights.TakeForScript(second);
	EXPECT_TRUE(fights.IsKnockedOut(second));
}

TEST_F(CreatureFightSystemTest, AScriptReadsAndSetsTheFightHealthInAndOutOfAFight)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Get<CreatureFighting>(second).fighter.health = 0.6f;
	EXPECT_FLOAT_EQ(fights.GetFightHealth(second), 0.6f);
	// Set in a fight, the fighter takes it at once
	fights.SetFightHealth(second, 1.0f);
	EXPECT_FLOAT_EQ(registry.Get<const CreatureFighting>(second).fighter.health, 1.0f);
	registry.Get<CreatureFighting>(second).fighter.health = 0.25f;
	// Out of the fight it stays as the fight left it
	fights.Withdraw(second);
	ASSERT_FALSE(registry.AllOf<CreatureFighting>(second));
	EXPECT_FLOAT_EQ(fights.GetFightHealth(second), 0.25f);
	// A creature that never fought has full fight health, and keeps whatever a script gives it
	const auto other = MakeCreature(registry, PlayerNames::PLAYER_THREE, {200.0f, 0.0f, 0.0f});
	EXPECT_FLOAT_EQ(fights.GetFightHealth(other), 1.0f);
	fights.SetFightHealth(other, 1.5f);
	EXPECT_FLOAT_EQ(fights.GetFightHealth(other), 1.5f);
}

TEST_F(CreatureFightSystemTest, AFightCalledOffEndsWithNoWinnerAndNoResponse)
{
	Locator::reactionSystem::emplace<RecordedReactions>();
	auto& reactions = static_cast<RecordedReactions&>(Locator::reactionSystem::value());
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto self : {first, second})
	{
		fight::Enter(registry.Get<CreatureFighting>(self).fighter, fight::State::Stance);
	}
	EXPECT_TRUE(fights.QueueMove(first, fight::AttackMove(fight::Band::High), false));
	fights.CallOff(first);
	for (const auto self : {first, second})
	{
		const auto& side = registry.Get<const CreatureFighting>(self);
		// Both wind down to stand, with nothing left to do and nothing to celebrate or sulk about
		EXPECT_TRUE(side.ended);
		EXPECT_TRUE(side.fighter.stopping);
		EXPECT_TRUE(side.played);
		EXPECT_EQ(side.stage, CreatureFighting::Stage::Respond);
		EXPECT_TRUE(side.fighter.queue.Empty());
	}
	// Nobody won it
	EXPECT_TRUE(reactions.made.empty());
	EXPECT_EQ(registry.Get<const CreatureFightRecord>(first).wins, 0u);
	EXPECT_EQ(registry.Get<const CreatureFightRecord>(second).wins, 0u);
	Locator::reactionSystem::reset();
}

TEST_F(CreatureFightSystemTest, OnTheWayToAFightACreatureIsSettingItUp)
{
	auto& registry = Locator::entitiesRegistry::value();
	EXPECT_FALSE(fights.IsSettingUpFight(first));
	registry.Get<CreatureFighting>(first).stage = CreatureFighting::Stage::Approach;
	EXPECT_TRUE(fights.IsSettingUpFight(first));
	// Called off then, it simply leaves the fight
	fights.CallOff(first);
	EXPECT_FALSE(registry.AllOf<CreatureFighting>(first));
}

TEST_F(CreatureFightSystemTest, ACreatureRefusesAFightWithOneTooBigOrTooSmall)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto big = MakeCreature(registry, PlayerNames::PLAYER_ONE, {100.0f, 0.0f, 0.0f});
	const auto small = MakeCreature(registry, PlayerNames::PLAYER_TWO, {130.0f, 0.0f, 0.0f});
	registry.Get<Creature>(big).size = 1.5f;
	registry.Get<Creature>(small).size = 0.6f;
	using Start = ecs::systems::CreatureFightSystemInterface::StartResult;
	// Less than half the asker's height, it says no, either way round
	EXPECT_EQ(fights.StartFight(big, small), Start::Refused);
	EXPECT_EQ(fights.StartFight(small, big), Start::Refused);
	EXPECT_FALSE(registry.AnyOf<CreatureFighting>(big));
	EXPECT_FALSE(registry.AnyOf<CreatureFighting>(small));
	// Only one of the two under a script's control: refused too
	registry.Get<Creature>(small).size = 1.0f;
	registry.Assign<ScriptControlled>(small);
	EXPECT_EQ(fights.StartFight(big, small), Start::Refused);
}

// A creature that does a joint action with a third creature refuses a fight; its partner may still ask it
TEST_F(CreatureFightSystemTest, ACreatureTakenUpWithAThirdCreatureRefusesAFight)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto asker = MakeCreature(registry, PlayerNames::PLAYER_ONE, {100.0f, 0.0f, 0.0f});
	const auto asked = MakeCreature(registry, PlayerNames::PLAYER_TWO, {130.0f, 0.0f, 0.0f});
	const auto third = MakeCreature(registry, PlayerNames::PLAYER_TWO, {160.0f, 0.0f, 0.0f});
	using Start = ecs::systems::CreatureFightSystemInterface::StartResult;
	registry.Assign<CreaturePartner>(asked, CreaturePartner {.partner = third});
	EXPECT_EQ(fights.StartFight(asker, asked), Start::Refused);
	EXPECT_FALSE(registry.AnyOf<CreatureFighting>(asked));
	registry.AssignOrReplace<CreaturePartner>(asked, CreaturePartner {.partner = asker});
	EXPECT_EQ(fights.StartFight(asker, asked), Start::Started);
}

TEST_F(CreatureFightSystemTest, AKnockOutKeepsTheWinnerAndTheLoserFromReactions)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Assign<CreatureMindState>(first).idle.serial = 4;
	registry.Assign<CreatureMindState>(second).idle.serial = 9;
	Locator::creatureMindSystem::emplace<ForcedMinds>();
	fights.KnockOut(second);
	// Each keeps the number of the agenda that it had at the knock-out
	ASSERT_TRUE(registry.AllOf<CreatureAfterFight>(first));
	ASSERT_TRUE(registry.AllOf<CreatureAfterFight>(second));
	EXPECT_EQ(registry.Get<const CreatureAfterFight>(first).agenda, 4u);
	EXPECT_EQ(registry.Get<const CreatureAfterFight>(second).agenda, 9u);
	Locator::creatureMindSystem::reset();
}

TEST_F(CreatureFightSystemTest, AFightFacesOffAndTauntsBeforeItLooksForAnArena)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Remove<CreatureFighting>(first);
	registry.Remove<CreatureFighting>(second);
	ASSERT_EQ(fights.StartFight(first, second), ecs::systems::CreatureFightSystemInterface::StartResult::Started);
	EXPECT_EQ(registry.Get<CreatureFighting>(first).stage, CreatureFighting::Stage::FaceOff);
	EXPECT_EQ(registry.Get<CreatureFighting>(second).stage, CreatureFighting::Stage::FaceOff);
	EXPECT_TRUE(registry.Get<CreatureFighting>(first).arenaEntity == entt::null);
	EXPECT_TRUE(fights.IsSettingUpFight(second));

	// The first creature asks, tells the other creature not to think and to turn, and turns itself on the fourth turn.
	// With no legs, nothing turns. On the next turn it tells the other creature to taunt with it.
	for (int turn = 0; turn < 4; ++turn)
	{
		fights.ProcessTurn();
	}
	EXPECT_EQ(registry.Get<CreatureFighting>(first).stage, CreatureFighting::Stage::FaceOff);
	fights.ProcessTurn();
	EXPECT_EQ(registry.Get<CreatureFighting>(first).stage, CreatureFighting::Stage::Taunt);
	EXPECT_EQ(registry.Get<CreatureFighting>(second).stage, CreatureFighting::Stage::Taunt);

	// This land has no room for an arena, so after the taunts the fight stops for both
	for (int turn = 0; turn < 4 && registry.AnyOf<CreatureFighting>(first); ++turn)
	{
		fights.ProcessTurn();
	}
	EXPECT_FALSE(registry.AnyOf<CreatureFighting>(first));
	EXPECT_FALSE(registry.AnyOf<CreatureFighting>(second));
}

namespace
{
/// The hands of the creatures. They record each creature that drops what it holds, and are busy for the creatures in
/// the busy list.
class RecordedHands final: public ecs::systems::CreatureObjectActionSystemInterface
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
	void Cancel(entt::entity) override {}
	void Drop(entt::entity creature) override { dropped.push_back(creature); }
	[[nodiscard]] State GetState(entt::entity creature) const override
	{
		return std::ranges::find(busy, creature) != busy.end() ? State::Busy : State::Idle;
	}
	[[nodiscard]] bool IsCatching(entt::entity) const override { return false; }
	[[nodiscard]] std::optional<float> GetProgress(entt::entity) const override { return std::nullopt; }
	[[nodiscard]] std::optional<entt::entity> GetHeld(entt::entity) const override { return std::nullopt; }
	[[nodiscard]] std::optional<float> FoodValueOf(entt::entity) const override { return std::nullopt; }
	[[nodiscard]] bool CanPickUp(entt::entity, entt::entity) const override { return false; }
	[[nodiscard]] bool CanEat(entt::entity, entt::entity) const override { return false; }
	[[nodiscard]] bool CanEatHeld(entt::entity) const override { return false; }
	[[nodiscard]] bool CanDestroy(entt::entity) const override { return false; }

	std::vector<entt::entity> dropped;
	std::vector<entt::entity> busy;
};
} // namespace

// Each creature keeps what it holds as the fight starts. Before its taunt, it checks its hands: with a thing in the hand
// it starts to let go, and it tries the taunt from the next turn with no new check. The let-go keeps the body busy.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, EachCreatureLetsGoOfWhatItHoldsBeforeItsTaunt)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Remove<CreatureFighting>(first);
	registry.Remove<CreatureFighting>(second);
	auto& hands = static_cast<RecordedHands&>(Locator::creatureObjectActionSystem::emplace<RecordedHands>());
	auto& minds = static_cast<ForcedMinds&>(Locator::creatureMindSystem::emplace<ForcedMinds>());
	auto& game = static_cast<ListedGameRandom&>(Locator::gameRandom::emplace<ListedGameRandom>());
	ASSERT_EQ(fights.StartFight(first, second), ecs::systems::CreatureFightSystemInterface::StartResult::Started);
	EXPECT_TRUE(hands.dropped.empty());
	EXPECT_EQ(minds.handsOf.size(), 0u);

	using creature_mind::HandsCheck;
	minds.handsOf[first] = {{HandsCheck::Wait, HandsCheck::LetGo}, 0};
	minds.handsOf[second] = {{}, 0};
	registry.Get<CreatureFighting>(first).stage = CreatureFighting::Stage::Taunt;
	registry.Get<CreatureFighting>(second).stage = CreatureFighting::Stage::Taunt;
	const auto& fighter = registry.Get<const CreatureFighting>(first);
	game.ranges.clear();

	// The other creature has empty hands, so it taunts on the first turn. The first creature cannot start to let go.
	fights.ProcessTurn();
	EXPECT_EQ(minds.handsOf[first].second, 1u);
	EXPECT_EQ(minds.handsOf[second].second, 1u);
	EXPECT_TRUE(registry.Get<const CreatureFighting>(second).played);
	EXPECT_FALSE(fighter.played);
	EXPECT_FALSE(fighter.handsChecked);
	EXPECT_EQ(game.ranges.size(), 1u);

	// It starts to let go, and does not try the taunt on this turn
	fights.ProcessTurn();
	EXPECT_EQ(minds.handsOf[first].second, 2u);
	EXPECT_TRUE(fighter.handsChecked);
	EXPECT_FALSE(fighter.played);
	EXPECT_EQ(game.ranges.size(), 1u);

	// While the let-go plays, each try draws a side and fails, with no new check of the hands
	hands.busy = {first};
	fights.ProcessTurn();
	fights.ProcessTurn();
	EXPECT_EQ(minds.handsOf[first].second, 2u);
	EXPECT_FALSE(fighter.played);
	EXPECT_EQ(game.ranges.size(), 3u);

	// The let-go ends and the taunt starts
	hands.busy.clear();
	fights.ProcessTurn();
	EXPECT_EQ(minds.handsOf[first].second, 2u);
	EXPECT_TRUE(fighter.played);
	EXPECT_EQ(game.ranges.size(), 4u);
	EXPECT_TRUE(hands.dropped.empty());
	Locator::gameRandom::reset();
	Locator::creatureMindSystem::reset();
	Locator::creatureObjectActionSystem::reset();
}

namespace
{
/// Legs that turn a creature to a heading at once, unless its body plays an action. They count the soft stops, the stops
/// at once and the asks to turn of each creature.
class TurningLegs final: public ecs::systems::CreatureLocomotionSystemInterface
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
	bool TurnToHeading(entt::entity creature, GameHeading heading) override
	{
		++asked[creature];
		auto& registry = Locator::entitiesRegistry::value();
		if (creature_layers::IsPlaying(registry.Get<const CreatureAnimation>(creature).body))
		{
			return false;
		}
		auto& legs = registry.Get<CreatureLocomotion>(creature);
		legs.heading = heading;
		legs.started = true;
		return true;
	}
	bool TurnToFaceAtOnce(entt::entity, glm::vec2) override { return false; }
	bool TurnToHeadingAtOnce(entt::entity, GameHeading) override { return false; }
	void Stop(entt::entity creature) override { ++stopped[creature]; }
	void StopAtOnce(entt::entity creature) override { ++stoppedAtOnce[creature]; }
	void ResetPace(entt::entity) override {}
	void Place(entt::entity, glm::vec3) override {}
	void PlaceWithoutWalking(entt::entity, glm::vec3) override {}
	[[nodiscard]] bool IsMoving(entt::entity) const override { return false; }
	[[nodiscard]] float KeptFraction(entt::entity) const override { return 0.0f; }
	[[nodiscard]] const creature_route::WalkableLand& GetWalkableLand() override
	{
		throw std::logic_error("the fights do not ask for the walkable land");
	}
	[[nodiscard]] std::vector<creature_route::Circle> ThingCirclesIn(entt::entity, glm::ivec2) override { return {}; }

	std::map<entt::entity, size_t> asked;
	std::map<entt::entity, size_t> stopped;
	std::map<entt::entity, size_t> stoppedAtOnce;
};
} // namespace

// As a fight starts, an action that the body of a creature plays continues. The request does not wait for the body,
// but the turn to face does. The other creature turns from the third turn and the first creature from the fourth.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, TheFaceOffTurnWaitsForTheBodyThatPlaysAnAction)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Remove<CreatureFighting>(first);
	registry.Remove<CreatureFighting>(second);
	auto& legs = static_cast<TurningLegs&>(Locator::creatureLocomotionSystem::emplace<TurningLegs>());
	using Body = creature_layers::BodyAction;
	registry.Get<CreatureAnimation>(first).body = Body {.kind = Body::Kind::Once, .animations = {5, 0, 0}};
	ASSERT_EQ(fights.StartFight(first, second), ecs::systems::CreatureFightSystemInterface::StartResult::Started);
	// Both stop as a plan stops, and the action of the body plays on
	EXPECT_EQ(legs.stopped[first], 1u);
	EXPECT_EQ(legs.stopped[second], 1u);
	EXPECT_TRUE(legs.stoppedAtOnce.empty());
	EXPECT_TRUE(creature_layers::IsPlaying(registry.Get<const CreatureAnimation>(first).body));

	// The first two turns are the request and the order not to think
	fights.ProcessTurn();
	fights.ProcessTurn();
	EXPECT_TRUE(legs.asked.empty());
	// The other creature turns from the third turn, the first creature from the fourth. Its body refuses the turn while
	// it plays the action.
	fights.ProcessTurn();
	fights.ProcessTurn();
	fights.ProcessTurn();
	EXPECT_GE(legs.asked[second], 1u);
	EXPECT_EQ(legs.asked[first], 2u);
	EXPECT_EQ(registry.Get<const CreatureFighting>(first).faceTurn.phase, creature_turn_to_face::Turn::Phase::Start);

	// The action ends. The turn starts, it faces the other creature, holds for a turn, and is done. On the next turn it
	// tells the other creature to taunt.
	registry.Get<CreatureAnimation>(first).body = {};
	int turns = 0;
	while (registry.Get<const CreatureFighting>(first).stage == CreatureFighting::Stage::FaceOff && turns < 20)
	{
		fights.ProcessTurn();
		++turns;
	}
	EXPECT_EQ(turns, 5);
	EXPECT_EQ(legs.asked[first], 3u);
	EXPECT_EQ(registry.Get<const CreatureFighting>(second).stage, CreatureFighting::Stage::Taunt);
	EXPECT_EQ(registry.Get<const CreatureFighting>(second).faceTurn.phase, creature_turn_to_face::Turn::Phase::Done);
	Locator::creatureLocomotionSystem::reset();
}

namespace
{
/// Keeps the kinds of the remarks asked for, and does nothing else
class RecordedGuidance final: public ecs::systems::GuidanceSystemInterface
{
public:
	void Reset() override {}
	void ProcessTurn() override {}
	bool PlayNow(guidance::Kind) override { return false; }
	bool RemarkNow(guidance::Kind) override { return false; }
	void RemarkIfNow(guidance::Kind kind) override { asked.push_back(kind); }
	void TownAttacked(entt::entity, PlayerNames, bool) override {}
	void TownAttackCry(entt::entity, const guidance::TownAttackVoice&) override {}
	void BeliefVoice(glm::vec3, float, GuidanceAlignment) override {}
	void BuildingBroken(entt::entity, std::optional<PlayerNames>, float) override {}
	void AlignmentTurn(float) override {}
	void SayOneOff(guidance::OneOff) override {}
	void KilledVillager(entt::entity) override {}
	void CreatureWonFight(entt::entity creature) override { wonFight.push_back(creature); }
	void WorshipperDied() override {}
	void LosingVillager(entt::entity) override {}
	void LowOnPeople(entt::entity) override {}
	void EndWaits() override {}
	[[nodiscard]] const std::vector<Remark>& GetRemarks() const override { return remarks; }
	[[nodiscard]] int32_t GetMoonWait() const override { return 0; }

	std::vector<guidance::Kind> asked;
	std::vector<entt::entity> wonFight;
	std::vector<Remark> remarks;
};
} // namespace

TEST_F(CreatureFightSystemTest, TheAdvisorsSpeakOfAFightOfThePlayersCreatureTooFarToShow)
{
	auto& guidance = static_cast<RecordedGuidance&>(Locator::guidanceSystem::emplace<RecordedGuidance>());
	fights.Reset();
	auto& camera = Locator::camera::value();
	camera.SetOrigin({0.0f, 30.0f, 2000.0f});
	camera.SetFocus({0.0f, 0.0f, 1900.0f});
	ShowTheFight({0.0f, 0.0f, 0.0f});
	EXPECT_FALSE(fights.IsCameraOnFight());
	EXPECT_EQ(guidance.asked, std::vector {guidance::Kind::CreatureFight});
	// A fight near enough is shown, and the advisors say nothing
	guidance.asked.clear();
	camera.SetOrigin({0.0f, 30.0f, 40.0f});
	camera.SetFocus({0.0f, 0.0f, 0.0f});
	ShowTheFight({0.0f, 0.0f, 0.0f});
	EXPECT_TRUE(fights.IsCameraOnFight());
	EXPECT_TRUE(guidance.asked.empty());
	// A fight far away between creatures of other players is not spoken of
	fights.Reset();
	Locator::entitiesRegistry::value().Get<Creature>(first).owner = PlayerNames::PLAYER_THREE;
	camera.SetOrigin({0.0f, 30.0f, 2000.0f});
	camera.SetFocus({0.0f, 0.0f, 1900.0f});
	ShowTheFight({0.0f, 0.0f, 0.0f});
	EXPECT_TRUE(guidance.asked.empty());
	Locator::guidanceSystem::reset();
}

TEST_F(CreatureFightSystemTest, AnOpponentWithNoLifeLeftLosesTheDuelToTheCreatureWhoseTurnItIs)
{
	auto& guidance = static_cast<RecordedGuidance&>(Locator::guidanceSystem::emplace<RecordedGuidance>());
	auto& reactions = static_cast<RecordedReactions&>(Locator::reactionSystem::emplace<RecordedReactions>());
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto self : {first, second})
	{
		fight::Enter(registry.Get<CreatureFighting>(self).fighter, fight::State::Stance);
		registry.Get<CreatureFighting>(self).fighter.health = 0.6f;
	}
	// The second creature has no life left, but its fight health is not zero
	registry.Assign<CreatureNeeds>(second).needs.life = 0.0f;
	// A creature that a script controls continues its duel
	registry.Assign<ScriptControlled>(first);
	fights.FightTurn(first);
	EXPECT_EQ(registry.Get<const CreatureFighting>(first).stage, CreatureFighting::Stage::Duel);
	registry.Remove<ScriptControlled>(first);
	fights.FightTurn(first);
	const auto& won = registry.Get<const CreatureFighting>(first);
	EXPECT_TRUE(won.ended);
	EXPECT_TRUE(won.fighter.stopping);
	EXPECT_EQ(won.stage, CreatureFighting::Stage::Respond);
	EXPECT_EQ(registry.Get<const CreatureFightRecord>(first).wins, 1u);
	EXPECT_EQ(guidance.wonFight, std::vector {first});
	ASSERT_EQ(reactions.made.size(), 1u);
	EXPECT_EQ(reactions.made.front().type, Reaction::ReactToFightWon);
	EXPECT_EQ(reactions.made.front().initiator, first);
	EXPECT_EQ(reactions.made.front().player, PlayerNames::PLAYER_ONE);
	// The loser does not faint
	EXPECT_FALSE(fights.IsKnockedOut(second));
	Locator::reactionSystem::reset();
	Locator::guidanceSystem::reset();
}

TEST_F(CreatureFightSystemTest, TheLoserOfADuelWonByItsLackOfLifeFightsOnUntilTheFightOfTheWinnerEnds)
{
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto self : {first, second})
	{
		fight::Enter(registry.Get<CreatureFighting>(self).fighter, fight::State::Stance);
		registry.Get<CreatureFighting>(self).fighter.health = 0.6f;
	}
	registry.Assign<CreatureNeeds>(first).needs.life = 1.0f;
	registry.Assign<CreatureNeeds>(second).needs.life = 0.0f;
	fights.FightTurn(first);
	ASSERT_EQ(registry.Get<const CreatureFighting>(first).stage, CreatureFighting::Stage::Respond);
	// The loser stays in its duel against the winner, with no response and no pay from its life
	EXPECT_EQ(registry.Get<const CreatureFighting>(second).partnerLeft, CreatureFighting::PartnerLeft::Won);
	fights.FightTurn(second);
	EXPECT_EQ(registry.Get<const CreatureFighting>(second).stage, CreatureFighting::Stage::Duel);
	EXPECT_FALSE(registry.Get<const CreatureFighting>(second).ended);
	EXPECT_EQ(registry.Get<const CreatureNeeds>(second).needs.life, 0.0f);
	// The winner stands, plays its response, and leaves the fight
	fight::Enter(registry.Get<CreatureFighting>(first).fighter, fight::State::Idle);
	fights.FightTurn(first);
	registry.Get<CreatureAnimation>(first).body = {};
	fights.FightTurn(first);
	EXPECT_FALSE(registry.AllOf<CreatureFighting>(first));
	// The loser stops at once with no response. Its life pays, because it stands in its stance.
	const auto& stopped = registry.Get<const CreatureFighting>(second);
	EXPECT_EQ(stopped.stage, CreatureFighting::Stage::Stop);
	EXPECT_TRUE(stopped.ended);
	EXPECT_TRUE(stopped.fighter.stopping);
	EXPECT_FLOAT_EQ(registry.Get<const CreatureNeeds>(second).needs.life, fight::LifeAfterFight(0.0f, 0.6f));
	// Its move finished, it leaves the fight
	fight::Enter(registry.Get<CreatureFighting>(second).fighter, fight::State::Idle);
	fights.FightTurn(second);
	EXPECT_FALSE(registry.AllOf<CreatureFighting>(second));
}

TEST_F(CreatureFightSystemTest, TheFightStepsWaitWhileTheHandHoldsTheCreatureAndTheFightStopsWhenTheHandLetsGo)
{
	auto& hand = static_cast<HoldingHand&>(Locator::creatureHandSystem::emplace<HoldingHand>());
	auto& registry = Locator::entitiesRegistry::value();
	// The first creature responds to a fight that ended, and the second still duels
	{
		auto& responds = registry.Get<CreatureFighting>(first);
		responds.stage = CreatureFighting::Stage::Respond;
		responds.ended = true;
	}
	fight::Enter(registry.Get<CreatureFighting>(second).fighter, fight::State::Stance);
	registry.Get<CreatureFighting>(second).fighter.health = 0.6f;
	registry.Assign<CreatureNeeds>(second).needs.life = 1.0f;
	hand.held = first;
	fights.HandTookHold(first);
	EXPECT_TRUE(registry.Get<const CreatureFighting>(first).held);
	EXPECT_EQ(registry.Get<const CreatureFighting>(first).stage, CreatureFighting::Stage::Respond);
	// While held, its response does not start
	fights.FightTurn(first);
	EXPECT_FALSE(registry.Get<const CreatureFighting>(first).played);
	EXPECT_EQ(registry.Get<const CreatureFighting>(first).stageSeconds, 0.0f);
	// Let go, it leaves the fight with no response. It stops its opponent too.
	hand.held.reset();
	fights.HandLetGo(first);
	EXPECT_FALSE(registry.AllOf<CreatureFighting>(first));
	const auto& other = registry.Get<const CreatureFighting>(second);
	EXPECT_EQ(other.stage, CreatureFighting::Stage::Stop);
	EXPECT_TRUE(other.fighter.stopping);
	EXPECT_FLOAT_EQ(registry.Get<const CreatureNeeds>(second).needs.life, fight::LifeAfterFight(1.0f, 0.6f));
	Locator::creatureHandSystem::reset();
}

TEST_F(CreatureFightSystemTest, ACreatureThatTheHandHoldsInADuelLeavesTheDuelAndItsOpponentFightsOnUntilTheHandLetsGo)
{
	auto& hand = static_cast<HoldingHand&>(Locator::creatureHandSystem::emplace<HoldingHand>());
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto self : {first, second})
	{
		fight::Enter(registry.Get<CreatureFighting>(self).fighter, fight::State::Stance);
		registry.Get<CreatureFighting>(self).fighter.health = 0.6f;
		registry.Assign<CreatureNeeds>(self).needs.life = 1.0f;
	}
	hand.held = first;
	// The fights see the hold on the first turn of the hold
	fights.FightTurn(first);
	const auto& held = registry.Get<const CreatureFighting>(first);
	EXPECT_EQ(held.stage, CreatureFighting::Stage::Stop);
	EXPECT_TRUE(held.ended);
	EXPECT_TRUE(held.fighter.stopping);
	EXPECT_FLOAT_EQ(registry.Get<const CreatureNeeds>(first).needs.life, fight::LifeAfterFight(1.0f, 0.6f));
	EXPECT_EQ(registry.Get<const CreatureFighting>(second).partnerLeft, CreatureFighting::PartnerLeft::Held);
	EXPECT_EQ(registry.Get<const CreatureFighting>(second).stage, CreatureFighting::Stage::Duel);
	// The held creature finishes its move, but it does not leave the fight while held
	fight::Enter(registry.Get<CreatureFighting>(first).fighter, fight::State::Idle);
	fights.FightTurn(first);
	EXPECT_TRUE(registry.AllOf<CreatureFighting>(first));
	// Let go, it leaves the fight, and its opponent stops with no response
	hand.held.reset();
	fights.HandLetGo(first);
	EXPECT_FALSE(registry.AllOf<CreatureFighting>(first));
	EXPECT_EQ(registry.Get<const CreatureFighting>(second).stage, CreatureFighting::Stage::Stop);
	EXPECT_TRUE(registry.Get<const CreatureFighting>(second).ended);
	Locator::creatureHandSystem::reset();
}

// After the hold, the plan asks the opponent again. The opponent agrees, and the fight starts when its move finished.
TEST_F(CreatureFightSystemTest, AFightAskedAgainAfterAHoldWaitsForTheMoveThatFinishes)
{
	using Result = ecs::systems::CreatureFightSystemInterface::StartResult;
	auto& hand = static_cast<HoldingHand&>(Locator::creatureHandSystem::emplace<HoldingHand>());
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto self : {first, second})
	{
		fight::Enter(registry.Get<CreatureFighting>(self).fighter, fight::State::Stance);
		registry.Assign<CreatureNeeds>(self).needs.life = 1.0f;
	}
	hand.held = first;
	fights.FightTurn(first);
	// While the hand holds it, the fight is not only a move that finishes
	EXPECT_EQ(fights.StartFight(first, second), Result::Busy);
	fight::Enter(registry.Get<CreatureFighting>(first).fighter, fight::State::Idle);
	hand.held.reset();
	fights.HandLetGo(first);
	ASSERT_FALSE(registry.AllOf<CreatureFighting>(first));
	ASSERT_EQ(registry.Get<const CreatureFighting>(second).stage, CreatureFighting::Stage::Stop);
	// The opponent still finishes its move: it agrees, but nothing starts
	EXPECT_EQ(fights.StartFight(first, second), Result::Finishing);
	EXPECT_FALSE(registry.AllOf<CreatureFighting>(first));
	EXPECT_EQ(registry.Get<const CreatureFighting>(second).stage, CreatureFighting::Stage::Stop);
	// Its move finished, it leaves the fight, and the new fight starts
	fight::Enter(registry.Get<CreatureFighting>(second).fighter, fight::State::Idle);
	fights.ProcessTurn();
	ASSERT_FALSE(registry.AllOf<CreatureFighting>(second));
	EXPECT_EQ(fights.StartFight(first, second), Result::Started);
	EXPECT_EQ(registry.Get<const CreatureFighting>(second).stage, CreatureFighting::Stage::FaceOff);
	Locator::creatureHandSystem::reset();
}

TEST_F(CreatureFightSystemTest, APlannedFightThatWaitsForAMoveAsksTheOtherCreatureOneTimeOnly)
{
	using Result = ecs::systems::CreatureFightSystemInterface::StartResult;
	auto& minds = static_cast<ForcedMinds&>(Locator::creatureMindSystem::emplace<ForcedMinds>());
	auto& hand = static_cast<HoldingHand&>(Locator::creatureHandSystem::emplace<HoldingHand>());
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto self : {first, second})
	{
		fight::Enter(registry.Get<CreatureFighting>(self).fighter, fight::State::Stance);
		registry.Assign<CreatureNeeds>(self).needs.life = 1.0f;
	}
	hand.held = first;
	fights.FightTurn(first);
	fight::Enter(registry.Get<CreatureFighting>(first).fighter, fight::State::Idle);
	hand.held.reset();
	fights.HandLetGo(first);
	ASSERT_FALSE(registry.AllOf<CreatureFighting>(first));
	ASSERT_EQ(registry.Get<const CreatureFighting>(second).stage, CreatureFighting::Stage::Stop);
	// The first question: the other creature agrees, and its move still finishes
	EXPECT_EQ(fights.StartPlannedFight(first, second, false), Result::Finishing);
	EXPECT_EQ(minds.deathChecks, 1u);
	// Now only the other creature is under a script's control, so a new question would get a refusal. The step keeps
	// the answer that it got and does not ask again.
	registry.Assign<ScriptControlled>(second);
	EXPECT_EQ(fights.StartPlannedFight(first, second, true), Result::Finishing);
	EXPECT_EQ(fights.StartPlannedFight(first, second, true), Result::Finishing);
	EXPECT_EQ(minds.deathChecks, 1u);
	// Its move finished, it leaves the fight, and the fight starts on the answer that it kept
	fight::Enter(registry.Get<CreatureFighting>(second).fighter, fight::State::Idle);
	fights.ProcessTurn();
	ASSERT_FALSE(registry.AllOf<CreatureFighting>(second));
	EXPECT_EQ(fights.StartPlannedFight(first, second, true), Result::Started);
	EXPECT_EQ(minds.deathChecks, 1u);
	EXPECT_EQ(registry.Get<const CreatureFighting>(second).stage, CreatureFighting::Stage::FaceOff);
	Locator::creatureHandSystem::reset();
}

TEST_F(CreatureFightSystemTest, TheFightMusicPlaysWhileTheHandDirectsTheFightOrNearItsArena)
{
	auto& registry = Locator::entitiesRegistry::value();
	EXPECT_TRUE(fights.DirectsFight());
	EXPECT_TRUE(fights.FightMusicHeard({0.0f, 30.0f, 2000.0f}));
	fights.Reset();
	// The arena of the fight, seen from near and from far
	const auto arena = registry.Get<const CreatureFighting>(first).arenaEntity;
	registry.Assign<Transform>(arena, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<CreatureArena>(arena, CreatureArena {.place = {0, 0},
	                                                     .radius = 40.0f,
	                                                     .temporary = true,
	                                                     .fightOn = true,
	                                                     .first = first,
	                                                     .second = second,
	                                                     .ring = nullptr});
	EXPECT_TRUE(fights.FightMusicHeard({0.0f, 30.0f, 90.0f}));
	EXPECT_FALSE(fights.FightMusicHeard({0.0f, 30.0f, 2000.0f}));
	// No fight is on in the arena, or the player's creature is not in it
	registry.Get<CreatureArena>(arena).fightOn = false;
	EXPECT_FALSE(fights.FightMusicHeard({0.0f, 30.0f, 90.0f}));
	registry.Get<CreatureArena>(arena).fightOn = true;
	registry.Get<Creature>(first).owner = PlayerNames::PLAYER_THREE;
	EXPECT_FALSE(fights.FightMusicHeard({0.0f, 30.0f, 90.0f}));
}

TEST_F(CreatureFightSystemTest, AFightGestureIsTakenOnlyWhenTheLineToTheOpponentMeetsItsBody)
{
	// Neither creature has a body, so the line from the player's creature meets nothing
	EXPECT_FALSE(fights.GestureSpecialMove());
	EXPECT_FALSE(fights.GestureSpell(MagicType::Fireball));
	EXPECT_TRUE(QueueOf(first).Empty());
	// Without a duel of the player's creature, no gesture is taken
	Locator::entitiesRegistry::value().Get<Creature>(first).owner = PlayerNames::PLAYER_THREE;
	EXPECT_EQ(fights.PlayersFighter(), std::nullopt);
	EXPECT_FALSE(fights.GestureSpecialMove());
}

TEST_F(CreatureFightSystemTest, ThePlayerDirectsTheirOwnCreatureAndTheComputerAnyOther)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Remove<CreatureFighting>(first);
	registry.Remove<CreatureFighting>(second);
	// The other creature starts the fight: who starts it does not matter
	ASSERT_EQ(fights.StartFight(second, first), ecs::systems::CreatureFightSystemInterface::StartResult::Started);
	EXPECT_EQ(registry.Get<const CreatureFighting>(first).fighter.control, fight::Control::Player);
	EXPECT_EQ(registry.Get<const CreatureFighting>(second).fighter.control, fight::Control::Computer);
	EXPECT_TRUE(fights.IsAutoFighting(first));
	EXPECT_TRUE(fights.IsAutoFighting(second));
}

TEST_F(CreatureFightSystemTest, EachHumanGodDirectsTheirOwnCreatureInANetworkGame)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& hands = static_cast<FakeHands&>(Locator::handSystem::emplace<FakeHands>());
	hands.hands[PlayerNames::PLAYER_ONE] = {registry.Create(), registry.Create()};
	hands.hands[PlayerNames::PLAYER_TWO] = {registry.Create(), registry.Create()};
	// The god of this computer is the second player: the computer has no part in it
	hands.own = PlayerNames::PLAYER_TWO;
	registry.Remove<CreatureFighting>(first);
	registry.Remove<CreatureFighting>(second);
	ASSERT_EQ(fights.StartFight(first, second), ecs::systems::CreatureFightSystemInterface::StartResult::Started);
	EXPECT_EQ(registry.Get<const CreatureFighting>(first).fighter.control, fight::Control::Player);
	EXPECT_EQ(registry.Get<const CreatureFighting>(second).fighter.control, fight::Control::Player);
}

TEST_F(CreatureFightSystemTest, ABlockTeachesTheFirstPlayerAloneAndEachHumanGodInANetworkGame)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& hands = static_cast<FakeHands&>(Locator::handSystem::emplace<FakeHands>());
	hands.hands[PlayerNames::PLAYER_ONE] = {registry.Create(), registry.Create()};
	hands.hands[PlayerNames::PLAYER_TWO] = {registry.Create(), registry.Create()};
	auto& time = static_cast<FakeTime&>(Locator::time::emplace<FakeTime>());
	// Both creatures stand in their stance near the middle of the arena, where they take moves from the queue
	for (const auto creature : {first, second})
	{
		registry.Assign<CreatureFightRecord>(creature);
		registry.Get<CreatureFighting>(creature).fighter.state = fight::State::Stance;
	}
	registry.Get<Transform>(first).position = {-1.0f, 0.0f, 0.0f};
	registry.Get<Transform>(second).position = {1.0f, 0.0f, 0.0f};
	const auto tendencyOf = [&](entt::entity creature) { return registry.Get<const CreatureFightRecord>(creature).tendency; };
	// A game alone: the block of the second player does not teach its creature, the block of the first player does
	fights.DoMove(PlayerNames::PLAYER_TWO, first, fight::BlockMove(), false);
	EXPECT_EQ(tendencyOf(second), 0.0f);
	fights.DoMove(PlayerNames::PLAYER_ONE, second, fight::BlockMove(), false);
	EXPECT_EQ(tendencyOf(first), fight::LearnTendency(0.0f, fight::Move::Kind::Block));
	// A network game: the block of the second player teaches its creature, also on the computer of the first player
	time.networkGame = true;
	registry.Get<CreatureFighting>(second).fighter = {};
	registry.Get<CreatureFighting>(second).fighter.state = fight::State::Stance;
	fights.DoMove(PlayerNames::PLAYER_TWO, first, fight::BlockMove(), false);
	EXPECT_EQ(tendencyOf(second), fight::LearnTendency(0.0f, fight::Move::Kind::Block));
}

TEST_F(CreatureFightSystemTest, AGodWithNoHandsLeavesItsCreatureToTheComputer)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& hands = static_cast<FakeHands&>(Locator::handSystem::emplace<FakeHands>());
	hands.hands[PlayerNames::PLAYER_TWO] = {registry.Create(), registry.Create()};
	hands.own = PlayerNames::PLAYER_TWO;
	registry.Remove<CreatureFighting>(first);
	registry.Remove<CreatureFighting>(second);
	ASSERT_EQ(fights.StartFight(first, second), ecs::systems::CreatureFightSystemInterface::StartResult::Started);
	EXPECT_EQ(registry.Get<const CreatureFighting>(first).fighter.control, fight::Control::Computer);
	EXPECT_EQ(registry.Get<const CreatureFighting>(second).fighter.control, fight::Control::Player);
}

TEST_F(CreatureFightSystemTest, TheHandOfThisComputerDirectsTheCreatureOfItsOwnGod)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& hands = static_cast<FakeHands&>(Locator::handSystem::emplace<FakeHands>());
	hands.hands[PlayerNames::PLAYER_ONE] = {registry.Create(), registry.Create()};
	hands.hands[PlayerNames::PLAYER_TWO] = {registry.Create(), registry.Create()};
	hands.own = PlayerNames::PLAYER_TWO;
	EXPECT_EQ(fights.PlayersFighter(), second);
	EXPECT_EQ(fights.HandTip(second), fight::Tip::Block);
	EXPECT_EQ(fights.HandTip(first), fight::Tip::Attack);
	// With no queue of commands, the press goes to the creature of the god of this computer
	EXPECT_TRUE(PressGround({15.0f, 10.0f}, fight::Button::Action));
	fights.Release(0, 0);
	EXPECT_EQ(QueueOf(second).Size(), 1u);
	EXPECT_TRUE(QueueOf(first).Empty());
}

TEST_F(CreatureFightSystemTest, TheFightCheatStopsTheDuelOfTheSendersCreature)
{
	Locator::reactionSystem::emplace<RecordedReactions>();
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto self : {first, second})
	{
		fight::Enter(registry.Get<CreatureFighting>(self).fighter, fight::State::Stance);
	}
	fights.DoFightCheat(PlayerNames::PLAYER_TWO);
	// The duel stops with no winner
	EXPECT_EQ(registry.Get<const CreatureFighting>(second).stage, CreatureFighting::Stage::Respond);
	EXPECT_EQ(registry.Get<const CreatureFighting>(first).stage, CreatureFighting::Stage::Respond);
	Locator::reactionSystem::reset();
}

TEST_F(CreatureFightSystemTest, TheFightCheatStartsAFightWithTheNearestCreature)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Remove<CreatureFighting>(first);
	registry.Remove<CreatureFighting>(second);
	// A third creature farther away is not the one that it fights
	MakeCreature(registry, PlayerNames::PLAYER_THREE, {200.0f, 0.0f, 0.0f});
	fights.DoFightCheat(PlayerNames::PLAYER_TWO);
	EXPECT_EQ(fights.OpponentOf(second), first);
	EXPECT_EQ(fights.OpponentOf(first), second);
	// A player with no creature does nothing
	fights.DoFightCheat(PlayerNames::PLAYER_FOUR);
}

TEST_F(CreatureFightSystemTest, LifeLostInADuelTakesTheFightHealthAndCanKnockTheCreatureOut)
{
	auto& registry = Locator::entitiesRegistry::value();
	Locator::creatureMindSystem::emplace<ForcedMinds>();
	registry.Get<CreatureFighting>(first).fighter.health = 0.5f;
	fights.LoseFightHealth(first, 0.2f);
	EXPECT_FLOAT_EQ(registry.Get<const CreatureFighting>(first).fighter.health, 0.3f);
	EXPECT_FALSE(fights.IsKnockedOut(first));
	// With none left it is knocked out, and its opponent wins
	fights.LoseFightHealth(first, 0.5f);
	EXPECT_TRUE(fights.IsKnockedOut(first));
	EXPECT_EQ(registry.Get<const CreatureFightRecord>(second).wins, 1u);
	Locator::creatureMindSystem::reset();
}

TEST_F(CreatureFightSystemTest, LifeLostOutOfADuelLeavesTheFightHealth)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Get<CreatureFighting>(first).stage = CreatureFighting::Stage::Taunt;
	const auto before = registry.Get<const CreatureFighting>(first).fighter.health;
	fights.LoseFightHealth(first, 0.2f);
	EXPECT_FLOAT_EQ(registry.Get<const CreatureFighting>(first).fighter.health, before);
}

// As the fight starts, the game's random numbers choose the taunt of each creature, the other creature's first. Each try
// to start the taunt draws a side first, and a body that plays an action refuses the try until it is free.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, TheGameChoosesTheTauntsAndEachTryDrawsASide)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Remove<CreatureFighting>(first);
	registry.Remove<CreatureFighting>(second);
	auto& game = static_cast<ListedGameRandom&>(Locator::gameRandom::emplace<ListedGameRandom>());
	game.numbers = {1, 0};
	ASSERT_EQ(fights.StartFight(first, second), ecs::systems::CreatureFightSystemInterface::StartResult::Started);
	EXPECT_EQ(game.ranges, (std::vector<uint32_t> {2, 2}));
	EXPECT_TRUE(registry.Get<const CreatureFighting>(second).rude);
	EXPECT_FALSE(registry.Get<const CreatureFighting>(first).rude);

	registry.Get<CreatureFighting>(first).stage = CreatureFighting::Stage::Taunt;
	registry.Get<CreatureFighting>(second).stage = CreatureFighting::Stage::Taunt;
	auto& body = registry.Get<CreatureAnimation>(first).body;
	body.kind = creature_layers::BodyAction::Kind::Once;
	for (int turn = 0; turn < 3; ++turn)
	{
		fights.ProcessTurn();
	}
	// The busy creature draws on each turn and does not taunt; the other draws once, taunts and goes on
	EXPECT_FALSE(registry.Get<const CreatureFighting>(first).played);
	EXPECT_EQ(registry.Get<const CreatureFighting>(first).stage, CreatureFighting::Stage::Taunt);
	EXPECT_NE(registry.Get<const CreatureFighting>(second).stage, CreatureFighting::Stage::Taunt);
	EXPECT_EQ(game.ranges.size(), 2u + 3u + 1u);
	Locator::gameRandom::reset();
}

// A faint starts on the body on the next turn of the knock-out. It cuts what the body plays and lies on the side that
// one draw of the game's random numbers chooses then.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, AFaintCutsTheBodyAndDrawsItsSide)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& game = static_cast<ListedGameRandom&>(Locator::gameRandom::emplace<ListedGameRandom>());
	game.numbers = {1};
	registry.Get<CreatureAnimation>(second).body.kind = creature_layers::BodyAction::Kind::Once;
	fights.ForceFaint(second);
	EXPECT_TRUE(game.ranges.empty());
	EXPECT_EQ(registry.Get<const CreatureAnimation>(second).body.kind, creature_layers::BodyAction::Kind::Once);
	fights.ProcessTurn();
	const auto& body = registry.Get<const CreatureAnimation>(second).body;
	EXPECT_EQ(body.kind, creature_layers::BodyAction::Kind::Sequence);
	EXPECT_TRUE(body.mirrored);
	EXPECT_EQ(game.ranges, (std::vector<uint32_t> {2}));
	Locator::gameRandom::reset();
}

// A faint that does not come from the mind first checks the hands. With a thing in the hand, the creature starts to let
// go, and the faint starts on the next turn with no new check. While the body or the hands are busy, the faint waits.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, AFaintWaitsForTheLetGoOfTheThingInTheHand)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& minds = static_cast<ForcedMinds&>(Locator::creatureMindSystem::emplace<ForcedMinds>());
	auto& game = static_cast<ListedGameRandom&>(Locator::gameRandom::emplace<ListedGameRandom>());
	using creature_mind::HandsCheck;
	minds.hands = {HandsCheck::Wait, HandsCheck::Wait, HandsCheck::LetGo};
	fights.ForceFaint(second);
	const auto faints = [&] {
		return registry.Get<const CreatureAnimation>(second).body.kind == creature_layers::BodyAction::Kind::Sequence;
	};
	// Two turns busy, then the let-go starts. The other creature, which stays in the fight, can draw the side of its
	// response in these turns.
	for (size_t turn = 1; turn <= 3; ++turn)
	{
		fights.ProcessTurn();
		EXPECT_FALSE(faints());
		EXPECT_EQ(minds.handChecks, turn);
	}
	EXPECT_TRUE(registry.Get<const CreatureKnockedOut>(second).handsChecked);
	// The faint starts with no new check and draws its side
	const auto drawn = game.ranges.size();
	fights.ProcessTurn();
	EXPECT_TRUE(faints());
	EXPECT_EQ(minds.handChecks, 3u);
	ASSERT_EQ(game.ranges.size(), drawn + 1);
	EXPECT_EQ(game.ranges.back(), 2u);
	EXPECT_FALSE(registry.Get<const CreatureKnockedOut>(second).fainting);
	Locator::gameRandom::reset();
	Locator::creatureMindSystem::reset();
}

// The knocked-out creature counts the time that it lies only from when it lies still: not while the faint waits for the
// hands, and not while the faint plays before it lies still.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, AKnockedOutCreatureCountsItsTimeFromWhenItLiesStill)
{
	auto& registry = Locator::entitiesRegistry::value();
	fights.ForceFaint(second);
	fights.ProcessTurn();
	fights.ProcessTurn();
	const auto& knockedOut = registry.Get<const CreatureKnockedOut>(second);
	EXPECT_LT(knockedOut.seconds, 0.0f);
	// It lies still: from now its time counts on each turn
	registry.Get<CreatureAnimation>(second).body.phase = creature_layers::BodyAction::Phase::Loop;
	fights.ProcessTurn();
	const auto first = knockedOut.seconds;
	fights.ProcessTurn();
	EXPECT_GT(knockedOut.seconds, first);
	EXPECT_EQ(knockedOut.stage, CreatureKnockedOut::Stage::Lying);
}

// The faint stops a walk at once as it starts on the body. An action of the hands can start a walk between the
// knock-out and the faint: if the walk stayed, the creature would get up to follow it and never be taken home.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, AFaintStopsAWalkThatStartedAfterTheKnockOut)
{
	auto& legs = static_cast<StoppingLegs&>(Locator::creatureLocomotionSystem::emplace<StoppingLegs>());
	fights.ForceFaint(second);
	// The knock-out stops the walk that the creature had
	ASSERT_EQ(legs.stopped, (std::vector<entt::entity> {second}));
	// The faint starts on the next turn and stops the walk again
	fights.ProcessTurn();
	EXPECT_EQ(Locator::entitiesRegistry::value().Get<const CreatureAnimation>(second).body.kind,
	          creature_layers::BodyAction::Kind::Sequence);
	EXPECT_EQ(legs.stopped, (std::vector<entt::entity> {second, second}));
	Locator::creatureLocomotionSystem::reset();
}

namespace
{
/// Every animation lasts one second; the fakes pose nothing
class SecondLongAnimations final: public ecs::systems::CreatureAnimationSystemInterface
{
public:
	void ProcessTurn() override {}
	void Update(std::chrono::duration<float, std::milli>) override {}
	void PoseTurn(entt::entity) override {}
	void PoseTurns() override {}
	void HoldTurnPose(entt::entity) override {}
	void PoseCopy(ecs::Registry&, entt::entity, float) override {}
	[[nodiscard]] std::optional<glm::vec3> BoneInAnimation(entt::entity, size_t, float, uint32_t, bool) override
	{
		return std::nullopt;
	}
	[[nodiscard]] std::optional<glm::vec3> MiddleOfBonesInAnimation(entt::entity, size_t, float,
	                                                                std::span<const uint32_t>) override
	{
		return std::nullopt;
	}
	[[nodiscard]] std::optional<float> AnimationDuration(entt::entity, size_t) override { return 1000.0f; }
	[[nodiscard]] std::optional<glm::vec3> AnimationTravel(entt::entity, size_t) override { return std::nullopt; }
	[[nodiscard]] std::optional<creature_kiss::KissPoints> KissPointsOf(entt::entity) override { return std::nullopt; }
	void KickSway(entt::entity, glm::vec3, glm::vec3) override {}
	void PlaceHandGlows(Registry&, entt::entity, int32_t, int32_t) override {}
	void SetCreedPower(entt::entity, creature_hand_glow::Place, float, float) override {}
	void LoadCreedPowers(entt::entity,
	                     std::span<const creature_hand_glow::SavedPower, creature_hand_glow::k_PlaceCount>) override
	{
	}
	void PlayCreedEvent(entt::entity, int32_t) override {}
	[[nodiscard]] std::optional<glm::vec3> MiddleOfGroinAndLips(const Registry&, entt::entity) override { return std::nullopt; }
	void AddAlignmentMotesOf(const Registry&, entt::entity) override {}
};

/// Both fighters are in a state, and only scripts would move them
void PutInStates(Registry& registry, entt::entity first, fight::State firstState, entt::entity second)
{
	for (const auto& [self, state] : {std::pair(first, firstState), std::pair(second, fight::State::Stance)})
	{
		auto& fighter = registry.Get<CreatureFighting>(self).fighter;
		fight::Enter(fighter, state);
		fighter.control = fight::Control::None;
	}
}
} // namespace

// The start ends when its time and the span of the turn reach its end, before it moves on. The stance that comes after
// it moves on in the same turn.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, TheStartEndsByTheSpanAndTheStanceMovesOnInTheSameTurn)
{
	Locator::creatureAnimationSystem::emplace<SecondLongAnimations>();
	auto& registry = Locator::entitiesRegistry::value();
	PutInStates(registry, first, fight::State::Start, second);
	auto& fighter = registry.Get<CreatureFighting>(first).fighter;
	// A creature of size 1: a span of 117 ms in a turn of 100 ms
	const auto span = fight::TestSpanMs(100.0f, ShownSize(registry.Get<const Creature>(first)), false);
	fighter.timeMs = static_cast<float>(1000 - span - 1);
	fights.ProcessTurn();
	// The start plays on by the span
	EXPECT_EQ(fighter.state, fight::State::Start);
	EXPECT_EQ(fighter.timeMs, static_cast<float>(1000 - 1));
	fighter.timeMs = static_cast<float>(1000 - span);
	fights.ProcessTurn();
	EXPECT_EQ(fighter.state, fight::State::Stance);
	EXPECT_EQ(fighter.timeMs,
	          static_cast<float>(fight::TurnAdvance(fight::PlaybackSpeed(fight::animations::k_Stance, 1.0f), 100)));
	Locator::creatureAnimationSystem::reset();
}

// The casting pose ends at the start of the turn after it began. The end of the cast moves on in the same turn.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, TheEndOfACastMovesOnInTheTurnThatThePoseEnds)
{
	Locator::creatureAnimationSystem::emplace<SecondLongAnimations>();
	auto& registry = Locator::entitiesRegistry::value();
	PutInStates(registry, first, fight::State::CastStart, second);
	auto& fighter = registry.Get<CreatureFighting>(first).fighter;
	const auto advance = fight::TurnAdvance(fight::PlaybackSpeed(fight::animations::k_StartCast, 1.0f), 100);
	fighter.timeMs = static_cast<float>(1000 - advance);
	fights.ProcessTurn();
	// The start of the cast ends; the pose begins and moves on
	EXPECT_EQ(fighter.state, fight::State::Cast);
	EXPECT_EQ(fighter.timeMs, static_cast<float>(advance));
	fights.ProcessTurn();
	EXPECT_EQ(fighter.state, fight::State::CastEnd);
	EXPECT_EQ(fighter.timeMs, static_cast<float>(advance));
	Locator::creatureAnimationSystem::reset();
}

// A fight called off finishes from the stance at once, and the finish moves on in the same turn. The finish plays at
// the fight's speed and ends when its time and the span reach its end.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, TheFinishMovesOnAtOnceAndEndsByTheSpan)
{
	Locator::creatureAnimationSystem::emplace<SecondLongAnimations>();
	auto& registry = Locator::entitiesRegistry::value();
	PutInStates(registry, first, fight::State::Stance, second);
	auto& fighter = registry.Get<CreatureFighting>(first).fighter;
	fighter.stopping = true;
	fights.ProcessTurn();
	const auto advance = fight::TurnAdvance(fight::PlaybackSpeed(fight::animations::k_Finish, 1.0f), 100);
	EXPECT_EQ(fighter.state, fight::State::Finish);
	EXPECT_EQ(fighter.timeMs, static_cast<float>(advance));
	const auto span = fight::TestSpanMs(100.0f, ShownSize(registry.Get<const Creature>(first)), false);
	fighter.timeMs = static_cast<float>(1000 - span);
	fights.ProcessTurn();
	EXPECT_EQ(fighter.state, fight::State::Idle);
	Locator::creatureAnimationSystem::reset();
}

// A frozen creature fights with fewer milliseconds in each turn: its animations move on by its own turn's time
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, AFrozenFighterMovesOnByItsOwnShorterTurn)
{
	Locator::creatureAnimationSystem::emplace<SecondLongAnimations>();
	auto& registry = Locator::entitiesRegistry::value();
	PutInStates(registry, first, fight::State::Stance, second);
	registry.Get<CreatureAnimation>(first).playbackScale = 0.5f;
	auto& fighter = registry.Get<CreatureFighting>(first).fighter;
	auto& other = registry.Get<CreatureFighting>(second).fighter;
	fighter.timeMs = 0.0f;
	other.timeMs = 0.0f;
	fights.ProcessTurn();
	const auto speed = fight::PlaybackSpeed(fight::animations::k_Stance, 1.0f);
	EXPECT_EQ(fighter.timeMs, static_cast<float>(fight::TurnAdvance(speed, 50)));
	EXPECT_EQ(other.timeMs, static_cast<float>(fight::TurnAdvance(speed, 100)));
	// Frozen through, it does not move on at all
	registry.Get<CreatureAnimation>(first).playbackScale = 0.0f;
	const auto before = fighter.timeMs;
	fights.ProcessTurn();
	EXPECT_EQ(fighter.timeMs, before);
	Locator::creatureAnimationSystem::reset();
}
