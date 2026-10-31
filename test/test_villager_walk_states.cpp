/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <memory>
#include <vector>

#include <glm/gtx/vec_swizzle.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "ECS/Components/Dance.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerWork.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/DanceRules.h"
#include "ECS/Registry.h"
#include "ECS/Systems/PathfindingSystemInterface.h"
#include "ECS/WallHugRules.h"
#include "InfoConstants.h"
#include "Locator.h"

#define LOCATOR_IMPLEMENTATIONS

#include "ECS/MapProduction.h"
#include "ECS/Systems/Implementations/LivingActionSystem.h"
#include "ECS/Systems/Implementations/VillagerArtefactDance.h"
#include "ECS/Systems/Implementations/VillagerCreatureDance.h"
#include "ECS/Systems/Implementations/VillagerCreatureReactions.h"
#include "ECS/Systems/Implementations/VillagerDance.h"
#include "ECS/Systems/Implementations/VillagerFootpath.h"
#include "ECS/Systems/Implementations/VillagerForester.h"
#include "ECS/Systems/Implementations/VillagerHome.h"
#include "ECS/Systems/Implementations/VillagerMoveToObject.h"
#include "ECS/Systems/Implementations/VillagerReactions.h"
#include "ECS/Systems/Implementations/VillagerTeleport.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
constexpr glm::vec3 k_Here {1000.0f, 0.0f, 1000.0f};
/// The state that the walks of the tests go into when they end
constexpr auto k_Next = VillagerStates::WaitForAnimation;

/// One step of a walk: the villager, its top state at the step, and the goal of the walk at the step
struct Step
{
	entt::entity villager;
	VillagerStates top;
	glm::vec2 goal;
};

/// Records each step of a walk and does no other work. When `arrives` is true, a step puts the walker on its goal.
class StepRecorder final: public systems::PathfindingSystemInterface
{
public:
	void Update() override {}
	void WalkInOwnTurn(entt::entity walker) override
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto& wallHug = registry.Get<WallHug>(walker);
		const auto& action = registry.Get<const LivingAction>(walker);
		steps.push_back({
		    .villager = walker,
		    .top = static_cast<VillagerStates>(action.states.at(static_cast<size_t>(LivingAction::Index::Top))),
		    .goal = wall_hug::ToPoint(wallHug.goalPlace),
		});
		if (!arrives)
		{
			return;
		}
		const auto goal = wall_hug::ToPoint(wallHug.goalPlace);
		registry.Get<Transform>(walker).position = glm::vec3(goal.x, 0.0f, goal.y);
		wallHug.position = wallHug.goalPlace;
		wallHug.placedAt = goal;
		registry.Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
		                MoveStateFinalStepTag>(walker);
		registry.AssignOrReplace<MoveStateArrivedTag>(walker, MoveStateClockwise::Undefined,
		                                              wall_hug::ToPoint(wallHug.position));
	}
	void ProcessLookahead() override {}
	void LookAgain(entt::entity) override {}
	void WalkTo(entt::entity, glm::ivec2) override {}
	void SetupWalkRound(entt::entity, glm::ivec2) override {}
	bool WalkOne(entt::entity) override { return false; }

	std::vector<Step> steps;
	bool arrives {false};
};
} // namespace

