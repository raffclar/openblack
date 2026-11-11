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
#include <optional>
#include <string_view>

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Creature/CreatureDesires.h"
#include "Creature/CreaturePlanner.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/FishFarmSystemInterface.h"
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

/// The fish farms of a land with one farm, which is the closest farm to any point
class OneFishFarm final: public FishFarmSystemInterface
{
public:
	explicit OneFishFarm(entt::entity farm)
	    : _farm(farm)
	{
	}
	void ProcessTurn(uint32_t) override {}
	void Update(float, glm::vec3) override {}
	void Scare(glm::vec3) override {}
	[[nodiscard]] std::optional<entt::entity> FarmWithFishAt(glm::vec2) const override { return std::nullopt; }
	[[nodiscard]] fish_farm::Type GetType() const override { return {}; }
	uint32_t TakeFish(entt::entity, uint32_t) override { return 0u; }
	[[nodiscard]] float FishLeft(entt::entity) const override { return 0.0f; }
	[[nodiscard]] std::optional<entt::entity> ClosestFarm(glm::vec3, float) const override { return _farm; }
	[[nodiscard]] std::optional<FarmChoice> BestFarmFor(entt::entity, glm::vec3) const override { return std::nullopt; }
	void RemoveFarm(entt::entity) override {}
	void AddFisherman(entt::entity, entt::entity) override {}
	void RemoveFisherman(entt::entity, entt::entity) override {}
	[[nodiscard]] glm::vec3 FishingSpot(entt::entity) override { return glm::vec3(0.0f); }
	[[nodiscard]] std::optional<int32_t> Fish(entt::entity, uint32_t, uint32_t, float) override { return std::nullopt; }
	void Reset() override {}

private:
	entt::entity _farm;
};

constexpr std::string_view k_FishAndEat = "FishAndEat";

class CreatureFishingAdopt: public ::testing::Test
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
		// The only action of the tables is the fishing action of the hunger desire
		_info = std::make_unique<InfoConstants>();
		auto& row = _info->creatureAction.at(0);
		std::ranges::copy(k_FishAndEat, row.name.begin());
		row.desire = static_cast<uint32_t>(creature_desires::Desire::Hunger);
		Locator::infoConstants::emplace(*_info);
		Locator::entitiesRegistry::emplace<Registry>();
		auto& registry = Locator::entitiesRegistry::value();
		const auto farm = registry.Create();
		registry.Assign<FishFarm>(farm);
		registry.Assign<Transform>(farm, glm::vec3(100.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		Locator::fishFarmSystem::emplace<OneFishFarm>(farm);
		creature = registry.Create();
		registry.Assign<Creature>(creature);
		registry.Assign<Transform>(creature, glm::vec3(50.0f, 0.0f, 50.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<CreatureAnimation>(creature);
		registry.Assign<CreatureMindState>(creature);
		minds.StartMind(creature);
	}
	void TearDown() override
	{
		Locator::fishFarmSystem::reset();
		Locator::entitiesRegistry::reset();
		Locator::infoConstants::reset();
	}

	std::unique_ptr<InfoConstants> _info;
	CreatureMindSystem minds;
	entt::entity creature {entt::null};
};

} // namespace

// The round of desires ends in a turn that plans no fishing action. The creature still takes up the fishing plan of its
// hunger: it finds the fish farm as the plan starts.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFishingAdopt, AFishingPlanStartsInATurnThatPlansNoFishing)
{
	auto& mind = Locator::entitiesRegistry::value().Get<CreatureMindState>(creature);
	ASSERT_TRUE(mind.desires.has_value());
	mind.planner.round.fill(false);
	mind.planner.best.at(static_cast<size_t>(creature_desires::Desire::Hunger)) = creature_planner::Plan {
	    .desire = creature_desires::Desire::Hunger,
	    .action = 0,
	    .object = entt::to_integral(creature),
	    .priority = 2.0f,
	};
	mind.turn = mind.plannedTurn + 1;

	minds.PlanCreatureTurn(creature);

	ASSERT_TRUE(mind.planner.current.has_value());
	EXPECT_EQ(mind.planner.current->action, 0u);
	EXPECT_TRUE(mind.planActive);
	EXPECT_FALSE(mind.idle.agenda.empty());
}
