/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <chrono>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"

#define LOCATOR_IMPLEMENTATIONS

#include "ECS/Systems/Implementations/CreatureMindSystem.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// Fights that record each call-off. Every creature is in a fight out of its duel, as after a fight that is over.
class RecordedFights final: public CreatureFightSystemInterface
{
public:
	void ProcessTurn() override {}
	void Reset() override {}
	void Update(std::chrono::duration<float, std::milli> /*gameTime*/) override {}
	StartResult StartFight(entt::entity /*creature*/, entt::entity /*opponent*/) override { return StartResult::Busy; }
	void AbortFight(entt::entity /*creature*/) override {}
	void CallOff(entt::entity creature) override { calledOff.push_back(creature); }
	void CallOffForPlan(entt::entity creature) override { calledOffForPlan.push_back(creature); }
	void GetUpForPlan(entt::entity creature) override { gotUpForPlan.push_back(creature); }
	[[nodiscard]] bool IsFighting(entt::entity /*creature*/) const override { return true; }
	[[nodiscard]] bool IsDuelling(entt::entity /*creature*/) const override { return false; }
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

	[[nodiscard]] float GetFightHealth(entt::entity /*creature*/) const override { return health; }
	void SetFightHealth(entt::entity /*creature*/, float value) override
	{
		health = value;
		healthsSet.push_back(value);
	}

	std::vector<entt::entity> calledOff;
	std::vector<entt::entity> calledOffForPlan;
	std::vector<entt::entity> gotUpForPlan;
	float health {1.0f};
	std::vector<float> healthsSet;
};

/// A creature after a fight that is over: the plans and the deeds that stop what it does
class CreatureForcedStopFightTest: public ::testing::Test
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
		_info = std::make_unique<InfoConstants>();
		// One deed that a creature copies, and one action for its plans
		_info->creatureMimic.at(0).chance = 1.0f;
		// The action of the take of a gift from the hand
		constexpr std::string_view k_Take = "TakeObjectFromHand";
		std::ranges::copy(k_Take, _info->creatureAction.at(1).name.begin());
		Locator::infoConstants::emplace(*_info);
		Locator::entitiesRegistry::emplace<Registry>();
		fights = &static_cast<RecordedFights&>(Locator::creatureFightSystem::emplace<RecordedFights>());
		auto& registry = Locator::entitiesRegistry::value();
		creature = registry.Create();
		registry.Assign<Creature>(creature, Creature {.owner = PlayerNames::PLAYER_ONE});
		registry.Assign<Transform>(
		    creature,
		    Transform {.position = glm::vec3(100.0f, 0.0f, 100.0f), .rotation = glm::mat3(1.0f), .scale = glm::vec3(1.0f)});
		registry.Assign<CreatureMindState>(creature);
		registry.Assign<CreatureAnimation>(creature);
		registry.Assign<CreatureFighting>(creature);
		minds.StartMind(creature);
	}
	void TearDown() override
	{
		Locator::creatureFightSystem::reset();
		Locator::entitiesRegistry::reset();
		Locator::infoConstants::reset();
	}

	std::unique_ptr<InfoConstants> _info;
	CreatureMindSystem minds;
	RecordedFights* fights {nullptr};
	entt::entity creature {entt::null};
};
} // namespace

// A deed that the creature sees and starts to copy stops what it does as a failure. This stops its fight for the plan.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureForcedStopFightTest, ADeedThatTheCreatureCopiesStopsItsFightForThePlan)
{
	ASSERT_TRUE(Locator::entitiesRegistry::value().Get<CreatureMindState>(creature).learnt.has_value());
	minds.PlayerDid(0, glm::vec3(100.0f, 0.0f, 100.0f), std::nullopt, std::nullopt);
	EXPECT_EQ(fights->calledOffForPlan, (std::vector<entt::entity> {creature}));
	EXPECT_TRUE(fights->calledOff.empty());
}