/// One villager with a walk under way, and a recorder in the place of the walk
class VillagerWalkStates: public ::testing::Test
{
protected:
	void SetUp() override
	{
		for (const auto* name : {"ai", "game", "scripting"})
		{
			if (spdlog::get(name) == nullptr)
			{
				spdlog::create<spdlog::sinks::null_sink_st>(name);
			}
		}
		Locator::entitiesRegistry::emplace<Registry>();
		Locator::entitiesMap::emplace<MapProduction>();
		Locator::livingActionSystem::emplace<systems::LivingActionSystem>();
		_steps = &static_cast<StepRecorder&>(Locator::pathfindingSystem::emplace<StepRecorder>());
		_info = std::make_unique<InfoConstants>();
		for (auto& villager : _info->villager)
		{
			// The speed divides by these: zero is not a value of the game's data
			villager.divisorForTownNeedsSpeedMod = 1.0f;
			villager.maxFoodCarried = villager.maxTraderFoodCarried = 10;
			villager.maxWoodCarried = villager.maxTraderWoodCarried = 10;
			villager.moveState = LivingStates::LivingMoveToPos;
		}
		// The walk to a teleport stone ends in the state of the stone, as in the game's data
		_info->villagerStateTable.at(static_cast<size_t>(VillagerStates::GoTowardsTeleportReaction)).isFinalState = 1;
		_info->villagerStateTable.at(static_cast<size_t>(k_Next)).isFinalState = 1;
		// The dance of a script is a state of the scripts, so that its dancer stays in the dance
		_info->villagerStateTable.at(static_cast<size_t>(VillagerStates::InDance)).isScriptState = 1;
		Locator::infoConstants::emplace(*_info);

		auto& registry = Locator::entitiesRegistry::value();
		_villager = registry.Create();
		registry.Assign<Transform>(_villager, k_Here, glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Villager>(_villager).life = 1.0f;
		registry.Assign<WallHug>(
		    _villager, WallHug {.goalPlace = wall_hug::ToWhole(glm::xz(k_Here) + glm::vec2(30.0f, 0.0f)), .speed = 1.0f});
		registry.Assign<MoveStateLinearTag>(_villager, MoveStateClockwise::Undefined, glm::xz(k_Here));
		registry.Assign<LivingAction>(_villager, VillagerStates::MoveToPos, static_cast<uint16_t>(0));
		SetStates(VillagerStates::MoveToPos, k_Next);
	}
	void TearDown() override
	{
		Locator::infoConstants::reset();
		Locator::pathfindingSystem::reset();
		Locator::livingActionSystem::reset();
		Locator::entitiesMap::reset();
		Locator::entitiesRegistry::reset();
	}

	[[nodiscard]] LivingAction& Action() const { return Locator::entitiesRegistry::value().Get<LivingAction>(_villager); }
	[[nodiscard]] VillagerStates Top() const
	{
		return static_cast<VillagerStates>(Action().states.at(static_cast<size_t>(LivingAction::Index::Top)));
	}
	[[nodiscard]] VillagerStates Final() const
	{
		return static_cast<VillagerStates>(Action().states.at(static_cast<size_t>(LivingAction::Index::Final)));
	}
	void SetStates(VillagerStates top, VillagerStates final) const
	{
		Action().states.at(static_cast<size_t>(LivingAction::Index::Top)) = static_cast<uint8_t>(top);
		Action().states.at(static_cast<size_t>(LivingAction::Index::Final)) = static_cast<uint8_t>(final);
	}

	/// A dance about a place 20 m from the villager, with the villager as the one dancer of its one group
	entt::entity JoinDance(Dance::State state = Dance::State::Dancing) const
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto dance = registry.Create();
		auto& data = registry.Assign<Dance>(dance);
		data.place = map_coords::FromMetres(glm::xz(k_Here) + glm::vec2(20.0f, 0.0f));
		data.state = state;
		data.groups.all.emplace_back().dancers.push_back(_villager);
		registry.Assign<Dancer>(_villager, Dancer {.dance = dance, .group = 0});
		return dance;
	}
	/// Puts the villager on its place in its dance
	void StandOnSlot() const
	{
		const auto slot = villager_dance::SlotOf(_villager);
		ASSERT_TRUE(slot.has_value());
		Locator::entitiesRegistry::value().Get<Transform>(_villager).position = glm::vec3(slot->x, 0.0f, slot->y);
	}

	std::unique_ptr<InfoConstants> _info;
	StepRecorder* _steps {nullptr};
	entt::entity _villager {entt::null};
};

// The plain walk takes its step first. A step that ends the walk takes the villager into its next state in the same turn.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(VillagerWalkStates, ThePlainWalkStepsAndThenArrivesInTheSameTurn)
{
	_steps->arrives = true;
	Locator::livingActionSystem::value().VillagerCallState(Action(), LivingAction::Index::Top);
	ASSERT_EQ(_steps->steps.size(), 1u);
	EXPECT_EQ(_steps->steps.front().top, VillagerStates::MoveToPos);
	EXPECT_EQ(Top(), k_Next);
}

