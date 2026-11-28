/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <memory>
#include <string_view>

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Creature/CreatureDesires.h"
#include "Creature/CreaturePlanner.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"

#define LOCATOR_IMPLEMENTATIONS 1
#include "ECS/Systems/Implementations/CreatureMindSystem.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using creature_desires::Desire;

namespace
{

constexpr std::string_view k_MimicPlayer = "MimicPlayer";

class CreatureKeptPlans: public ::testing::Test
{
protected:
	void SetUp() override
	{
		for (const auto* name : {"ai", "game"})
		{
			if (spdlog::get(name) == nullptr)
			{
				spdlog::create<spdlog::sinks::null_sink_st>(name);
			}
		}
		// The only action of the tables is the copy of the player, which makes no steps
		_info = std::make_unique<InfoConstants>();
		auto& row = _info->creatureAction.at(0);
		std::ranges::copy(k_MimicPlayer, row.name.begin());
		row.desire = static_cast<uint32_t>(Desire::FollowPlayerDesire);
		Locator::infoConstants::emplace(*_info);
		Locator::entitiesRegistry::emplace<Registry>();
		auto& registry = Locator::entitiesRegistry::value();
		_creature = registry.Create();
		registry.Assign<Creature>(_creature);
		registry.Assign<Transform>(_creature, glm::vec3(50.0f, 0.0f, 50.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<CreatureAnimation>(_creature);
		registry.Assign<CreatureMindState>(_creature);
		_minds.StartMind(_creature);
	}
	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		Locator::infoConstants::reset();
	}

	CreatureMindState& Mind() { return Locator::entitiesRegistry::value().Get<CreatureMindState>(_creature); }

	/// The creature has its next turn to think and to choose from the round of desires
	void NextTurn()
	{
		auto& mind = Mind();
		mind.planner.round.fill(false);
		mind.turn = mind.plannedTurn + 1;
		_minds.MindTurn(_creature);
		_minds.DesiresTurn(_creature);
	}

	std::unique_ptr<InfoConstants> _info;
	CreatureMindSystem _minds;
	entt::entity _creature {entt::null};
};

} // namespace

// The copy of the player makes no steps. The creature keeps the copy plan as its plan with no agenda, and the turn with
// no agenda gives the plan a priority of 0.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureKeptPlans, TheCopyOfThePlayerStaysAsThePlanWithNoAgenda)
{
	auto& mind = Mind();
	ASSERT_TRUE(mind.desires.has_value());
	mind.planner.best.at(static_cast<size_t>(Desire::FollowPlayerDesire)) = creature_planner::Plan {
	    .desire = Desire::FollowPlayerDesire,
	    .action = 0,
	    .priority = 2.0f,
	};

	NextTurn();

	ASSERT_TRUE(mind.planner.current.has_value());
	EXPECT_EQ(mind.planner.current->action, 0u);
	EXPECT_EQ(mind.planner.current->desire, Desire::FollowPlayerDesire);
	EXPECT_FLOAT_EQ(mind.planner.current->priority, 0.0f);
	EXPECT_FALSE(mind.planActive);
	EXPECT_TRUE(mind.idle.agenda.empty());
	// The next stop makes the action of the copy new
	EXPECT_EQ(creature_planner::ActionInView(mind.planner), 0u);
}

// A forced plan with no steps stays as the plan, marked as forced. The next turn with no plan removes the forced mark,
// the mark of the leash and the priority of the plan.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureKeptPlans, AForcedPlanWithNoStepsIsForcedUntilTheNextTurnWithNoPlan)
{
	ASSERT_TRUE(_minds.ForcePlan(_creature, {.desire = Desire::ObeyPlayer, .action = k_MimicPlayer, .ordered = true}));
	auto& mind = Mind();
	ASSERT_TRUE(mind.planner.current.has_value());
	EXPECT_FALSE(mind.planActive);
	EXPECT_TRUE(mind.planForced);
	// The order of the leash marks the creature as led by the leash after the forced plan
	mind.ledByLeash = true;

	NextTurn();

	EXPECT_FALSE(mind.planForced);
	EXPECT_FALSE(mind.ledByLeash);
	ASSERT_TRUE(mind.planner.current.has_value());
	EXPECT_FLOAT_EQ(mind.planner.current->priority, 0.0f);
}

// A forced plan that is not marked as forced, such as the order of a script, keeps no forced mark with no steps
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureKeptPlans, AnUnmarkedPlanWithNoStepsIsNotForced)
{
	ASSERT_TRUE(_minds.ForcePlan(_creature, {.desire = Desire::ObeyPlayer, .action = k_MimicPlayer, .forced = false}));
	EXPECT_TRUE(Mind().planner.current.has_value());
	EXPECT_FALSE(Mind().planForced);
}