// A forced activity that ends the fight of the body again stops the fight for the plan
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureForcedStopFightTest, AReactionStopsTheFightForThePlan)
{
	static_cast<void>(minds.ForceActivity(creature, {.actionObject = creature, .forced = false, .fightBodyEndsAgain = true}));
	EXPECT_EQ(fights->calledOffForPlan, (std::vector<entt::entity> {creature}));
	EXPECT_TRUE(fights->calledOff.empty());
}

// The other forced activities keep the plain call-off
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureForcedStopFightTest, TheOtherForcedActivitiesKeepThePlainCallOff)
{
	static_cast<void>(minds.ForceActivity(creature, {.actionObject = creature}));
	EXPECT_EQ(fights->calledOff, (std::vector<entt::entity> {creature}));
	EXPECT_TRUE(fights->calledOffForPlan.empty());
}

// A forced activity takes the place of a faint, as when the creature sees a villager do a skill that it does not know:
// a knocked-out creature gets up for it
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureForcedStopFightTest, AForcedActivityGetsTheCreatureUpFromItsFaint)
{
	static_cast<void>(minds.ForceActivity(creature, {.actionObject = creature, .forced = false, .fightBodyEndsAgain = true}));
	EXPECT_EQ(fights->gotUpForPlan, (std::vector<entt::entity> {creature}));
}

// A plan forced by hand, such as the puke, also takes the place of a faint
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureForcedStopFightTest, AForcedPlanGetsTheCreatureUpFromItsFaint)
{
	static_cast<void>(minds.Puke(creature));
	EXPECT_EQ(fights->gotUpForPlan, (std::vector<entt::entity> {creature}));
}

// The puke, the order of a computer god and the take of a gift from the hand also end the fight of the body again
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureForcedStopFightTest, ThePukeStopsTheFightForThePlan)
{
	static_cast<void>(minds.Puke(creature));
	EXPECT_EQ(fights->calledOffForPlan, (std::vector<entt::entity> {creature}));
	EXPECT_TRUE(fights->calledOff.empty());
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureForcedStopFightTest, TheOrderOfAComputerGodStopsTheFightForThePlan)
{
	static_cast<void>(minds.ObeyGodOrder(creature, 0, entt::null));
	EXPECT_EQ(fights->calledOffForPlan, (std::vector<entt::entity> {creature}));
	EXPECT_TRUE(fights->calledOff.empty());
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureForcedStopFightTest, TheTakeOfAGiftStopsTheFightForThePlan)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto gift = registry.Create();
	static_cast<void>(minds.TakeFromHand(creature, gift, true, glm::vec2(0.0f)));
	EXPECT_EQ(fights->calledOffForPlan, (std::vector<entt::entity> {creature}));
	EXPECT_TRUE(fights->calledOff.empty());
}

// The mind file keeps the fight health that the last fight left, also below 0, and a load gives it back
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureForcedStopFightTest, TheMindFileKeepsTheFightHealthOfTheCreature)
{
	fights->health = -0.25f;
	const auto saved = minds.SaveMind(creature);
	ASSERT_TRUE(saved.has_value());
	EXPECT_EQ(saved->fightHealth, std::optional<float>(-0.25f));
	fights->health = 1.0f;
	minds.LoadMind(creature, std::make_shared<const creaturemind::MindFileData>(*saved));
	EXPECT_EQ(fights->healthsSet, (std::vector<float> {-0.25f}));
}

// A load of a file with the fight health that the creature has changes nothing
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureForcedStopFightTest, ALoadWithTheSameFightHealthSetsNothing)
{
	const auto saved = minds.SaveMind(creature);
	ASSERT_TRUE(saved.has_value());
	minds.LoadMind(creature, std::make_shared<const creaturemind::MindFileData>(*saved));
	EXPECT_TRUE(fights->healthsSet.empty());
}

// A stroke or a slap that reaches the mind stops what the creature does, and that also ends a faint: a knocked-out
// creature gets up where it lies
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureForcedStopFightTest, FeedbackGetsTheCreatureUpFromItsFaint)
{
	minds.ReceiveFeedback(creature, -1.0f);
	ASSERT_FALSE(fights->gotUpForPlan.empty());
	EXPECT_EQ(fights->gotUpForPlan.front(), creature);
}