// A villager that the hand holds takes no step and does not arrive
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(VillagerWalkStates, AVillagerInTheHandTakesNoStep)
{
	_steps->arrives = true;
	Locator::entitiesRegistry::value().Assign<InHand>(_villager);
	villager_home::MoveToPos(Action());
	EXPECT_TRUE(_steps->steps.empty());
	EXPECT_EQ(Top(), VillagerStates::MoveToPos);
}

// A forester that goes to a forest walks as the plain walk does: one step, first
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(VillagerWalkStates, AForesterStepsAsThePlainWalk)
{
	SetStates(VillagerStates::ForesterMoveToForest, k_Next);
	Locator::livingActionSystem::value().VillagerCallState(Action(), LivingAction::Index::Top);
	ASSERT_EQ(_steps->steps.size(), 1u);
	EXPECT_EQ(_steps->steps.front().top, VillagerStates::ForesterMoveToForest);
}

// A walk to a thing that has gone takes no step
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(VillagerWalkStates, AWalkToAThingThatIsGoneTakesNoStep)
{
	SetStates(VillagerStates::MoveToObject, k_Next);
	Locator::entitiesRegistry::value().Assign<MoveTarget>(_villager, MoveTarget {.thing = entt::null});
	villager_move_to_object::MoveToObject(Action());
	EXPECT_TRUE(_steps->steps.empty());
}

