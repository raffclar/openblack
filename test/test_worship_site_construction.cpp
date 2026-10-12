/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <numbers>

#include <gtest/gtest.h>

#include "ECS/WorshipSiteConstruction.h"

using namespace openblack;
namespace wsc = openblack::ecs::worship_site_construction;

TEST(WorshipSiteConstruction, ThePileStandsAtItsPointOfTheSiteTurnedAsTheSiteIs)
{
	// Facing the first way the point is the model's own, about the site
	const auto ahead = wsc::PilePlace({100.0f, 200.0f}, 0.0f);
	EXPECT_NEAR(ahead.x, 109.0f, 1e-4f);
	EXPECT_NEAR(ahead.y, 150.0f, 1e-4f);
	// A quarter turn takes the model's x to its z, and its z to minus its x
	const auto turned = wsc::PilePlace({100.0f, 200.0f}, std::numbers::pi_v<float> / 2.0f);
	EXPECT_NEAR(turned.x, 150.0f, 1e-4f);
	EXPECT_NEAR(turned.y, 209.0f, 1e-4f);
}

TEST(WorshipSiteConstruction, GoingUpWoodGoesOntoTheSitesPileAndFoodStillIntoThePot)
{
	using enum wsc::Destination;
	EXPECT_EQ(wsc::WhereGiven(ResourceType::Wood, true), SitePile);
	EXPECT_EQ(wsc::WhereGiven(ResourceType::Food, true), FoodPot);
	// Anything given as any resource goes to the building site, which takes only wood
	EXPECT_EQ(wsc::WhereGiven(ResourceType::Any, true), Nowhere);
	// Built, the site takes only food
	EXPECT_EQ(wsc::WhereGiven(ResourceType::Wood, false), Nowhere);
	EXPECT_EQ(wsc::WhereGiven(ResourceType::Food, false), FoodPot);
	EXPECT_EQ(wsc::WhereGiven(ResourceType::Any, false), Nowhere);
}

TEST(WorshipSiteConstruction, ASiteStoresWoodOnlyWhileItGoesUp)
{
	EXPECT_TRUE(wsc::Stores(ResourceType::Wood, true));
	EXPECT_FALSE(wsc::Stores(ResourceType::Wood, false));
	EXPECT_TRUE(wsc::Stores(ResourceType::Food, false));
	EXPECT_TRUE(wsc::Stores(ResourceType::Any, false));
}
