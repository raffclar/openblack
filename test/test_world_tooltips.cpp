/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Gui/WorldToolTipRules.h"

using namespace openblack::gui::world_tooltip;

TEST(WorldToolTips, BeliefIsAlwaysTheBeliefLeftTooltipInThousandths)
{
	// Ahead or behind, the town centre offers the same tooltip, with the signed margin in thousandths
	const auto ahead = Belief(0.5);
	EXPECT_EQ(ahead.index, k_BeliefLeft);
	EXPECT_DOUBLE_EQ(ahead.first, 500.0);
	const auto behind = Belief(-0.25);
	EXPECT_EQ(behind.index, k_BeliefLeft);
	EXPECT_DOUBLE_EQ(behind.first, -250.0);
	EXPECT_FALSE(behind.second.has_value());
}

TEST(WorldToolTips, BeliefReadsNeededWithTheShortfallWhileThePlayerTrails)
{
	const auto ahead = Worded(k_BeliefLeft, 500.0);
	EXPECT_EQ(ahead.words, k_BeliefLeft);
	EXPECT_DOUBLE_EQ(ahead.number, 500.0);
	const auto behind = Worded(k_BeliefLeft, -250.0);
	EXPECT_EQ(behind.words, k_BeliefNeeded);
	EXPECT_DOUBLE_EQ(behind.number, 250.0);
	// Level with the leader still reads as belief left
	EXPECT_EQ(Worded(k_BeliefLeft, 0.0).words, k_BeliefLeft);
	EXPECT_EQ(Compose(k_BeliefNeeded, u"%3.0f Belief Needed", Worded(k_BeliefLeft, -12.5).number, std::nullopt),
	          u" 13 Belief Needed");
	// Other tooltips keep their words and numbers, negative or not
	const auto other = Worded(k_TempleHealth, -3.0);
	EXPECT_EQ(other.words, k_TempleHealth);
	EXPECT_DOUBLE_EQ(other.number, -3.0);
}

TEST(WorldToolTips, TempleHealthIsPercentage)
{
	EXPECT_DOUBLE_EQ(TempleHealth(1.0f).first, 100.0);
	EXPECT_EQ(Compose(k_TempleHealth, u"Temple Health: %3.0f%%", TempleHealth(0.755f).first, std::nullopt),
	          u"Temple Health:  76%");
}

TEST(WorldToolTips, PopulationAsPeopleOverPlaces)
{
	const auto population = Population(12, 34);
	EXPECT_EQ(Compose(population.index, u"Population: %3.0f", population.first, population.second), u"Population:  12/34");
}

TEST(WorldToolTips, StoredWritesFoodThenWood)
{
	const auto stored = Stored(5, 120);
	EXPECT_EQ(Compose(stored.index, u"Stored - Food: %3.0f Wood: %3.0f", stored.first, stored.second),
	          u"Stored - Food:   5 Wood: 120");
}

TEST(WorldToolTips, PilesAndHeldAmounts)
{
	EXPECT_EQ(PileAmount(true, 1).index, k_FoodAmount);
	EXPECT_EQ(PileAmount(false, 1).index, k_WoodAmount);
	EXPECT_EQ(HeldAmount(7).index, k_HeldAmount);
	EXPECT_EQ(Compose(k_HeldAmount, u"Amount: %3.0f", 7.0, std::nullopt), u"Amount:   7");
}

TEST(WorldToolTips, AScaffoldOverAPlannedGraveyardNamesIt)
{
	// A planned graveyard's text for building it is the game's "Graveyard" tooltip text
	EXPECT_EQ(ScaffoldDrop(k_FirstToolTipText + k_Graveyard), std::optional(k_Graveyard));
	EXPECT_EQ(ScaffoldDrop(std::nullopt), std::optional(k_Build));
}

