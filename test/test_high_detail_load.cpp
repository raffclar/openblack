/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <memory>

#include <entt/core/hashed_string.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "ECS/Components/HighDetail.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Registry.h"
#include "Locator.h"

#define LOCATOR_IMPLEMENTATIONS

#include "ECS/Systems/Implementations/HighDetailSystem.h"
#include "Resources/Resources.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
constexpr entt::id_type k_UsualModel = entt::hashed_string {"usual"}.value();
constexpr entt::id_type k_DetailedModel = entt::hashed_string {"misc/detailed"}.value();

class HighDetailLoadTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (spdlog::get("game") == nullptr)
		{
			spdlog::register_logger(std::make_shared<spdlog::logger>("game", std::make_shared<spdlog::sinks::null_sink_mt>()));
		}
		Locator::entitiesRegistry::emplace<Registry>();
		Locator::resources::emplace<resources::Resources>();
	}
	void TearDown() override
	{
		Locator::resources::reset();
		Locator::entitiesRegistry::reset();
	}

	/// A villager in high detail as a save keeps it, with an order, a grip and eyes that blinked and turned
	static entt::entity LoadedVillager()
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto thing = registry.Create();
		registry.Assign<Mesh>(thing, Mesh {.id = k_DetailedModel, .submeshId = 0, .bbSubmeshId = 0});
		auto& detail = registry.Assign<HighDetail>(thing);
		detail.usualModel = k_UsualModel;
		detail.face = high_detail_rules::Face::Boy;
		detail.orders.followIntroHand = true;
		detail.heldAt = glm::vec3(1.0f, 2.0f, 3.0f);
		detail.eyes = villager_eyes::Eyes {.blink = {}, .roll = 0.25f};
		return thing;
	}

	HighDetailSystem highDetail;
};

} // namespace

TEST_F(HighDetailLoadTest, ALoadKeepsTheOrdersTheGripAndTheEyesOfAThingInHighDetail)
{
	// The detailed model is in the cache, as the load of a land puts it there
	Locator::resources::value().GetMeshes().Register(k_DetailedModel, [] { return std::shared_ptr<graphics::L3DMesh> {}; });
	const auto thing = LoadedVillager();

	highDetail.RemakeModelsAfterLoad();

	const auto& registry = Locator::entitiesRegistry::value();
	EXPECT_EQ(registry.Get<Mesh>(thing).id, k_DetailedModel);
	const auto& detail = registry.Get<HighDetail>(thing);
	EXPECT_EQ(detail.usualModel, k_UsualModel);
	EXPECT_EQ(detail.face, high_detail_rules::Face::Boy);
	EXPECT_TRUE(detail.orders.followIntroHand);
	EXPECT_EQ(detail.heldAt, glm::vec3(1.0f, 2.0f, 3.0f));
	ASSERT_TRUE(detail.eyes.has_value());
	EXPECT_EQ(detail.eyes->roll, 0.25f);
}

TEST_F(HighDetailLoadTest, AThingWhoseDetailedModelDoesNotLoadWearsItsUsualModel)
{
	const auto thing = LoadedVillager();

	highDetail.RemakeModelsAfterLoad();

	const auto& registry = Locator::entitiesRegistry::value();
	EXPECT_EQ(registry.Get<Mesh>(thing).id, k_UsualModel);
	ASSERT_TRUE(registry.AllOf<HighDetail>(thing));
	EXPECT_FALSE(registry.Get<HighDetail>(thing).usualModel.has_value());
}
