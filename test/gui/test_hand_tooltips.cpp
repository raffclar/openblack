/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <bit>

#include <gtest/gtest.h>

#include "Gui/HandToolTipRules.h"
#include "Gui/ToolTips.h"

using namespace openblack::gui;
using namespace openblack::gui::hand_tooltip;

namespace
{
constexpr uint32_t k_AllFeatures = k_PitchFeature | k_RotateFeature | k_ZoomFeature;

std::array<ToolTipInfo, ToolTips::k_Count> SameInfo()
{
	std::array<ToolTipInfo, ToolTips::k_Count> info {};
	info.fill({.priority = 0.5f, .displayTime = 1.0f, .displayTimeAfterFocus = 0.0f});
	return info;
}
} // namespace

TEST(HandToolTips, OverTheLandTheHandOffersToMoveWithTheMoveButtonAndAllArrows)
{
	const auto offer = OverNothing({.tricons = 0, .features = k_AllFeatures, .centreDistance = 100.0f, .pitch = 0.3f});
	EXPECT_EQ(offer, (Offer {.index = k_Move, .action = ToolTipAction::Select, .arrows = ToolTipArrows::k_All}));
}

TEST(HandToolTips, TheTiltHintComesBeforeTheTurnHint)
{
	const auto both = OverNothing({.tricons = k_PitchHint | k_RotateHint, .features = k_AllFeatures});
	EXPECT_EQ(both, (Offer {.index = k_Pitch, .action = ToolTipAction::Select, .arrows = ToolTipArrows::k_UpDown}));
	const auto turn = OverNothing({.tricons = k_RotateHint, .features = k_AllFeatures});
	EXPECT_EQ(turn, (Offer {.index = k_Rotate, .action = ToolTipAction::Select, .arrows = ToolTipArrows::k_All}));
}

TEST(HandToolTips, AControlThatTheScriptsStopIsNotOffered)
{
	// A lesson that stops the tilt leaves the turn
	const auto noTilt = OverNothing({.tricons = k_PitchHint | k_RotateHint, .features = k_RotateFeature});
	EXPECT_EQ(noTilt.index, k_Rotate);
	EXPECT_EQ(OverNothing({.tricons = k_PitchHint | k_RotateHint, .features = 0}).index, k_Move);
	EXPECT_EQ(OverNothing({.features = k_PitchFeature, .centreDistance = 12.0f, .pitch = 1.0f}).index, k_Move);
}

TEST(HandToolTips, ZoomOutNearTheLandWithTheCameraPointedDown)
{
	const auto near = OverNothing({.features = k_AllFeatures, .centreDistance = 14.9f, .pitch = 0.6f});
	EXPECT_EQ(near, (Offer {.index = k_ZoomOut, .action = ToolTipAction::ZoomIn, .arrows = ToolTipArrows::k_None}));
	EXPECT_EQ(OverNothing({.features = k_AllFeatures, .centreDistance = 15.0f, .pitch = 0.6f}).index, k_Move);
	EXPECT_EQ(OverNothing({.features = k_AllFeatures, .centreDistance = 14.0f, .pitch = 0.55f}).index, k_Move);
	// With no land at the middle of the screen the hand offers to move
	EXPECT_EQ(OverNothing({.features = k_AllFeatures, .centreDistance = std::nullopt, .pitch = 1.0f}).index, k_Move);
}

TEST(HandToolTips, OverTheFishOfAFarmTheHandOffersToPickThemUp)
{
	const Offer pickUp {.index = k_PickUp, .action = ToolTipAction::Apply, .arrows = ToolTipArrows::k_None};
	// It comes after the tilt and the turn of the camera, and before the zoom
	EXPECT_EQ(OverNothing({.features = k_AllFeatures, .centreDistance = 14.9f, .pitch = 0.6f}, true), pickUp);
	EXPECT_EQ(OverNothing({.features = k_AllFeatures, .centreDistance = 100.0f}, true), pickUp);
	EXPECT_EQ(OverNothing({.tricons = k_PitchHint, .features = k_AllFeatures}, true).index, k_Pitch);
	EXPECT_EQ(OverNothing({.tricons = k_RotateHint, .features = k_AllFeatures}, true).index, k_Rotate);
	// It sets off no help event
	EXPECT_FALSE(OverNothingHelpEvent({.features = k_AllFeatures, .centreDistance = 100.0f}, true).has_value());
	EXPECT_EQ(OverNothingHelpEvent({.features = k_AllFeatures, .centreDistance = 100.0f}, false), k_CameraOnLandEvent);
}