TEST(WorldToolTips, OnlyTheGamesTooltipTextsAreTooltips)
{
	EXPECT_EQ(ToolTipOfText(k_FirstToolTipText), std::optional(0u));
	EXPECT_EQ(ToolTipOfText(k_FirstToolTipText + k_ToolTipCount - 1), std::optional(k_ToolTipCount - 1));
	EXPECT_FALSE(ToolTipOfText(k_FirstToolTipText - 1).has_value());
	EXPECT_FALSE(ToolTipOfText(k_FirstToolTipText + k_ToolTipCount).has_value());
	EXPECT_FALSE(ScaffoldDrop(0u).has_value());
}

TEST(WorldToolTips, TheTooltipHangsWhereTheHandIsDrawnNotWhereThePointerIs)
{
	// A camera looking straight down the z axis: a point at x = y = 0 lands mid-screen, whatever the pointer does
	const glm::mat4 worldToClip {1.0f};
	const glm::ivec2 screen {800, 600};
	const glm::vec3 pinnedHand {0.0f, 0.0f, 0.0f};
	// clip.w is 1 for the identity, so a near plane above it puts the hand behind the camera
	const auto anchored = HandOnScreen(worldToClip, 0.5f, pinnedHand, screen);
	ASSERT_TRUE(anchored.has_value());
	EXPECT_EQ(*anchored, glm::ivec2(400, 300));
	// The hand right of and above the middle is drawn there, in whole pixels cut down
	EXPECT_EQ(*HandOnScreen(worldToClip, 0.5f, {0.501f, 0.5f, 0.0f}, screen), glm::ivec2(600, 150));
}

TEST(WorldToolTips, NoTooltipWithTheHandBehindTheCamera)
{
	EXPECT_FALSE(HandOnScreen(glm::mat4 {1.0f}, 2.0f, {0.0f, 0.0f, 0.0f}, {800, 600}).has_value());
}

TEST(WorldToolTips, AHandOffTheScreenKeepsItsTooltipOnTheEdge)
{
	// Off the screen the hand still counts, its tooltip held in by the tooltip's size
	const auto off = HandOnScreen(glm::mat4 {1.0f}, 0.5f, {2.0f, -2.0f, 0.0f}, {800, 600});
	ASSERT_TRUE(off.has_value());
	EXPECT_EQ(KeptOnScreen(*off, {800, 600}, 24), glm::ivec2(776, 576));
	EXPECT_EQ(KeptOnScreen({-30, -1}, {800, 600}, 24), glm::ivec2(0, 0));
	EXPECT_EQ(KeptOnScreen({100, 200}, {800, 600}, 24), glm::ivec2(100, 200));
}

TEST(WorldToolTips, NothingUnderThePointerShowsWithAMiracleInTheHand)
{
	// Holding or pouring food beside a wood pile, the pile the pointer crosses says nothing
	EXPECT_FALSE(OffersOverToolTips(true));
	// With nothing in the hand, what is under the pointer shows
	EXPECT_TRUE(OffersOverToolTips(false));
}

namespace
{
using openblack::MagicType;
using openblack::SpellSeedType;
using openblack::magic::HeldCastAtPointer;
using openblack::magic::HeldCastOffer;
using openblack::magic::MayCastOn;
using openblack::magic::OfferFor;
using openblack::magic::PointedThing;

HeldCastAtPointer ReadyOverLand(MagicType type, bool atPoint)
{
	return {.handInInfluence = true, .ready = true, .type = type, .onPointed = std::nullopt, .atPoint = atPoint};
}
} // namespace

TEST(WorldToolTips, AHeldMiracleOffersCastOnTheLandWhereItMayBeCast)
{
	EXPECT_EQ(OfferFor(ReadyOverLand(MagicType::Food, true)), HeldCastOffer::Cast);
	EXPECT_EQ(HeldMiracle(HeldCastOffer::Cast), k_Cast);
	// Where its rule forbids it (food over the sea), the hand offers nothing of its own
	EXPECT_EQ(OfferFor(ReadyOverLand(MagicType::Food, false)), HeldCastOffer::None);
	EXPECT_FALSE(HeldMiracle(HeldCastOffer::None).has_value());
}

