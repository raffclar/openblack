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
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/TornadoPileTake.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// A store whose pile says it holds a count, and gives at most what it really has
class FakeStores final: public systems::ResourceStoreSystemInterface
{
public:
	[[nodiscard]] ObjectResource ResourceOf(entt::entity) const override { return {}; }
	[[nodiscard]] bool IsStore(entt::entity, ResourceType) const override { return false; }
	uint32_t AddToStore(entt::entity, ResourceType, uint32_t, std::optional<PlayerNames>, bool,
	                    std::optional<glm::vec3>) override
	{
		return 0;
	}
	uint32_t AddToPile(entt::entity, ResourceType, uint32_t, bool) override { return 0; }
	uint32_t TakeFromPile(entt::entity, ResourceType, uint32_t, std::optional<PlayerNames>) override { return 0; }
	bool TakeObject(entt::entity, entt::entity, std::optional<PlayerNames>) override { return false; }
	bool TakeObjectAs(entt::entity, entt::entity, ObjectResource, std::optional<PlayerNames>) override { return false; }
	bool PourAt(ResourceType, glm::vec3, uint32_t, bool, PlayerNames, bool) override { return false; }
	[[nodiscard]] std::optional<entt::entity> StoreOf(entt::entity) const override { return std::nullopt; }
	[[nodiscard]] uint32_t GetResource(entt::entity object, ResourceType type) const override
	{
		asked.emplace_back(object, type);
		return count;
	}
	uint32_t AddResource(entt::entity, ResourceType, uint32_t, std::optional<glm::vec3>) override { return 0; }
	uint32_t RemoveResource(entt::entity object, ResourceType type, uint32_t amount) override
	{
		removed.emplace_back(object, type, amount);
		return std::min(amount, gives);
	}
	uint32_t AddToTownsSite(entt::entity, entt::entity, ResourceType, uint32_t, std::optional<glm::vec3>) override { return 0; }
	uint32_t RemoveFromTownsSite(entt::entity, entt::entity, ResourceType, uint32_t) override { return 0; }

	uint32_t count {0};
	uint32_t gives {0};
	mutable std::vector<std::pair<entt::entity, ResourceType>> asked;
	std::vector<std::tuple<entt::entity, ResourceType, uint32_t>> removed;
};

constexpr auto k_Pile = static_cast<entt::entity>(7);
} // namespace

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(TornadoPileTake, TakesWhatItAsksUpToTheCountOfThePile)
{
	FakeStores stores;
	stores.count = 3000;
	stores.gives = 3000;
	EXPECT_EQ(tornado_pile::Take(stores, k_Pile, ResourceType::Food, 1000), 1000u);
	ASSERT_EQ(stores.asked.size(), 1u);
	EXPECT_EQ(stores.asked.front(), std::pair(k_Pile, ResourceType::Food));
	ASSERT_EQ(stores.removed.size(), 1u);
	EXPECT_EQ(stores.removed.front(), std::tuple(k_Pile, ResourceType::Food, 1000u));

	// A pile that holds less than the tornado asks gives all of it
	stores.count = 250;
	stores.gives = 250;
	EXPECT_EQ(tornado_pile::Take(stores, k_Pile, ResourceType::Wood, 1000), 250u);
	EXPECT_EQ(stores.removed.back(), std::tuple(k_Pile, ResourceType::Wood, 250u));
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(TornadoPileTake, ThePotHoldsTheCountAlsoWhenThePileGivesLess)
{
	FakeStores stores;
	stores.count = 800;
	stores.gives = 300;
	EXPECT_EQ(tornado_pile::Take(stores, k_Pile, ResourceType::Food, 1000), 800u);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(TornadoPileTake, AnEmptyPileGivesNothing)
{
	FakeStores stores;
	EXPECT_EQ(tornado_pile::Take(stores, k_Pile, ResourceType::Food, 1000), 0u);
	EXPECT_TRUE(stores.removed.empty());
}