TEST(HandToolTips, ZoomOutShowsTheZoomInControlsMouse)
{
	EXPECT_EQ(BindableActionOf(ToolTipAction::ZoomIn), 4);
	EXPECT_EQ(BindableActionOf(ToolTipAction::Select), 1);
	EXPECT_EQ(BindableActionOf(ToolTipAction::Apply), 2);
	EXPECT_FALSE(BindableActionOf(ToolTipAction::None).has_value());
}

TEST(HandToolTips, OverAThingToTakeItIsPickUpThenItsTap)
{
	const auto stone = OverTakeable(false, std::nullopt);
	ASSERT_EQ(stone.size(), 1u);
	EXPECT_EQ(stone[0], (Offer {.index = k_PickUp, .action = ToolTipAction::Apply}));
	const auto rock = OverTakeable(false, k_TapToBreak);
	ASSERT_EQ(rock.size(), 2u);
	EXPECT_EQ(rock[0].index, k_PickUp);
	EXPECT_EQ(rock[1], (Offer {.index = k_TapToBreak, .action = ToolTipAction::Apply}));
	// A thing that moves can only be caught
	const auto flying = OverTakeable(true, k_TapToBreak);
	ASSERT_EQ(flying.size(), 1u);
	EXPECT_EQ(flying[0], (Offer {.index = k_Catch, .action = ToolTipAction::Apply}));
}

TEST(HandToolTips, TapsAndScrolls)
{
	EXPECT_EQ(TapOffer(TapKind::Knock).index, k_Tap);
	EXPECT_EQ(TapOffer(TapKind::Break).index, k_TapToBreak);
	EXPECT_EQ(TapOffer(TapKind::Open).index, k_TapToOpen);
	EXPECT_EQ(TapOffer(TapKind::Open).action, ToolTipAction::Apply);
	EXPECT_EQ(ScrollOffer(false).index, k_Activate);
	EXPECT_EQ(ScrollOffer(true).index, k_Reminder);
}

TEST(HandToolTips, HoldingAThingTheHandOffersToDropOrPlantItThenToThrowIt)
{
	const auto rock = Holding(true, HeldUse::None, false);
	ASSERT_TRUE(rock.has_value());
	ASSERT_EQ(rock->size(), 2u);
	EXPECT_EQ((*rock)[0], (Offer {.index = k_Drop, .action = ToolTipAction::Apply}));
	EXPECT_EQ((*rock)[1], (Offer {.index = k_Throw, .action = ToolTipAction::Apply, .arrows = ToolTipArrows::k_All}));
	const auto tree = Holding(true, HeldUse::None, true);
	ASSERT_TRUE(tree.has_value());
	EXPECT_EQ((*tree)[0].index, k_Plant);
	EXPECT_EQ((*Holding(true, HeldUse::Give, false))[0].index, k_Give);
	EXPECT_EQ((*Holding(true, HeldUse::Supply, true))[0].index, k_Supply);
	EXPECT_EQ(Holding(true, HeldUse::Give, false)->size(), 1u);
}

TEST(HandToolTips, OverAnAltarThatTakesTheHeldThingTheHandOffersToSacrificeIt)
{
	const auto offers = Holding(true, HeldUse::Sacrifice, true);
	ASSERT_TRUE(offers.has_value());
	ASSERT_EQ(offers->size(), 1u);
	EXPECT_EQ((*offers)[0].index, k_Sacrifice);
	EXPECT_EQ((*offers)[0].action, ToolTipAction::Apply);
	EXPECT_FALSE(Holding(false, HeldUse::Sacrifice, false).has_value());
}

TEST(HandToolTips, OutsideTheInfluenceAHeldThingOffersWhatTheLandOffers)
{
	EXPECT_FALSE(Holding(false, HeldUse::Give, false).has_value());
	EXPECT_FALSE(Holding(false, HeldUse::None, true).has_value());
}

TEST(HandToolTips, DropAndThrowTakeTurnsForEachDisplayTime)
{
	ToolTips toolTips(SameInfo());
	toolTips.SetLevel(ToolTipLevel::All);
	const auto offers = *Holding(true, HeldUse::None, false);
	const auto turn = [&]() {
		for (const auto& offer : offers)
		{
			toolTips.Submit(offer.index, offer.action, offer.arrows);
		}
		toolTips.ProcessTurn();
		return toolTips.GetShown()->index;
	};
	// One second of display time holds for 25 turns
	for (int i = 0; i < 25; ++i)
	{
		EXPECT_EQ(turn(), k_Drop) << i;
	}
	EXPECT_EQ(turn(), k_Throw);
}

