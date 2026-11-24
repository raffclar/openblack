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
#include <optional>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Construction.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"

#define LOCATOR_IMPLEMENTATIONS 1
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
		_info = std::make_unique<InfoConstants>();
		auto& wood = _info->pot.at(static_cast<size_t>(PotInfo::MagicWood));
		wood.resourceType = ResourceType::Wood;
		wood.potType = PotType::PileWood;
		wood.maxAmountInPot = 1000;
		Locator::infoConstants::emplace(*_info);

		auto& registry = Locator::entitiesRegistry::value();
		_town = registry.Create();
		registry.Assign<Town>(_town, Town {.id = k_TownId});
		registry.Assign<TownResourceTally>(_town).woodAtSites = 500.0f;

		_temple = registry.Create();
		registry.Assign<Temple>(_temple, Temple {.owner = PlayerNames::PLAYER_ONE, .town = static_cast<int32_t>(k_TownId)});
		auto& templeSites = registry.Assign<BuildingSites>(_temple);
		templeSites.sites.push_back(BuildingSite {.town = _town, .takesBroughtWood = true});
		_templePiles = {Pile(glm::vec3(0.0f, 0.0f, 0.0f), 100), Pile(glm::vec3(30.0f, 0.0f, 0.0f), 100)};
		templeSites.sites.front().piles.at(0) = _templePiles.at(0);
		templeSites.sites.front().piles.at(1) = _templePiles.at(1);

		_abode = registry.Create();
		registry.Assign<Abode>(_abode, Abode {.type = AbodeNumber {}, .townId = k_TownId, .foodAmount = 0, .woodAmount = 0});
		auto& abodeSites = registry.Assign<BuildingSites>(_abode);
		abodeSites.sites.push_back(BuildingSite {.town = _town, .takesBroughtWood = true});
		_abodePile = Pile(glm::vec3(80.0f, 0.0f, 0.0f), 100);
		abodeSites.sites.front().piles.at(0) = _abodePile;
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
		return Locator::entitiesRegistry::value().Get<const TownResourceTally>(_town).woodAtSites;
	}

	std::unique_ptr<InfoConstants> _info;
	ResourceStoreSystem _stores;
	entt::entity _town {entt::null};
	entt::entity _temple {entt::null};
	entt::entity _abode {entt::null};
	std::array<entt::entity, 2> _templePiles {entt::null, entt::null};
	entt::entity _abodePile {entt::null};
};
} // namespace

// The hand that empties a pile of a temple's site leaves the pile there, empty, and its town counts that much less wood
// at its sites
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(SitePileTakeTest, AHandEmptiesATempleSitePileThatStaysAndTheTownCountsLess)
{
	const auto pile = _templePiles.at(1);
	EXPECT_EQ(_stores.TakeFromPile(pile, ResourceType::Wood, 150, PlayerNames::PLAYER_ONE, glm::vec3(29.0f, 0.0f, 0.0f)), 100u);
	ASSERT_TRUE(Exists(pile));
	EXPECT_EQ(Holds(pile), 0u);
	EXPECT_EQ(Holds(_templePiles.at(0)), 100u);
	EXPECT_FLOAT_EQ(WoodAtSites(), 400.0f);
}

// The hand takes from the pile of a temple's site nearest the point of the hand, which can be another pile than the
// pile that it scoops
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(SitePileTakeTest, AHandTakesFromTheTempleSitePileNearestItsPoint)
{
	EXPECT_EQ(
	    _stores.TakeFromPile(_templePiles.at(0), ResourceType::Wood, 40, PlayerNames::PLAYER_ONE, glm::vec3(25.0f, 0.0f, 0.0f)),
	    40u);
	EXPECT_EQ(Holds(_templePiles.at(0)), 100u);
	EXPECT_EQ(Holds(_templePiles.at(1)), 60u);
	EXPECT_FLOAT_EQ(WoodAtSites(), 460.0f);
}

// A hand takes only wood from a temple's site
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(SitePileTakeTest, AHandTakesNoFoodFromATempleSite)
{
	EXPECT_EQ(
	    _stores.TakeFromPile(_templePiles.at(0), ResourceType::Food, 40, PlayerNames::PLAYER_ONE, glm::vec3(0.0f, 0.0f, 0.0f)),
	    0u);
	EXPECT_EQ(Holds(_templePiles.at(0)), 100u);
	EXPECT_FLOAT_EQ(WoodAtSites(), 500.0f);
}

// With no hand, a temple's site gives from its piles in turn and its town counts the same wood at its sites
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(SitePileTakeTest, WithNoHandATempleSiteGivesFromItsPilesInTurn)
{
	EXPECT_EQ(_stores.RemoveResource(_templePiles.at(1), ResourceType::Wood, 150), 150u);
	EXPECT_EQ(Holds(_templePiles.at(0)), 0u);
	EXPECT_EQ(Holds(_templePiles.at(1)), 50u);
	EXPECT_FLOAT_EQ(WoodAtSites(), 500.0f);
}

