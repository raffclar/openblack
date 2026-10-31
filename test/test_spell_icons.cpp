/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <array>
#include <limits>
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

TEST(SpellIcons, ASiteIconShowsTheHighestPowerUpLevelItsPlayerMayCast)
{
	// The water's levels: the first power-up, then none, as a seed with one power-up gesture has
	const std::array levels {MagicType::WaterPowerUpOne, MagicType::None, MagicType::None};
	const auto nothing = [](MagicType /*type*/) { return false; };
	EXPECT_EQ(SiteShownPowerUp(levels, nothing), magic::k_BasePowerUpLevel);
	const auto waterUp = [](MagicType type) { return type == MagicType::WaterPowerUpOne; };
	EXPECT_EQ(SiteShownPowerUp(levels, waterUp), 0);
	// An empty level asks after magic type none, as the game does
	const auto noneToo = [](MagicType type) { return type == MagicType::None; };
	EXPECT_EQ(SiteShownPowerUp(levels, noneToo), 2);
}

TEST(SpellIcons, ATownLetsGoOfAMagicTypeOnlyWhenItHoldsIt)
{
	HeldMagic held {};
	EXPECT_FALSE(Release(held, MagicType::Heal));
	EXPECT_TRUE(Hold(held, MagicType::Heal));
	EXPECT_TRUE(Release(held, MagicType::Heal));
	EXPECT_FALSE(held.at(static_cast<size_t>(MagicType::Heal)));
	EXPECT_FALSE(Release(held, MagicType::Heal));
}

