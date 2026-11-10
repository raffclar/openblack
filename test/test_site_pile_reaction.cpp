/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Components/Construction.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{

/// Keeps the reactions made, and does nothing else
class MadeReactions final: public systems::ReactionSystemInterface
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

class SitePileReaction: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<Registry>();
		_info = std::make_unique<InfoConstants>();
		_info->pot.at(static_cast<size_t>(PotInfo::MagicWood)).associatedReaction = Reaction::ReactToWood;
		Locator::infoConstants::emplace(*_info);
		Locator::reactionSystem::emplace<MadeReactions>();
	}
	void TearDown() override
	{
		Locator::reactionSystem::reset();
		Locator::infoConstants::reset();
		Locator::entitiesRegistry::reset();
	}

	std::unique_ptr<InfoConstants> _info;
};

} // namespace

// A site's pile with wood left in it calls the villagers when its site closes. A site's pile is a pile of the neutral
// player, so its reaction has the neutral player and impresses those who see it.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(SitePileReaction, AClosedSitesPileWithWoodCallsForTheNeutralPlayer)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto town = registry.Create();
	const auto building = registry.Create();
	registry.Assign<Transform>(building, glm::vec3(10.0f, 0.0f, 10.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const auto pile = registry.Create();
	registry.Assign<Transform>(pile, glm::vec3(12.0f, 0.0f, 10.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Pot>(pile, Pot {.amount = 5, .maxAmount = 100, .type = PotInfo::MagicWood});
	registry.Assign<SitePile>(pile);
	BuildingSite site {.town = town};
	site.piles.front() = pile;
	registry.Assign<BuildingSites>(building, BuildingSites {.sites = {site}});

	construction::CloseSite(building);

	const auto& made = static_cast<MadeReactions&>(Locator::reactionSystem::value()).made;
	ASSERT_EQ(made.size(), 1u);
	EXPECT_EQ(made.front().initiator, pile);
	EXPECT_EQ(made.front().type, Reaction::ReactToWood);
	EXPECT_EQ(made.front().player, PlayerNames::NEUTRAL);
	EXPECT_FALSE(made.front().playerless);
	EXPECT_EQ(made.front().position, glm::vec3(12.0f, 0.0f, 10.0f));
}