// The hand that empties the pile of any other site leaves it there, empty, and its town counts that much less wood at
// its sites
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(SitePileTakeTest, AHandEmptiesTheSitePileOfAnAbodeThatStays)
{
	EXPECT_EQ(_stores.TakeFromPile(_abodePile, ResourceType::Wood, 300, PlayerNames::PLAYER_ONE, glm::vec3(0.0f)), 100u);
	ASSERT_TRUE(Exists(_abodePile));
	EXPECT_EQ(Holds(_abodePile), 0u);
	EXPECT_FLOAT_EQ(WoodAtSites(), 400.0f);
}

// A pile on its own still goes when the hand empties it
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(SitePileTakeTest, ALonePileGoesWhenTheHandEmptiesIt)
{
	const auto pile = Pile(glm::vec3(200.0f, 0.0f, 0.0f), 30);
	EXPECT_EQ(_stores.TakeFromPile(pile, ResourceType::Wood, 50, PlayerNames::PLAYER_ONE, glm::vec3(200.0f, 0.0f, 0.0f)), 30u);
	EXPECT_FALSE(Exists(pile));
}

// An abode with a site keeps the food that it gets. Only the wood goes to the pile of its site.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(SitePileTakeTest, AnAbodeWithASiteKeepsItsFoodAndGivesItsWoodToTheSite)
{
	EXPECT_EQ(_stores.AddResource(_abode, ResourceType::Food, 30, std::nullopt), 30u);
	EXPECT_EQ(_stores.AddResource(_abode, ResourceType::Wood, 20, std::nullopt), 20u);
	const auto& abode = Locator::entitiesRegistry::value().Get<const Abode>(_abode);
	EXPECT_EQ(abode.foodAmount, 30u);
	EXPECT_EQ(abode.woodAmount, 0u);
	EXPECT_EQ(Holds(_abodePile), 120u);
	EXPECT_EQ(_stores.GetResource(_abode, ResourceType::Food), 30u);
}

// An abode with a site gives food from its own count. It gives wood from the pile of its site.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(SitePileTakeTest, AnAbodeWithASiteGivesItsOwnFoodAndTheWoodOfItsSite)
{
	Locator::entitiesRegistry::value().Get<Abode>(_abode).foodAmount = 50;
	EXPECT_EQ(_stores.RemoveResource(_abode, ResourceType::Food, 20), 20u);
	EXPECT_EQ(_stores.RemoveResource(_abode, ResourceType::Wood, 40), 40u);
	EXPECT_EQ(Locator::entitiesRegistry::value().Get<const Abode>(_abode).foodAmount, 30u);
	EXPECT_EQ(Holds(_abodePile), 60u);
}

// A storage pit with a site gives food and wood from its own piles, not from the pile of its site
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(SitePileTakeTest, AStoragePitWithASiteGivesFromItsOwnPiles)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto pit = registry.Create();
	registry.Assign<Abode>(pit, Abode {.type = AbodeNumber {}, .townId = k_TownId, .foodAmount = 70, .woodAmount = 90});
	auto& sites = registry.Assign<BuildingSites>(pit);
	sites.sites.push_back(BuildingSite {.town = _town, .takesBroughtWood = true});
	const auto sitePile = Pile(glm::vec3(120.0f, 0.0f, 0.0f), 100);
	sites.sites.front().piles.at(0) = sitePile;
	auto& store = registry.Assign<StoragePit>(pit);
	store.woodPiles.fill(entt::null);
	store.woodPiles.at(0) = Pile(glm::vec3(130.0f, 0.0f, 0.0f), 90);
	store.foodPile = Pile(glm::vec3(140.0f, 0.0f, 0.0f), 70);

	EXPECT_EQ(_stores.RemoveResource(pit, ResourceType::Wood, 40), 40u);
	EXPECT_EQ(_stores.RemoveResource(pit, ResourceType::Food, 30), 30u);
	EXPECT_EQ(Holds(sitePile), 100u);
	EXPECT_EQ(Holds(store.woodPiles.at(0)), 50u);
	EXPECT_EQ(Holds(store.foodPile), 40u);
	EXPECT_EQ(registry.Get<const Abode>(pit).woodAmount, 50u);
	EXPECT_EQ(registry.Get<const Abode>(pit).foodAmount, 40u);
}