TEST(WorldToolTips, TheMiraclesCastAtACircleOfferCastCircleOverTheLand)
{
	for (const auto type : {MagicType::StormWindRain, MagicType::StormWindRainLightning, MagicType::Tornado, MagicType::Shield,
	                        MagicType::PhysicalShield})
	{
		EXPECT_EQ(OfferFor(ReadyOverLand(type, true)), HeldCastOffer::CastCircle);
	}
	EXPECT_EQ(HeldMiracle(HeldCastOffer::CastCircle), k_CastCircle);
	// The miracles either side of them in the table don't
	EXPECT_EQ(OfferFor(ReadyOverLand(MagicType::FoodPowerUpOne, true)), HeldCastOffer::Cast);
	EXPECT_EQ(OfferFor(ReadyOverLand(MagicType::Wood, true)), HeldCastOffer::Cast);
}

TEST(WorldToolTips, OverAThingOnlyTheThingCounts)
{
	auto held = ReadyOverLand(MagicType::Food, true);
	held.onPointed = false;
	EXPECT_EQ(OfferFor(held), HeldCastOffer::None);
	// A storm held over a thing it may be cast on still says "Cast", never "Cast Circle"
	held = ReadyOverLand(MagicType::StormWindRain, false);
	held.onPointed = true;
	EXPECT_EQ(OfferFor(held), HeldCastOffer::Cast);
}

TEST(WorldToolTips, AHeldMiracleOffersNothingUnreadyOrOutOfInfluence)
{
	auto held = ReadyOverLand(MagicType::Water, true);
	held.ready = false;
	EXPECT_EQ(OfferFor(held), HeldCastOffer::None);
	held = ReadyOverLand(MagicType::Water, true);
	held.handInInfluence = false;
	EXPECT_EQ(OfferFor(held), HeldCastOffer::None);
}

TEST(WorldToolTips, WhatAHeldSeedMayBeCastOn)
{
	// The creature spells on a creature with room for another
	EXPECT_TRUE(MayCastOn(SpellSeedType::CreatureSpellBig, {.castOnByKind = true}));
	EXPECT_FALSE(MayCastOn(SpellSeedType::Food, {}));
	// Never on a thing a script has highlighted
	EXPECT_FALSE(MayCastOn(SpellSeedType::CreatureSpellBig, {.scriptHighlight = true, .castOnByKind = true}));
	// A fireball takes the fire seed only, whatever else would say
	EXPECT_TRUE(MayCastOn(SpellSeedType::Fire, {.fireBall = true}));
	EXPECT_FALSE(MayCastOn(SpellSeedType::Food, {.fireBall = true, .castOnByKind = true}));
}

TEST(WorldToolTips, TheTemplesHealthAndTheVillageCentresTipsShowAnywhere)
{
	for (const auto subject : {Subject::Temple, Subject::VillageCentre, Subject::OwnCreature})
	{
		EXPECT_FALSE(NeedsInfluence(subject));
		EXPECT_TRUE(Shows(subject, true));
		EXPECT_TRUE(Shows(subject, false));
	}
}

TEST(WorldToolTips, WhatTheHandCouldTakeOrTapShowsOnlyInTheInfluence)
{
	for (const auto subject :
	     {Subject::TempleEntrance, Subject::Store, Subject::Pile, Subject::DesireFlag, Subject::OthersCreature})
	{
		EXPECT_TRUE(NeedsInfluence(subject));
		EXPECT_TRUE(Shows(subject, true));
		EXPECT_FALSE(Shows(subject, false));
	}
}

TEST(WorldToolTips, TheWayIntoTheOwnBuiltTempleShowsOnlyInTheInfluence)
{
	EXPECT_TRUE(OffersEnterTemple(true, true, true));
	EXPECT_FALSE(OffersEnterTemple(true, true, false));
}