// A villager within a step of the thing arrives with no step
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(VillagerWalkStates, AVillagerNearTheThingArrivesWithNoStep)
{
	auto& registry = Locator::entitiesRegistry::value();
	SetStates(VillagerStates::MoveToObject, k_Next);
	const auto thing = registry.Create();
	registry.Assign<Transform>(thing, k_Here + glm::vec3(0.01f, 0.0f, 0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<MoveTarget>(_villager, MoveTarget {.thing = thing});
	villager_move_to_object::MoveToObject(Action());
	EXPECT_TRUE(_steps->steps.empty());
	EXPECT_EQ(Top(), k_Next);
}

// The walk to a thing aims at where the thing is now, then takes its step
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(VillagerWalkStates, AWalkToAThingStepsTowardsWhereTheThingIsNow)
{
	auto& registry = Locator::entitiesRegistry::value();
	SetStates(VillagerStates::MoveToObject, k_Next);
	const auto thing = registry.Create();
	const glm::vec3 there = k_Here + glm::vec3(0.0f, 0.0f, 12.0f);
	registry.Assign<Transform>(thing, there, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<MoveTarget>(_villager, MoveTarget {.thing = thing});
	villager_move_to_object::MoveToObject(Action());
	ASSERT_EQ(_steps->steps.size(), 1u);
	EXPECT_EQ(_steps->steps.front().goal, glm::xz(there));
}

// A walk on a footpath takes its step before it looks at the footpath
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(VillagerWalkStates, AFootpathWalkStepsFirst)
{
	SetStates(VillagerStates::MoveOnPath, k_Next);
	villager_footpath::MoveOnPath(Action());
	ASSERT_EQ(_steps->steps.size(), 1u);
	EXPECT_EQ(_steps->steps.front().top, VillagerStates::MoveOnPath);
	// With no footpath to walk on, it then goes on into its next state
	EXPECT_EQ(Top(), k_Next);
}

// A dancer away from its place starts its walk to the place and takes the first step in the same turn
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(VillagerWalkStates, ADancerAwayFromItsPlaceStepsInTheTurnItSetsOff)
{
	JoinDance();
	SetStates(VillagerStates::InDance, VillagerStates::InvalidState);
	villager_dance::InDance(Action());
	ASSERT_EQ(_steps->steps.size(), 1u);
	EXPECT_EQ(_steps->steps.front().top, VillagerStates::MoveToDancePos);
	const auto slot = villager_dance::SlotOf(_villager);
	ASSERT_TRUE(slot.has_value());
	EXPECT_NEAR(_steps->steps.front().goal.x, slot->x, 0.01f);
	EXPECT_NEAR(_steps->steps.front().goal.y, slot->y, 0.01f);
	EXPECT_EQ(Final(), VillagerStates::InDance);
}

// A dancer on its place takes no step
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(VillagerWalkStates, ADancerOnItsPlaceTakesNoStep)
{
	JoinDance();
	StandOnSlot();
	SetStates(VillagerStates::InDance, VillagerStates::InvalidState);
	villager_dance::InDance(Action());
	EXPECT_TRUE(_steps->steps.empty());
	EXPECT_EQ(Top(), VillagerStates::InDance);
}

// The other dances walk as a dance does: a worshipper of a creature, and a dancer that reacts
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(VillagerWalkStates, TheOtherDancesStepAfterTheyStartTheWalk)
{
	JoinDance();
	for (const auto state : {VillagerStates::WorshippingCreature, VillagerStates::DanceWhileReacting})
	{
		_steps->steps.clear();
		SetStates(state, VillagerStates::InvalidState);
		Locator::livingActionSystem::value().VillagerCallState(Action(), LivingAction::Index::Top);
		ASSERT_EQ(_steps->steps.size(), 1u);
		EXPECT_EQ(_steps->steps.front().top, VillagerStates::MoveToDancePos);
		EXPECT_EQ(Final(), state);
	}
}

// A dancer that walks to its place takes no step when it has no dance, and steps to the new place of a moved group
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(VillagerWalkStates, AWalkToAPlaceInADanceStepsToThePlaceOfAMovedGroup)
{
	SetStates(VillagerStates::MoveToDancePos, VillagerStates::InDance);
	EXPECT_EQ(villager_dance::MoveToDancePos(Action()), 0u);
	EXPECT_TRUE(_steps->steps.empty());

	const auto dance = JoinDance();
	Locator::entitiesRegistry::value().Get<Dance>(dance).groups.all.front().moved = true;
	villager_dance::MoveToDancePos(Action());
	ASSERT_EQ(_steps->steps.size(), 1u);
	const auto slot = villager_dance::SlotOf(_villager);
	ASSERT_TRUE(slot.has_value());
	EXPECT_NEAR(_steps->steps.front().goal.x, slot->x, 0.01f);
	EXPECT_NEAR(_steps->steps.front().goal.y, slot->y, 0.01f);
}

// A dancer of an artefact dance that stands where a walk to the dance ends starts its walk to its place, with no step yet
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(VillagerWalkStates, AnArtefactDancerWhereTheWalkEndsStartsItsWalkWithNoStep)
{
	const auto dance = JoinDance();
	const auto arrive =
	    map_coords::ToMetres(ecs::dance_rules::ArrivePlace(Locator::entitiesRegistry::value().Get<const Dance>(dance).place));
	Locator::entitiesRegistry::value().Get<Transform>(_villager).position = glm::vec3(arrive.x, 0.0f, arrive.y);
	SetStates(VillagerStates::ArtifactDance, VillagerStates::InvalidState);
	villager_artefact_dance::ArtefactDance(Action());
	EXPECT_TRUE(_steps->steps.empty());
	EXPECT_EQ(Top(), VillagerStates::MoveToPos);
	EXPECT_EQ(Final(), VillagerStates::ArtifactDance);
}

// The states that openblack does not make yet take no step
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(VillagerWalkStates, TheStatesNotMadeYetTakeNoStep)
{
	for (const auto state :
	     {VillagerStates::MoveOnStructure, VillagerStates::FootballWalkToPosition, VillagerStates::MoveTowardsObjectToLookAt,
	      VillagerStates::DanceForEditingPurposes, VillagerStates::DanceButNotWorship, VillagerStates::FootballMoveToBall})
	{
		SetStates(state, k_Next);
		Locator::livingActionSystem::value().VillagerCallState(Action(), LivingAction::Index::Top);
	}
	EXPECT_TRUE(_steps->steps.empty());
}

// A villager that heads for a teleport stone takes no step in that state. It starts a plain walk round things to the
// stone, and the walk ends in the state of the stone again.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(VillagerWalkStates, AWalkToATeleportStoneTakesNoStepAndStartsThePlainWalk)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto stone = registry.Create();
	const auto stoneAt = k_Here + glm::vec3(30.0f, 0.0f, 0.0f);
	registry.Assign<Transform>(stone, stoneAt, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<TeleportTraveller>(_villager, TeleportTraveller {.stone = stone});
	SetStates(VillagerStates::GoTowardsTeleportReaction, VillagerStates::InvalidState);

	villager_teleport::GoTowardsTeleportReaction(Action());

	EXPECT_TRUE(_steps->steps.empty());
	EXPECT_EQ(Top(), VillagerStates::MoveToPos);
	EXPECT_NE(Top(), VillagerStates::GoTowardsTeleportReaction);
	EXPECT_EQ(Final(), VillagerStates::GoTowardsTeleportReaction);
	EXPECT_EQ(registry.Get<const WallHug>(_villager).goalPlace, wall_hug::ToWhole(glm::xz(stoneAt)));
	EXPECT_TRUE(registry.AllOf<MoveStateLinearTag>(_villager));
}

// A villager nearer to its stone than one step of its walk goes into the jump, with no step
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(VillagerWalkStates, AVillagerAtItsTeleportStoneGoesIntoTheJumpWithNoStep)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto stone = registry.Create();
	registry.Assign<Transform>(stone, k_Here, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<TeleportTraveller>(_villager, TeleportTraveller {.stone = stone});
	SetStates(VillagerStates::GoTowardsTeleportReaction, VillagerStates::InvalidState);

	villager_teleport::GoTowardsTeleportReaction(Action());

	EXPECT_TRUE(_steps->steps.empty());
	EXPECT_EQ(Top(), VillagerStates::TeleportReaction);
}

// A villager that leaves the stone for a state that it works towards, and that is not a state of the stone, stops on
// its way to worship
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(VillagerTeleportRules, OnlyAFinalStateOutsideTheStoneEndsTheWayToWorship)
{
	EXPECT_TRUE(villager_teleport::LeavesTheWayToWorship(VillagerStates::DecideWhatToDo, true));
	EXPECT_FALSE(villager_teleport::LeavesTheWayToWorship(VillagerStates::DecideWhatToDo, false));
	for (const auto state : villager_teleport::k_TeleportStates)
	{
		EXPECT_FALSE(villager_teleport::LeavesTheWayToWorship(state, true));
	}
}

// The exit of the stone's states takes the villager off its town's list of those on their way to worship when the
// villager goes to another state that it works towards. The walk to the stone keeps the villager on the list.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(VillagerWalkStates, LeavingTheStoneForAnotherTaskEndsTheWayToWorship)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto town = registry.Create();
	registry.Assign<Town>(town).onWayToWorship.push_back(_villager);
	registry.Get<Villager>(_villager).town = town;
	registry.Assign<Worshipper>(_villager).onTheWay = true;
	SetStates(VillagerStates::GoTowardsTeleportReaction, VillagerStates::InvalidState);

	villager_teleport::ExitReactToTeleport(Action(), VillagerStates::TeleportReaction);
	villager_teleport::ExitReactToTeleport(Action(), VillagerStates::MoveToPos);
	EXPECT_EQ(registry.Get<const Town>(town).onWayToWorship.size(), 1u);
	EXPECT_TRUE(registry.AllOf<Worshipper>(_villager));

	villager_teleport::ExitReactToTeleport(Action(), k_Next);
	EXPECT_TRUE(registry.Get<const Town>(town).onWayToWorship.empty());
	EXPECT_FALSE(registry.AllOf<Worshipper>(_villager));
}

// A villager that walks to a creature that is not there takes no step
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(VillagerWalkStates, AWalkToACreatureThatIsNotThereTakesNoStep)
{
	SetStates(VillagerStates::MoveTowardsCreatureReaction, k_Next);
	villager_creature::MoveTowardsCreatureReaction(Action());
	EXPECT_TRUE(_steps->steps.empty());
}
