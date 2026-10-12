/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "Magic/SpellIcons.h"

using namespace openblack;
using namespace openblack::magic::spell_icons;

namespace
{
constexpr auto k_None = SpellSeedType::None;

/// A made-up table: magic types 1 to 3 are a seed's plain miracle and two power-ups, 5 another seed, 7 a third
std::optional<SeedLevel> FakeSeedOf(MagicType type)
{
	switch (static_cast<int>(type))
	{
	case 1:
		return SeedLevel {.seed = SpellSeedType::Water, .powerUp = magic::k_BasePowerUpLevel};
	case 2:
		return SeedLevel {.seed = SpellSeedType::Water, .powerUp = 0};
	case 3:
		return SeedLevel {.seed = SpellSeedType::Water, .powerUp = 1};
	case 5:
		return SeedLevel {.seed = SpellSeedType::Heal, .powerUp = magic::k_BasePowerUpLevel};
	case 7:
		return SeedLevel {.seed = SpellSeedType::Teleport, .powerUp = magic::k_BasePowerUpLevel};
	default:
		return std::nullopt;
	}
}
} // namespace

TEST(SpellIcons, ATownHoldsEachMagicTypeOnceAndNeverNone)
{
	HeldMagic held {};
	EXPECT_TRUE(Hold(held, MagicType::Heal));
	EXPECT_FALSE(Hold(held, MagicType::Heal));
	EXPECT_FALSE(Hold(held, MagicType::None));
	EXPECT_TRUE(Release(held, MagicType::Heal));
	EXPECT_FALSE(Release(held, MagicType::Heal));
}

TEST(SpellIcons, ANewMiracleGoesOnTheFirstEmptyHandTheModelHasAPlaceFor)
{
	std::array<SpellSeedType, k_CentreHands> shown {SpellSeedType::Fire, k_None, SpellSeedType::Heal, k_None, k_None, k_None};
	EXPECT_EQ(FreeHand(shown, 6), 1u);
	// The first empty hand has no place on the model: nothing is shown, even if a later hand had one
	EXPECT_EQ(FreeHand(shown, 1), std::nullopt);
	shown.fill(SpellSeedType::Food);
	EXPECT_EQ(FreeHand(shown, 6), std::nullopt);
}

TEST(SpellIcons, TheHandsShowEachSeedOnceInTheOrderOfTheMagicTypesUpToSix)
{
	HeldMagic held {};
	held.at(7) = true;
	held.at(3) = true;
	held.at(5) = true;
	held.at(1) = true;
	// A power-up level brings its seed's hand as the plain miracle does, once
	const auto hands = HandsFor(held, FakeSeedOf, 6);
	EXPECT_EQ(hands, (std::vector {SpellSeedType::Water, SpellSeedType::Heal, SpellSeedType::Teleport}));
	EXPECT_EQ(HandsFor(held, FakeSeedOf, 2), (std::vector {SpellSeedType::Water, SpellSeedType::Heal}));
}

TEST(SpellIcons, TheSeedAboveAHandShowsTheHighestPowerUpLevel)
{
	EXPECT_EQ(ShownPowerUp({false, false, false}), magic::k_BasePowerUpLevel);
	EXPECT_EQ(ShownPowerUp({true, false, false}), 0);
	EXPECT_EQ(ShownPowerUp({true, false, true}), 2);
}

TEST(SpellIcons, ASiteKeepsAnIconForEveryMiracleItsTownsOfferOnce)
{
	const std::vector<std::vector<SpellSeedType>> towns {{SpellSeedType::Heal, SpellSeedType::Water},
	                                                     {SpellSeedType::Water, SpellSeedType::Fire}};
	EXPECT_EQ(SiteSeeds(towns), (std::vector {SpellSeedType::Heal, SpellSeedType::Water, SpellSeedType::Fire}));
}

TEST(SpellIcons, ATapStartsTheChargeWithTheHandReadyAndTheMiracleEnabled)
{
	const IconCharge idle {.required = 500.0f};
	EXPECT_EQ(Tap(idle, true, true, true), TapResult::StartedCharge);
	EXPECT_EQ(Tap(idle, false, true, true), TapResult::Nothing);
	EXPECT_EQ(Tap(idle, true, false, true), TapResult::Nothing);
	// Away from a site an icon only charges from what it has stored
	EXPECT_EQ(Tap(idle, true, true, false), TapResult::Nothing);
}

TEST(SpellIcons, TappingAgainWhileItChargesCallsItOffForTheHandCharging)
{
	const IconCharge charging {.charging = true, .chargedByThisHand = true, .store = 120.0f, .required = 500.0f};
	EXPECT_EQ(Tap(charging, true, true, true), TapResult::Cancelled);
	// Another player's hand can't, nor start it again
	auto other = charging;
	other.chargedByThisHand = false;
	EXPECT_EQ(Tap(other, true, true, true), TapResult::Nothing);
}

TEST(SpellIcons, AChargedMiracleGoesIntoAReadyHandUnlessASeedOfItIsOut)
{
	const IconCharge charged {.store = 500.0f, .required = 500.0f};
	EXPECT_TRUE(ReadyForHand(charged, true));
	EXPECT_EQ(Tap(charged, true, true, true), TapResult::PutInHand);
	// With the hand busy the charged miracle waits, and the tap does nothing
	EXPECT_EQ(Tap(charged, false, true, true), TapResult::Nothing);
	auto out = charged;
	out.seedOut = true;
	EXPECT_FALSE(ReadyForHand(out, true));
	EXPECT_EQ(Tap(out, true, true, true), TapResult::Nothing);
}

TEST(SpellIcons, TheClickRisesWithTheHandAndStopsAtTheSixth)
{
	EXPECT_EQ(ClickPitch(0), 100u);
	EXPECT_EQ(ClickPitch(1), 115u);
	EXPECT_EQ(ClickPitch(4), 155u);
	EXPECT_EQ(ClickPitch(5), 175u);
	EXPECT_EQ(ClickPitch(9), 175u);
}

TEST(SpellIcons, TheAnnouncerNamesTheChargedMiracle)
{
	EXPECT_EQ(FullyChargedVoice(SpellSeedType::Storm), 14);
	EXPECT_EQ(FullyChargedVoice(SpellSeedType::Fire), 2);
	EXPECT_EQ(FullyChargedVoice(SpellSeedType::LightningBolt), 8);
	EXPECT_EQ(FullyChargedVoice(SpellSeedType::Teleport), 15);
	EXPECT_EQ(FullyChargedVoice(SpellSeedType::BeamExplosion), 1);
	EXPECT_EQ(FullyChargedVoice(SpellSeedType::CreatureSpellHungry), 8);
}
