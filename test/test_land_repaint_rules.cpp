/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/Components/MapCellResident.h"
#include "ECS/LandRepaintRules.h"

namespace land_repaint = openblack::ecs::land_repaint;
using land_repaint::Change;
using land_repaint::Filing;
using land_repaint::Paints;
using land_repaint::Repaints;
using openblack::ecs::components::MapCellResident;
using openblack::ecs::map_cells::Placement;

namespace
{
constexpr Paints k_None {.oldPlace = false, .newPlace = false};
constexpr Paints k_Both {.oldPlace = true, .newPlace = true};
} // namespace

TEST(LandRepaintRules, TheFilingComesFromHowTheThingIsInTheCells)
{
	EXPECT_EQ(land_repaint::FilingOf(MapCellResident {.placement = Placement::FixedFront, .coversOutline = true}),
	          Filing::Outline);
	EXPECT_EQ(land_repaint::FilingOf(MapCellResident {.placement = Placement::FixedFront, .coversOutline = false}),
	          Filing::OneCell);
	EXPECT_EQ(land_repaint::FilingOf(MapCellResident {.placement = Placement::FixedBack, .coversOutline = false}),
	          Filing::OneCell);
	EXPECT_EQ(land_repaint::FilingOf(MapCellResident {.placement = Placement::MobileFront, .coversOutline = false}),
	          Filing::Mobile);
}

TEST(LandRepaintRules, OnlyAThingOverItsOutlineAsksForAPaintAsItComesOut)
{
	EXPECT_EQ(land_repaint::RepaintsOf(Filing::Outline, false), (Repaints {.goingIn = true, .comingOut = true}));
	EXPECT_EQ(land_repaint::RepaintsOf(Filing::OneCell, false), (Repaints {.goingIn = true, .comingOut = false}));
	EXPECT_EQ(land_repaint::RepaintsOf(Filing::Mobile, false), (Repaints {.goingIn = false, .comingOut = false}));
	// The flag of the model asks for a paint each time
	for (const auto filing : {Filing::Outline, Filing::OneCell, Filing::Mobile})
	{
		EXPECT_EQ(land_repaint::RepaintsOf(filing, true), (Repaints {.goingIn = true, .comingOut = true}));
	}
}

TEST(LandRepaintRules, AThingThatOnlyGrowsInItsCellsPaintsNothing)
{
	for (const auto filing : {Filing::Outline, Filing::OneCell, Filing::Mobile})
	{
		EXPECT_EQ(land_repaint::PaintsOf(filing, land_repaint::RepaintsOf(filing, false), Change {.resized = true}), k_None);
		EXPECT_EQ(land_repaint::PaintsOf(filing, land_repaint::RepaintsOf(filing, false), Change {}), k_None);
	}
}

TEST(LandRepaintRules, AFilingAgainPaintsThePlacesThatTheThingAsksFor)
{
	const Change filedAgain {.filedAgain = true, .moved = true};
	EXPECT_EQ(land_repaint::PaintsOf(Filing::Outline, land_repaint::RepaintsOf(Filing::Outline, false), filedAgain), k_Both);
	// A tree that the hand puts down in other cells paints only its new place
	EXPECT_EQ(land_repaint::PaintsOf(Filing::OneCell, land_repaint::RepaintsOf(Filing::OneCell, false), filedAgain),
	          (Paints {.oldPlace = false, .newPlace = true}));
	// A ball that rolls into other cells paints nothing
	EXPECT_EQ(land_repaint::PaintsOf(Filing::Mobile, land_repaint::RepaintsOf(Filing::Mobile, false), filedAgain), k_None);
	// The largest ring of the beach temple puzzle paints its two places
	EXPECT_EQ(land_repaint::PaintsOf(Filing::Mobile, land_repaint::RepaintsOf(Filing::Mobile, true), filedAgain), k_Both);
}

TEST(LandRepaintRules, AThingThatMovesInItsCellsPaintsNothing)
{
	for (const bool flagged : {false, true})
	{
		const auto repaints = land_repaint::RepaintsOf(Filing::Mobile, flagged);
		EXPECT_EQ(land_repaint::PaintsOf(Filing::Mobile, repaints, Change {.moved = true}), k_None);
		EXPECT_EQ(land_repaint::PaintsOf(Filing::Mobile, repaints, Change {.otherLook = true}), k_None);
	}
}

TEST(LandRepaintRules, AThingThatStaysPutAndMovesOrChangesItsLookPaintsTheTwoPlaces)
{
	for (const auto filing : {Filing::Outline, Filing::OneCell})
	{
		const auto repaints = land_repaint::RepaintsOf(filing, false);
		EXPECT_EQ(land_repaint::PaintsOf(filing, repaints, Change {.moved = true}), k_Both);
		EXPECT_EQ(land_repaint::PaintsOf(filing, repaints, Change {.resized = true, .otherLook = true}), k_Both);
	}
}

TEST(LandRepaintRules, AModelThatBecomesReadyPaintsTheTwoPlaces)
{
	for (const auto filing : {Filing::Outline, Filing::OneCell, Filing::Mobile})
	{
		EXPECT_EQ(land_repaint::PaintsOf(filing, land_repaint::RepaintsOf(filing, false), Change {.modelReady = true}), k_Both);
	}
}
