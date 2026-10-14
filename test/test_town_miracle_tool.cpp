/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "Debug/TownMiracleRules.h"

using namespace openblack;
using namespace openblack::town_miracle_tool;

namespace
{
constexpr std::array k_FireLevels {MagicType::FireballPowerUpOne, MagicType::FireballPowerUpTwo, MagicType::None};
} // namespace

TEST(TownMiracleTool, ALandFileLineGivesThePlainMiracleAfterAPowerUp)
{
	EXPECT_EQ(TypesToGive(MagicType::FireballPowerUpOne, MagicType::Fireball, true),
	          (std::vector {MagicType::FireballPowerUpOne, MagicType::Fireball}));
	// The script command gives only the one magic type
	EXPECT_EQ(TypesToGive(MagicType::FireballPowerUpOne, MagicType::Fireball, false),
	          (std::vector {MagicType::FireballPowerUpOne}));
	// A plain miracle is given once
	EXPECT_EQ(TypesToGive(MagicType::Heal, MagicType::Heal, true), (std::vector {MagicType::Heal}));
}

TEST(TownMiracleTool, ACreatureThatStealsAMiracleTakesThePlainOneFirstThenEachLevel)
{
	EXPECT_EQ(TypesToTake(MagicType::Fireball, MagicType::Fireball, k_FireLevels, true),
	          (std::vector {MagicType::Fireball, MagicType::FireballPowerUpOne, MagicType::FireballPowerUpTwo}));
	// The script command takes only the one magic type
	EXPECT_EQ(TypesToTake(MagicType::FireballPowerUpTwo, MagicType::Fireball, k_FireLevels, false),
	          (std::vector {MagicType::FireballPowerUpTwo}));
}

TEST(TownMiracleTool, ATownWithASiteHasNoReason)
{
	EXPECT_FALSE(WhyNoSite({.hasSite = true}).has_value());
}

TEST(TownMiracleTool, TheReasonsComeInTheOrderTheGameChecks)
{
	SiteChecks checks {.neutral = true, .landWithoutSites = true, .population = 0};
	EXPECT_EQ(WhyNoSite(checks), NoSite::LandWithoutSites);
	checks.anyLand = true;
	EXPECT_EQ(WhyNoSite(checks), NoSite::NoPeople);
	checks.population = 3;
	EXPECT_EQ(WhyNoSite(checks), NoSite::Neutral);
	checks.neutral = false;
	EXPECT_EQ(WhyNoSite(checks), NoSite::NoTemple);
	checks.hasTemple = true;
	EXPECT_EQ(WhyNoSite(checks), NoSite::TempleNotBuilt);
	checks.templeBuilt = true;
	EXPECT_EQ(WhyNoSite(checks), NoSite::NoPlace);
}

TEST(TownMiracleTool, EachReasonHasAText)
{
	for (const auto reason : {NoSite::Neutral, NoSite::NoTemple, NoSite::TempleNotBuilt, NoSite::LandWithoutSites,
	                          NoSite::NoPeople, NoSite::NoPlace})
	{
		EXPECT_FALSE(Describe(reason).empty());
	}
}
