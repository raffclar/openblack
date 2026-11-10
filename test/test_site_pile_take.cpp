/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <memory>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Construction.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/ResourceStoreSystem.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
constexpr uint32_t k_TownId = 4;

/// A town with a site at a temple and a site at an abode, each keeping wood in piles of magic wood
class SitePileTakeTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_st>("game");
		}
		Locator::entitiesRegistry::emplace<Registry>();
		info = std::make_unique<InfoConstants>();
		auto& wood = info->pot.at(static_cast<size_t>(PotInfo::MagicWood));
		wood.resourceType = ResourceType::Wood;
		wood.potType = PotType::PileWood;
		wood.maxAmountInPot = 1000;
		Locator::infoConstants::emplace(*info);

		auto& registry = Locator::entitiesRegistry::value();
		town = registry.Create();
		registry.Assign<Town>(town, Town {.id = k_TownId});
		registry.Assign<TownResourceTally>(town).woodAtSites = 500.0f;

		temple = registry.Create();
		registry.Assign<Temple>(temple, Temple {.owner = PlayerNames::PLAYER_ONE, .town = static_cast<int32_t>(k_TownId)});
		auto& templeSites = registry.Assign<BuildingSites>(temple);
		templeSites.sites.push_back(BuildingSite {.town = town, .takesBroughtWood = true});
		templePiles = {Pile(glm::vec3(0.0f, 0.0f, 0.0f), 100), Pile(glm::vec3(30.0f, 0.0f, 0.0f), 100)};
		templeSites.sites.front().piles.at(0) = templePiles.at(0);
		templeSites.sites.front().piles.at(1) = templePiles.at(1);

		abode = registry.Create();
		registry.Assign<Abode>(abode, Abode {.type = AbodeNumber {}, .townId = k_TownId, .foodAmount = 0, .woodAmount = 0});
		auto& abodeSites = registry.Assign<BuildingSites>(abode);
		abodeSites.sites.push_back(BuildingSite {.town = town, .takesBroughtWood = true});
		abodePile = Pile(glm::vec3(80.0f, 0.0f, 0.0f), 100);
		abodeSites.sites.front().piles.at(0) = abodePile;
	}
	void TearDown() override
	{
		Locator::infoConstants::reset();
		Locator::entitiesRegistry::reset();
	}

	[[nodiscard]] static entt::entity Pile(glm::vec3 position, uint32_t amount)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto pile = registry.Create();
		registry.Assign<Transform>(pile,
		                           Transform {.position = position, .rotation = glm::mat3(1.0f), .scale = glm::vec3(1.0f)});
		registry.Assign<Pot>(pile, Pot {.amount = amount, .maxAmount = 1000, .type = PotInfo::MagicWood});
		return pile;
	}
	[[nodiscard]] static bool Exists(entt::entity pile) { return Locator::entitiesRegistry::value().Valid(pile); }
	[[nodiscard]] static uint32_t Holds(entt::entity pile)
	{
		return Locator::entitiesRegistry::value().Get<const Pot>(pile).amount;
	}
	[[nodiscard]] float WoodAtSites() const
	{
		return Locator::entitiesRegistry::value().Get<const TownResourceTally>(town).woodAtSites;
	}

	std::unique_ptr<InfoConstants> info;
	ResourceStoreSystem stores;
	entt::entity town {entt::null};
	entt::entity temple {entt::null};
	entt::entity abode {entt::null};
	std::array<entt::entity, 2> templePiles {entt::null, entt::null};
	entt::entity abodePile {entt::null};
};
} // namespace

// The hand that empties a pile of a temple's site leaves the pile there, empty, and its town counts that much less wood
// at its sites
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(SitePileTakeTest, AHandEmptiesATempleSitePileThatStaysAndTheTownCountsLess)
{
	const auto pile = templePiles.at(1);
	EXPECT_EQ(stores.TakeFromPile(pile, ResourceType::Wood, 150, PlayerNames::PLAYER_ONE, glm::vec3(29.0f, 0.0f, 0.0f)), 100u);
	ASSERT_TRUE(Exists(pile));
	EXPECT_EQ(Holds(pile), 0u);
	EXPECT_EQ(Holds(templePiles.at(0)), 100u);
	EXPECT_FLOAT_EQ(WoodAtSites(), 400.0f);
}

// The hand takes from the pile of a temple's site nearest the point of the hand, which can be another pile than the
// pile that it scoops
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(SitePileTakeTest, AHandTakesFromTheTempleSitePileNearestItsPoint)
{
	EXPECT_EQ(
	    stores.TakeFromPile(templePiles.at(0), ResourceType::Wood, 40, PlayerNames::PLAYER_ONE, glm::vec3(25.0f, 0.0f, 0.0f)),
	    40u);
	EXPECT_EQ(Holds(templePiles.at(0)), 100u);
	EXPECT_EQ(Holds(templePiles.at(1)), 60u);
	EXPECT_FLOAT_EQ(WoodAtSites(), 460.0f);
}

// A hand takes only wood from a temple's site
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(SitePileTakeTest, AHandTakesNoFoodFromATempleSite)
{
	EXPECT_EQ(
	    stores.TakeFromPile(templePiles.at(0), ResourceType::Food, 40, PlayerNames::PLAYER_ONE, glm::vec3(0.0f, 0.0f, 0.0f)),
	    0u);
	EXPECT_EQ(Holds(templePiles.at(0)), 100u);
	EXPECT_FLOAT_EQ(WoodAtSites(), 500.0f);
}

// With no hand, a temple's site gives from its piles in turn and its town counts the same wood at its sites
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(SitePileTakeTest, WithNoHandATempleSiteGivesFromItsPilesInTurn)
{
	EXPECT_EQ(stores.RemoveResource(templePiles.at(1), ResourceType::Wood, 150), 150u);
	EXPECT_EQ(Holds(templePiles.at(0)), 0u);
	EXPECT_EQ(Holds(templePiles.at(1)), 50u);
	EXPECT_FLOAT_EQ(WoodAtSites(), 500.0f);
}

// The hand that empties the pile of any other site leaves it there, empty, and its town counts that much less wood at
// its sites
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(SitePileTakeTest, AHandEmptiesTheSitePileOfAnAbodeThatStays)
{
	EXPECT_EQ(stores.TakeFromPile(abodePile, ResourceType::Wood, 300, PlayerNames::PLAYER_ONE, glm::vec3(0.0f)), 100u);
	ASSERT_TRUE(Exists(abodePile));
	EXPECT_EQ(Holds(abodePile), 0u);
	EXPECT_FLOAT_EQ(WoodAtSites(), 400.0f);
}

// A pile on its own still goes when the hand empties it
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(SitePileTakeTest, ALonePileGoesWhenTheHandEmptiesIt)
{
	const auto pile = Pile(glm::vec3(200.0f, 0.0f, 0.0f), 30);
	EXPECT_EQ(stores.TakeFromPile(pile, ResourceType::Wood, 50, PlayerNames::PLAYER_ONE, glm::vec3(200.0f, 0.0f, 0.0f)), 30u);
	EXPECT_FALSE(Exists(pile));
}