TEST(SpellIcons, ASiteIconGoesOnlyWhenNoOtherTownOffersItsMiracle)
{
	// After a town's heal hand goes, the other town of the site still offers the water only
	const std::vector<std::vector<SpellSeedType>> after {{SpellSeedType::Water}, {SpellSeedType::Water}};
	const auto kept = SiteSeeds(after);
	EXPECT_EQ(std::ranges::find(kept, SpellSeedType::Heal), kept.end());
	EXPECT_NE(std::ranges::find(kept, SpellSeedType::Water), kept.end());
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

TEST(SpellIcons, AChargedMiracleGoesIntoAReadyHandUnlessAHandHoldsASeedOfIt)
{
	const IconCharge charged {.store = 500.0f, .required = 500.0f};
	EXPECT_TRUE(ReadyForHand(charged, true));
	EXPECT_EQ(Tap(charged, true, true, true), TapResult::PutInHand);
	// With the hand busy the charged miracle waits, and the tap does nothing
	EXPECT_EQ(Tap(charged, false, true, true), TapResult::Nothing);
	auto held = charged;
	held.seedHeld = true;
	EXPECT_FALSE(ReadyForHand(held, true));
	EXPECT_EQ(Tap(held, true, true, true), TapResult::Nothing);
}

TEST(SpellIcons, OnlyAHeldSeedOfItStopsANewCharge)
{
	// A seed of it that is out of the hand, as one that cast a miracle, does not stop a new charge
	const IconCharge idle {.required = 500.0f};
	EXPECT_TRUE(MayStartCharge(idle, true, true));
	auto held = idle;
	held.seedHeld = true;
	EXPECT_FALSE(MayStartCharge(held, true, true));
}

TEST(SpellIcons, WithItsSeedHeldTheNeedIsWhatTheSeedStillNeeds)
{
	// The required value then is what the held seed still needs, and the store of the icon does not count
	const IconCharge free {.store = 100.0f, .required = 500.0f};
	EXPECT_EQ(Need(free), 400.0f);
	auto held = free;
	held.seedHeld = true;
	EXPECT_EQ(Need(held), 500.0f);
}

TEST(SpellIcons, AChargingIconWithoutAHeldSeedIsDoneWhenItsStoreIsFull)
{
	const auto part = ChargingIconTurn(300.0f, 500.0f, std::nullopt);
	EXPECT_EQ(part.store, 300.0f);
	EXPECT_FALSE(part.seedStore.has_value());
	EXPECT_FALSE(part.full);
	EXPECT_TRUE(ChargingIconTurn(500.0f, 500.0f, std::nullopt).full);
}

TEST(SpellIcons, AChargingIconMovesItsStoreIntoTheHeldSeedUpToWhatTheSeedNeeds)
{
	// The seed holds 200 of 500: it takes all 120 that the icon stored, and the charge continues
	const auto some = ChargingIconTurn(120.0f, 500.0f, 200.0f);
	EXPECT_EQ(some.store, 0.0f);
	ASSERT_TRUE(some.seedStore.has_value());
	EXPECT_EQ(*some.seedStore, 320.0f);
	EXPECT_FALSE(some.full);
	// The seed needs only 100 more of the 250 stored: it takes 100, the icon keeps 150, and the charge is done
	const auto done = ChargingIconTurn(250.0f, 500.0f, 400.0f);
	EXPECT_EQ(done.store, 150.0f);
	EXPECT_EQ(*done.seedStore, 500.0f);
	EXPECT_TRUE(done.full);
}

TEST(SpellIcons, AHeldSeedThatIsAlreadyFullTakesNothingAndEndsTheCharge)
{
	// This is how the icon whose full seed is in the hand stops at once
	const auto full = ChargingIconTurn(80.0f, 500.0f, 500.0f);
	EXPECT_EQ(full.store, 80.0f);
	EXPECT_EQ(*full.seedStore, 500.0f);
	EXPECT_TRUE(full.full);
	// A seed that holds more than the cost also takes nothing
	const auto over = ChargingIconTurn(80.0f, 500.0f, 600.0f);
	EXPECT_EQ(over.store, 80.0f);
	EXPECT_EQ(*over.seedStore, 600.0f);
	EXPECT_TRUE(over.full);
}

TEST(SpellIcons, ANeedThatIsNotANumberEndsTheCharge)
{
	const float nan = std::numeric_limits<float>::quiet_NaN();
	EXPECT_TRUE(ChargingIconTurn(0.0f, nan, std::nullopt).full);
	const auto held = ChargingIconTurn(50.0f, nan, 0.0f);
	EXPECT_EQ(held.store, 50.0f);
	EXPECT_TRUE(held.full);
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

TEST(SpellIcons, OnlyTheOwnersHandTapsAnIcon)
{
	EXPECT_TRUE(ValidToTap(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_ONE, true, false));
	EXPECT_FALSE(ValidToTap(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE, true, false));
	EXPECT_FALSE(ValidToTap(PlayerNames::NEUTRAL, PlayerNames::PLAYER_ONE, true, false));
}

TEST(SpellIcons, AnyHandTapsANeutralIconWhileInfluenceCoversTheLand)
{
	EXPECT_TRUE(ValidToTap(PlayerNames::NEUTRAL, PlayerNames::PLAYER_ONE, true, true));
	EXPECT_TRUE(ValidToTap(PlayerNames::NEUTRAL, PlayerNames::PLAYER_TWO, true, true));
	// The icon of another player stays its own, and an icon that does not work takes no tap
	EXPECT_FALSE(ValidToTap(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE, true, true));
	EXPECT_FALSE(ValidToTap(PlayerNames::NEUTRAL, PlayerNames::PLAYER_ONE, false, true));
}

TEST(SpellIcons, AnIconThatDoesNotWorkTakesNoTap)
{
	EXPECT_FALSE(ValidToTap(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_ONE, false, false));
}

TEST(SpellIcons, AnIconWorksWhenBuiltWithMoreThanThreeQuartersOfItsLife)
{
	EXPECT_TRUE(Functional(1.0f, true));
	EXPECT_TRUE(Functional(0.76f, true));
	EXPECT_FALSE(Functional(0.75f, true));
	EXPECT_FALSE(Functional(0.5f, true));
	EXPECT_FALSE(Functional(1.0f, false));
}

TEST(SpellIcons, AWorshipIconShowsItsStoreAsAPercentOfItsMiracle)
{
	// A quarter of the charge shows as 25, worked out in float as store over need, then times 100
	ASSERT_TRUE(ShownCharge(250.0f, 1000.0f).has_value());
	EXPECT_EQ(*ShownCharge(250.0f, 1000.0f), 25.0f);
	EXPECT_EQ(*ShownCharge(1.0f, 3.0f), (1.0f / 3.0f) * 100.0f);
	// An empty icon, or one with no charge to show at all, shows only the name
	EXPECT_FALSE(ShownCharge(0.0f, 1000.0f).has_value());
	EXPECT_FALSE(ShownCharge(0.0f, 0.0f).has_value());
}

TEST(SpellIcons, TheSeedInTheHandTakesItsChargeOffTheNeedThatTheIconShows)
{
	EXPECT_EQ(ShownRequired(1000.0f, std::nullopt), 1000.0f);
	EXPECT_EQ(ShownRequired(1000.0f, 600.0f), 400.0f);
	// The icon charges what the held seed still needs: 100 of the 400 left shows as 25
	EXPECT_EQ(*ShownCharge(100.0f, ShownRequired(1000.0f, 600.0f)), 25.0f);
}