TEST(HandToolTips, BuildingToolTipsShowOverAnOwnBuildingThatIsNotWhole)
{
	EXPECT_TRUE(OffersBuildingToolTips(false, true, k_PitchFeature));
	EXPECT_FALSE(OffersBuildingToolTips(true, true, k_PitchFeature));
	// A neutral town's building, or one of another player
	EXPECT_FALSE(OffersBuildingToolTips(false, false, k_PitchFeature));
	// A lesson that stops the tilt stops them too
	EXPECT_FALSE(OffersBuildingToolTips(false, true, k_RotateFeature | k_ZoomFeature));
}

TEST(HandToolTips, WoodRequiredShowsTheSitesWantOrHowFarItIsBuilt)
{
	const auto wanted = WoodRequired(42.4f, 30.0f);
	EXPECT_EQ(wanted.words, k_WoodRequired);
	EXPECT_DOUBLE_EQ(wanted.number, static_cast<double>(42.4f));
	EXPECT_DOUBLE_EQ(WoodRequired(-3.0f, 30.0f).number, 0.0);
	const auto noSite = WoodRequired(std::nullopt, 30.0f);
	EXPECT_EQ(noSite.words, k_BuildingCompleted);
	EXPECT_DOUBLE_EQ(noSite.number, 30.0);
	EXPECT_DOUBLE_EQ(BuildingCompleted(false, 0.25f, 1.0f), 25.0);
	EXPECT_DOUBLE_EQ(BuildingCompleted(true, 1.0f, 0.5f), 50.0);
	// The percentage is a float product: 0.0025 as a float gives 0.25 exactly
	EXPECT_EQ(std::bit_cast<uint32_t>(BuildingCompleted(false, 0.0025f, 1.0f)), std::bit_cast<uint32_t>(0.25f));
}

TEST(HandToolTips, GrippingTheLandWithBothButtonsOffersToZoom)
{
	const CameraFacts camera {.features = k_AllFeatures, .centreDistance = 100.0f, .pitch = 0.3f};
	EXPECT_EQ(GrippingLand(true, camera),
	          (Offer {.index = k_Zoom, .action = ToolTipAction::ZoomOn, .arrows = ToolTipArrows::k_UpDown}));
	EXPECT_EQ(BindableActionOf(ToolTipAction::ZoomOn), 6);
	// With the Move button alone the hand shows what it shows over the land
	EXPECT_EQ(GrippingLand(false, camera), OverNothing(camera));
	// When the scripts stop the zoom, both buttons also show what the hand shows over the land
	const CameraFacts noZoom {.features = k_PitchFeature | k_RotateFeature, .centreDistance = 100.0f, .pitch = 0.3f};
	EXPECT_EQ(GrippingLand(true, noZoom), OverNothing(noZoom));
}

TEST(HandToolTips, OverTheLandTheHandCountsTheCameraOnTheLand)
{
	// The middle of the screen shows the land: "Move" and "Zoom Out" count it
	EXPECT_EQ(OverNothingHelpEvent({.features = k_AllFeatures, .centreDistance = 100.0f, .pitch = 0.3f}), k_CameraOnLandEvent);
	EXPECT_EQ(OverNothingHelpEvent({.features = k_AllFeatures, .centreDistance = 10.0f, .pitch = 1.0f}), k_CameraOnLandEvent);
	// No land at the middle of the screen
	EXPECT_FALSE(OverNothingHelpEvent({.features = k_AllFeatures, .centreDistance = std::nullopt}).has_value());
	// The camera hints come first and count nothing
	EXPECT_FALSE(
	    OverNothingHelpEvent({.tricons = k_PitchHint, .features = k_AllFeatures, .centreDistance = 100.0f}).has_value());
	EXPECT_FALSE(
	    OverNothingHelpEvent({.tricons = k_RotateHint, .features = k_AllFeatures, .centreDistance = 100.0f}).has_value());
}

TEST(HandToolTips, AMiracleIconNamesItsMiracleWithNoMouse)
{
	// "Heal Miracle Increase" is the 79th of the game's tooltips
	const auto offer = SpellIconOffer(0xE73 + 78);
	ASSERT_TRUE(offer.has_value());
	EXPECT_EQ(offer->index, 78u);
	EXPECT_EQ(offer->action, ToolTipAction::None);
	EXPECT_EQ(offer->arrows, ToolTipArrows::k_None);
	// A text before or after the game's tooltips shows nothing
	EXPECT_FALSE(SpellIconOffer(0).has_value());
	EXPECT_FALSE(SpellIconOffer(0xE72).has_value());
	EXPECT_FALSE(SpellIconOffer(0xE73 + 170).has_value());
	EXPECT_TRUE(SpellIconOffer(0xE73 + 169).has_value());
}
