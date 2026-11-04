/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/BuildingSiteList.h"
#include "ECS/Components/Construction.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/ScriptDestination.h"
#include "ECS/VanillaWorldImport.h"
#include "ScriptHeaders/ScriptPlayers.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace world_import = openblack::ecs::vanilla_world_import;

namespace
{

/// A town for the site lists: only its number counts
entt::entity TownNumbered(uint32_t number)
{
	return static_cast<entt::entity>(number);
}

/// A town of the file that lists these sites, the newest first
world_import::ImportedTown TownListing(std::vector<uint32_t> sites)
{
	world_import::ImportedTown town;
	town.sites = std::move(sites);
	return town;
}

} // namespace

TEST(ScriptNativesRows8, ScriptPlayerZeroIsTheNeutralPlayer)
{
	using script::script_players::NameOf;
	EXPECT_EQ(NameOf(0), PlayerNames::NEUTRAL);
	EXPECT_EQ(NameOf(1), PlayerNames::PLAYER_ONE);
	EXPECT_EQ(NameOf(2), PlayerNames::PLAYER_TWO);
	EXPECT_EQ(NameOf(7), PlayerNames::PLAYER_SEVEN);
	// The number 8 names the neutral player too
	EXPECT_EQ(NameOf(8), PlayerNames::NEUTRAL);
}

TEST(ScriptNativesRows8, AVillagerGoalHasItsHeightAboveTheLand)
{
	ecs::Registry registry;
	const auto villager = registry.Create();
	registry.Assign<Villager>(villager);
	auto& walk = registry.Assign<WallHug>(villager);
	walk.goalPlace = {500, 600};
	walk.goalHeight = 2.5f;

	EXPECT_EQ(ecs::script_destination::Of(registry, villager), (map_coords::MapCoords {500, 600, 2.5f}));
}

TEST(ScriptNativesRows8, TheBuildingTownOpensASiteThatTwoTownsList)
{
	// Town 0 lists sites 3 and 1, town 1 lists sites 5 and 3. Site 3 is at a building of town 1.
	const std::vector towns {TownListing({3, 1}), TownListing({5, 3})};
	const auto ownerOf = [](uint32_t site) -> std::optional<std::size_t> {
		return site == 3 ? std::optional<std::size_t>(1) : std::optional<std::size_t>(site == 5 ? 1 : 0);
	};

	const auto listings = world_import::PlanSiteListings(towns, ownerOf);

	ASSERT_EQ(listings.size(), 4u);
	// Each town's list comes with its oldest site first
	EXPECT_EQ(listings[0].town, 0u);
	EXPECT_EQ(listings[0].site, 1u);
	EXPECT_EQ(listings[0].opener, 0u);
	EXPECT_EQ(listings[1].town, 0u);
	EXPECT_EQ(listings[1].site, 3u);
	EXPECT_EQ(listings[1].opener, 1u);
	EXPECT_EQ(listings[2].town, 1u);
	EXPECT_EQ(listings[2].site, 3u);
	EXPECT_EQ(listings[2].opener, 1u);
	EXPECT_EQ(listings[3].town, 1u);
	EXPECT_EQ(listings[3].site, 5u);
	EXPECT_EQ(listings[3].opener, 1u);
}

TEST(ScriptNativesRows8, TheFirstTownOpensASiteThatTheBuildingTownDoesNotList)
{
	// Site 4 is at a building of town 2, which lists no site. Site 6 has no building town.
	const std::vector towns {TownListing({4}), TownListing({6, 4}), TownListing({})};
	const auto ownerOf = [](uint32_t site) -> std::optional<std::size_t> {
		return site == 4 ? std::optional<std::size_t>(2) : std::nullopt;
	};

	const auto listings = world_import::PlanSiteListings(towns, ownerOf);

	ASSERT_EQ(listings.size(), 3u);
	EXPECT_EQ(listings[0].opener, 0u);
	EXPECT_EQ(listings[1].site, 4u);
	EXPECT_EQ(listings[1].opener, 0u);
	EXPECT_EQ(listings[2].site, 6u);
	EXPECT_EQ(listings[2].opener, 1u);
}

TEST(ScriptNativesRows8, ATownSetsThePlaceOfASiteInItsList)
{
	BuildingSite site {.town = TownNumbered(1), .number = 4};
	ASSERT_TRUE(ecs::building_site_list::Share(site, TownNumbered(2), 9));

	EXPECT_TRUE(ecs::building_site_list::SetNumberFor(site, TownNumbered(1), 2));
	EXPECT_TRUE(ecs::building_site_list::SetNumberFor(site, TownNumbered(2), 5));
	EXPECT_FALSE(ecs::building_site_list::SetNumberFor(site, TownNumbered(3), 7));

	EXPECT_EQ(site.number, 2u);
	EXPECT_EQ(ecs::building_site_list::NumberFor(site, TownNumbered(1)), 2u);
	EXPECT_EQ(ecs::building_site_list::NumberFor(site, TownNumbered(2)), 5u);
	EXPECT_EQ(ecs::building_site_list::NumberFor(site, TownNumbered(3)), 0u);
}
