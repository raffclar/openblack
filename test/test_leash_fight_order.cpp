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
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Creature/CreatureFight.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "Locator.h"

#define LOCATOR_IMPLEMENTATIONS 1
#include "ECS/Systems/Implementations/LeashSystem.h"
#include "Resources/Resources.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::systems;

namespace
{
/// A fight system that starts each fight as the real one starts it: the plain start refuses a creature with too little
/// life, the forced start does not. It keeps each start that the test asks for.
class StartingFights final: public CreatureFightSystemInterface
{
public:
	void ProcessTurn() override {}
	void Update(std::chrono::duration<float, std::milli> /*gameTime*/) override {}
	StartResult StartFight(entt::entity creature, entt::entity opponent) override
	{
		plain.emplace_back(creature, opponent);
		const auto* needs = Locator::entitiesRegistry::value().TryGet<const components::CreatureNeeds>(creature);
		return creature_fight::HealthyEnoughToFight(needs != nullptr ? needs->needs.life : 1.0f) ? StartResult::Started
		                                                                                         : StartResult::TooWeak;
	}
	StartResult StartForcedFight(entt::entity creature, entt::entity opponent) override
	{
		forced.emplace_back(creature, opponent);
		return StartResult::Started;
	}
	void AbortFight(entt::entity /*creature*/) override {}
	[[nodiscard]] bool IsFighting(entt::entity /*creature*/) const override { return false; }
	[[nodiscard]] std::optional<entt::entity> OpponentOf(entt::entity /*creature*/) const override { return std::nullopt; }
	bool QueueMove(entt::entity /*creature*/, const creature_fight::Move& /*move*/, bool /*replace*/) override { return false; }
	void ReleaseCharge(entt::entity /*creature*/, float /*heldMs*/) override {}
	void SetAutoFighting(entt::entity /*creature*/, bool /*autoFight*/) override {}
	[[nodiscard]] bool IsAutoFighting(entt::entity /*creature*/) const override { return false; }
	void Reset() override {}
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
	void ForceFaint(entt::entity /*creature*/) override {}
	void KillPermanently(entt::entity /*creature*/) override {}
	void Resurrect(entt::entity /*creature*/) override {}
	[[nodiscard]] bool IsKnockedOut(entt::entity /*creature*/) const override { return false; }
	[[nodiscard]] std::optional<creature_fight_hud::Values> GetPanel() const override { return std::nullopt; }
	void SetCameraWatches(bool /*enabled*/) override {}
	[[nodiscard]] bool GetCameraWatches() const override { return false; }
	[[nodiscard]] bool IsCameraOnFight() const override { return false; }

	std::vector<std::pair<entt::entity, entt::entity>> plain;
	std::vector<std::pair<entt::entity, entt::entity>> forced;
};

/// The player's creature on the aggression leash, near another creature
class LeashFightOrder: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// The leash writes to the game log
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_st>("game");
		}
		Locator::entitiesRegistry::emplace<Registry>();
		// The mark of the order looks for the mesh of its ring, which an empty store does not have
		Locator::resources::emplace<resources::Resources>();
		Locator::creatureFightSystem::emplace<StartingFights>();
		auto& registry = Locator::entitiesRegistry::value();
		creature = MakeCreature(registry, PlayerNames::PLAYER_ONE, {100.0f, 0.0f, 100.0f});
		other = MakeCreature(registry, PlayerNames::PLAYER_TWO, {104.0f, 0.0f, 100.0f});
		registry.Assign<components::CreatureLeash>(creature).selected = LeashType::Evil;
		// A creature must know the learning leash before it can wear any leash
		leashes.SetKnown(creature, LeashType::Rope, true);
		leashes.SetKnown(creature, LeashType::Evil, true);
	}
	void TearDown() override
	{
		Locator::creatureFightSystem::reset();
		Locator::resources::reset();
		Locator::entitiesRegistry::reset();
	}

	static entt::entity MakeCreature(Registry& registry, PlayerNames owner, glm::vec3 at)
	{
		const auto entity = registry.Create();
		registry.Assign<components::Transform>(entity, at, glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<components::Creature>(entity, components::Creature {.owner = owner, .leashable = true});
		return entity;
	}

	[[nodiscard]] static StartingFights& Fights()
	{
		return static_cast<StartingFights&>(Locator::creatureFightSystem::value());
	}

	LeashSystem leashes;
	entt::entity creature {entt::null};
	entt::entity other {entt::null};
};
} // namespace

// The leash order forces the fight. A creature with 0.1 of its life or less starts the fight too, because only the
// creature's own plan tests the life.
TEST_F(LeashFightOrder, ALeashOrderOnACreatureStartsAFightAlsoWithLittleLife)
{
	Locator::entitiesRegistry::value().Assign<components::CreatureNeeds>(creature).needs.life = 0.05f;
	ASSERT_TRUE(leashes.HandTie(creature, other));
	EXPECT_TRUE(Fights().plain.empty());
	ASSERT_EQ(Fights().forced.size(), 1U);
	EXPECT_EQ(Fights().forced.front(), std::pair(creature, other));
}

// A creature with no life left takes no leash order, so no fight starts
TEST_F(LeashFightOrder, ACreatureWithNoLifeTakesNoLeashOrderToFight)
{
	Locator::entitiesRegistry::value().Assign<components::CreatureNeeds>(creature).needs.life = 0.0f;
	ASSERT_TRUE(leashes.HandTie(creature, other));
	EXPECT_TRUE(Fights().plain.empty());
	EXPECT_TRUE(Fights().forced.empty());
}
