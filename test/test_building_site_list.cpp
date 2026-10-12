/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "ECS/BuildingSiteList.h"
#include "ECS/Components/Construction.h"

using namespace openblack::ecs;
using components::BuildingSites;

namespace
{
constexpr auto k_FirstTown = entt::entity {3};
constexpr auto k_SecondTown = entt::entity {7};
constexpr auto k_ThirdTown = entt::entity {9};
} // namespace

TEST(BuildingSiteList, EachTownAskedHasItsOwnSiteWithItsOwnBuildersAndDesire)
{
	BuildingSites sites;
	auto& first = building_site_list::Open(sites, k_FirstTown);
	first.desire = 0.5f;
	first.workers.push_back(entt::entity {20});
	first.workerCount = 1;
	auto& second = building_site_list::Open(sites, k_SecondTown);
	second.desire = 0.25f;

	ASSERT_EQ(sites.sites.size(), 2u);
	const auto* firstFound = building_site_list::Find(sites, k_FirstTown);
	const auto* secondFound = building_site_list::Find(sites, k_SecondTown);
	ASSERT_NE(firstFound, nullptr);
	ASSERT_NE(secondFound, nullptr);
	EXPECT_FLOAT_EQ(firstFound->desire, 0.5f);
	EXPECT_EQ(firstFound->workerCount, 1);
	EXPECT_FLOAT_EQ(secondFound->desire, 0.25f);
	EXPECT_EQ(secondFound->workerCount, 0);
	EXPECT_TRUE(secondFound->workers.empty());
	// A town not asked has none
	EXPECT_EQ(building_site_list::Find(sites, k_ThirdTown), nullptr);
	EXPECT_EQ(building_site_list::Find(sites, entt::null), nullptr);
}

TEST(BuildingSiteList, BroughtWoodGoesOntoTheNewestSite)
{
	BuildingSites sites;
	EXPECT_EQ(building_site_list::TakingBroughtWood(sites), nullptr);
	building_site_list::Open(sites, k_FirstTown);
	ASSERT_NE(building_site_list::TakingBroughtWood(sites), nullptr);
	EXPECT_EQ(building_site_list::TakingBroughtWood(sites)->town, k_FirstTown);
	building_site_list::Open(sites, k_SecondTown);
	EXPECT_EQ(building_site_list::TakingBroughtWood(sites)->town, k_SecondTown);
	// Only ever one site takes it
	int taking = 0;
	for (const auto& site : sites.sites)
	{
		taking += site.takesBroughtWood ? 1 : 0;
	}
	EXPECT_EQ(taking, 1);
	// The newest goes first
	EXPECT_EQ(sites.sites.front().town, k_SecondTown);
}

TEST(BuildingSiteList, ClosingAnySiteLeavesNoneTakingBroughtWoodUntilAnotherOpens)
{
	BuildingSites sites;
	building_site_list::Open(sites, k_FirstTown);
	building_site_list::Open(sites, k_SecondTown);
	building_site_list::Open(sites, k_ThirdTown);

	// The oldest site closes, not the one taking brought wood, yet none takes it after
	const auto closed = building_site_list::Close(sites, k_FirstTown);
	ASSERT_TRUE(closed.has_value());
	EXPECT_EQ(closed->town, k_FirstTown);
	EXPECT_EQ(sites.sites.size(), 2u);
	EXPECT_EQ(building_site_list::TakingBroughtWood(sites), nullptr);
	EXPECT_NE(building_site_list::Find(sites, k_SecondTown), nullptr);
	EXPECT_NE(building_site_list::Find(sites, k_ThirdTown), nullptr);

	// A town without a site there closes nothing
	EXPECT_FALSE(building_site_list::Close(sites, k_FirstTown).has_value());
	EXPECT_EQ(sites.sites.size(), 2u);

	// A new site takes it again
	building_site_list::Open(sites, k_FirstTown);
	ASSERT_NE(building_site_list::TakingBroughtWood(sites), nullptr);
	EXPECT_EQ(building_site_list::TakingBroughtWood(sites)->town, k_FirstTown);
}

TEST(BuildingSiteList, AFinishedBuildingClosesTheSiteTakingBroughtWoodFirstThenTheNewest)
{
	BuildingSites sites;
	building_site_list::Open(sites, k_FirstTown);
	building_site_list::Open(sites, k_SecondTown);
	building_site_list::Open(sites, k_ThirdTown);
	EXPECT_EQ(building_site_list::ClosingOrder(sites), (std::vector {k_ThirdTown, k_SecondTown, k_FirstTown}));

	// With none taking brought wood, simply the newest first
	building_site_list::Close(sites, k_ThirdTown);
	EXPECT_EQ(building_site_list::ClosingOrder(sites), (std::vector {k_SecondTown, k_FirstTown}));

	// The site taking it goes first even when an older one is listed before it
	building_site_list::Open(sites, k_ThirdTown);
	sites.sites.front().takesBroughtWood = false;
	sites.sites.back().takesBroughtWood = true;
	EXPECT_EQ(building_site_list::ClosingOrder(sites), (std::vector {k_FirstTown, k_ThirdTown, k_SecondTown}));
}
