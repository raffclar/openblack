/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include <MindFile.h>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureDeeds.h"
#include "Creature/CreatureFace.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "Locator.h"

#define LOCATOR_IMPLEMENTATIONS

#include "ECS/Systems/Implementations/ResourceStoreSystem.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// Minds that record each deed of a player that the creatures can see
class SeeingMinds final: public CreatureMindSystemInterface
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
	void PlayerDid(size_t deed, const glm::vec3&, std::optional<entt::entity> object,
	               std::optional<PlayerNames> player) override
	{
		deeds.push_back({.deed = deed, .object = object, .player = player});
	}
	[[nodiscard]] const creature_mind_tables::Tables* GetTables() override { return nullptr; }
	bool PlayAction(entt::entity, size_t) override { return false; }
	bool PlayGesture(entt::entity, size_t) override { return false; }
	void PullFace(entt::entity, size_t) override {}
	std::optional<creature_face::Request> ShowFeeling(entt::entity, creature_face::Cue) override { return std::nullopt; }
	bool SitDown(entt::entity) override { return false; }
	void StandUp(entt::entity) override {}
	void ReceiveFeedback(entt::entity, float) override {}
	bool ForceAction(entt::entity, size_t, bool, double) override { return false; }
	bool Sleep(entt::entity) override { return false; }
	bool Eat(entt::entity, std::optional<entt::entity>) override { return false; }
	bool Drink(entt::entity) override { return false; }
	bool Poo(entt::entity) override { return false; }
	bool Puke(entt::entity) override { return false; }
	bool Faint(entt::entity) override { return false; }
	void Wake(entt::entity) override {}
	void FoughtFight(entt::entity, bool) override {}

	struct Deed
	{
		size_t deed;
		std::optional<entt::entity> object;
		std::optional<PlayerNames> player;
	};
	std::vector<Deed> deeds;
};

/// A workshop that takes a gift from the hand of a player
class WorkshopGiftDeedTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<Registry>();
		minds = &static_cast<SeeingMinds&>(Locator::creatureMindSystem::emplace<SeeingMinds>());
		auto& registry = Locator::entitiesRegistry::value();
		workshop = registry.Create();
		registry.Assign<Workshop>(workshop);
		registry.Assign<Transform>(
		    workshop,
		    Transform {.position = glm::vec3(10.0f, 0.0f, 20.0f), .rotation = glm::mat3(1.0f), .scale = glm::vec3(1.0f)});
		const auto player = registry.Create();
		registry.Assign<Player>(player, Player {.name = PlayerNames::PLAYER_ONE, .handTookFrom = PlayerNames::PLAYER_ONE});
	}
	void TearDown() override
	{
		Locator::creatureMindSystem::reset();
		Locator::entitiesRegistry::reset();
	}

	ResourceStoreSystem stores;
	SeeingMinds* minds {nullptr};
	entt::entity workshop {entt::null};
};
} // namespace

// Wood that a player gives to a workshop is the deed of wood put in the workshop, which the creatures of the player
// can copy
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(WorkshopGiftDeedTest, WoodGivenToAWorkshopIsTheDeedOfWoodInTheWorkshop)
{
	stores.ReportGift(workshop, ResourceType::Wood, PlayerNames::PLAYER_ONE);
	ASSERT_EQ(minds->deeds.size(), 1u);
	EXPECT_EQ(minds->deeds[0].deed, creature_deeds::k_PutWoodInWorkshop);
	EXPECT_EQ(minds->deeds[0].object, std::optional(workshop));
	EXPECT_EQ(minds->deeds[0].player, std::optional(PlayerNames::PLAYER_ONE));
}

// Food given to a workshop is no deed
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(WorkshopGiftDeedTest, FoodGivenToAWorkshopIsNoDeed)
{
	stores.ReportGift(workshop, ResourceType::Food, PlayerNames::PLAYER_ONE);
	EXPECT_TRUE(minds->deeds.empty());
}