// A scaffold holds wood by its value. It gives wood to its site and takes nothing with no site.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(SitePileTakeTest, AScaffoldHoldsWoodByItsValueAndGivesWoodToItsSite)
{
	_info->scaffold.woodValue = 100;
	Locator::infoConstants::reset();
	Locator::infoConstants::emplace(*(_info.get()));
	auto& registry = Locator::entitiesRegistry::value();
	const auto scaffold = registry.Create();
	registry.Assign<Transform>(scaffold,
	                           Transform {.position = glm::vec3(0.0f), .rotation = glm::mat3(1.0f), .scale = glm::vec3(1.0f)});
	registry.Assign<Scaffold>(scaffold);
	// A scaffold of scale 1 has the value 3
	EXPECT_EQ(_stores.GetResource(scaffold, ResourceType::Wood), 300u);
	EXPECT_EQ(_stores.GetResource(scaffold, ResourceType::Food), 0u);
	EXPECT_EQ(_stores.AddResource(scaffold, ResourceType::Wood, 10, std::nullopt), 0u);

	registry.Get<Scaffold>(scaffold).site = _abode;
	EXPECT_EQ(_stores.AddResource(scaffold, ResourceType::Food, 10, std::nullopt), 0u);
	EXPECT_EQ(_stores.AddResource(scaffold, ResourceType::Wood, 10, std::nullopt), 10u);
	EXPECT_EQ(Holds(_abodePile), 110u);
	EXPECT_EQ(registry.Get<const Abode>(_abode).foodAmount, 0u);
}

// An abode's site takes and gives only wood. A script's "any" resource gets nothing from the site or into it.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(SitePileTakeTest, AnAbodeSiteTakesAndGivesNoAnyResource)
{
	EXPECT_EQ(_stores.AddResource(_abode, ResourceType::Any, 30, std::nullopt), 0u);
	EXPECT_EQ(_stores.RemoveResource(_abode, ResourceType::Any, 30), 0u);
	EXPECT_EQ(_stores.RemoveResource(_abodePile, ResourceType::Any, 30), 0u);
	EXPECT_EQ(Holds(_abodePile), 100u);
	const auto& abode = Locator::entitiesRegistry::value().Get<const Abode>(_abode);
	EXPECT_EQ(abode.foodAmount, 0u);
	EXPECT_EQ(abode.woodAmount, 0u);
	EXPECT_FLOAT_EQ(WoodAtSites(), 500.0f);
}

// A temple's site puts all resources that come to a point on its nearest pile, and the town counts them as wood at its
// sites. It takes nothing that comes to no point.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(SitePileTakeTest, ATempleSiteTakesAllResourcesThatComeToAPoint)
{
	const auto atFirstPile = map_coords::FromMetres({1.0f, 0.0f});
	EXPECT_EQ(_stores.AddToTownsSite(_temple, _town, ResourceType::Food, 30, atFirstPile), 30u);
	EXPECT_EQ(_stores.AddToTownsSite(_temple, _town, ResourceType::Any, 20, atFirstPile), 20u);
	EXPECT_EQ(Holds(_templePiles.at(0)), 150u);
	EXPECT_EQ(Holds(_templePiles.at(1)), 100u);
	EXPECT_FLOAT_EQ(WoodAtSites(), 550.0f);

	EXPECT_EQ(_stores.AddResource(_temple, ResourceType::Wood, 20, std::nullopt), 0u);
	EXPECT_EQ(_stores.AddResource(_temple, ResourceType::Any, 20, std::nullopt), 0u);
	EXPECT_EQ(Holds(_templePiles.at(0)), 150u);
}

// With no hand, a temple's site gives from its piles in turn, whatever resource is asked for
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(SitePileTakeTest, WithNoHandATempleSiteGivesAllResourcesFromItsPiles)
{
	EXPECT_EQ(_stores.RemoveResource(_templePiles.at(1), ResourceType::Food, 30), 30u);
	EXPECT_EQ(_stores.RemoveResource(_templePiles.at(1), ResourceType::Any, 120), 120u);
	EXPECT_EQ(Holds(_templePiles.at(0)), 0u);
	EXPECT_EQ(Holds(_templePiles.at(1)), 50u);
	EXPECT_FLOAT_EQ(WoodAtSites(), 500.0f);
}

// A building with no site has no count for "none" or "any". It answers 0 and its counts do not change.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(SitePileTakeTest, ABuildingWithNoSiteHasNothingForNoneOrAny)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto abode = registry.Create();
	registry.Assign<Abode>(abode, Abode {.type = AbodeNumber {}, .townId = k_TownId, .foodAmount = 70, .woodAmount = 90});
	for (const auto type : {ResourceType::None, ResourceType::Any})
	{
		EXPECT_EQ(_stores.GetResource(abode, type), 0u);
		EXPECT_EQ(_stores.AddResource(abode, type, 30, std::nullopt), 0u);
		EXPECT_EQ(_stores.RemoveResource(abode, type, 30), 0u);
	}
	EXPECT_EQ(registry.Get<const Abode>(abode).foodAmount, 70u);
	EXPECT_EQ(registry.Get<const Abode>(abode).woodAmount, 90u);
}
