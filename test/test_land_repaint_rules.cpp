/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <optional>

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
constexpr std::optional<land_repaint::Flag> k_NoFlag = std::nullopt;
constexpr std::array k_Filings {Filing::Outline, Filing::OneCell, Filing::Plain};
} // namespace

TEST(LandRepaintRules, TheFilingComesFromHowTheThingIsInTheCells)
{
	EXPECT_EQ(land_repaint::FilingOf(MapCellResident {.placement = Placement::FixedFront, .coversOutline = true}),
	          Filing::Outline);
	EXPECT_EQ(land_repaint::FilingOf(MapCellResident {.placement = Placement::FixedFront, .coversOutline = false}),
	          Filing::OneCell);
	// A street lantern, a town's flag, a pot or a seed goes into its cell as a plain thing
	EXPECT_EQ(land_repaint::FilingOf(MapCellResident {.placement = Placement::FixedBack, .coversOutline = false}),
	          Filing::Plain);
	EXPECT_EQ(land_repaint::FilingOf(MapCellResident {.placement = Placement::MobileFront, .coversOutline = false}),
	          Filing::Plain);
}

TEST(LandRepaintRules, OnlyAThingOverItsOutlineAsksForAPaintAsItComesOut)
{
	EXPECT_EQ(land_repaint::RepaintsOf(Filing::Outline, k_NoFlag, 1), (Repaints {.goingIn = true, .comingOut = true}));
	EXPECT_EQ(land_repaint::RepaintsOf(Filing::OneCell, k_NoFlag, 1), (Repaints {.goingIn = true, .comingOut = false}));
	EXPECT_EQ(land_repaint::RepaintsOf(Filing::Plain, k_NoFlag, 1), (Repaints {.goingIn = false, .comingOut = false}));
}

TEST(LandRepaintRules, TheFlagOfTheModelCountsAfterTheFilingThatItCameIn)
{
	constexpr land_repaint::Flag k_Flag {.afterFiling = 2};
	// The filing in which the thing got the flag went in with no paint, but it asks as it comes out
	EXPECT_EQ(land_repaint::RepaintsOf(Filing::Plain, k_Flag, 2), (Repaints {.goingIn = false, .comingOut = true}));
	for (const auto filing : k_Filings)
	{
		EXPECT_EQ(land_repaint::RepaintsOf(filing, k_Flag, 3), (Repaints {.goingIn = true, .comingOut = true}));
	}
}

TEST(LandRepaintRules, AThingThatChangesInTheSameFilingPaintsNothing)
{
	for (const auto filing : k_Filings)
	{
		const auto repaints = land_repaint::RepaintsOf(filing, k_NoFlag, 1);
		EXPECT_EQ(land_repaint::PaintsOf(repaints, Change {}), k_None);
		EXPECT_EQ(land_repaint::PaintsOf(repaints, Change {.resized = true}), k_None);
		EXPECT_EQ(land_repaint::PaintsOf(repaints, Change {.moved = true}), k_None);
		EXPECT_EQ(land_repaint::PaintsOf(repaints, Change {.otherLook = true}), k_None);
		EXPECT_EQ(land_repaint::PaintsOf(repaints, Change {.moved = true, .resized = true, .otherLook = true}), k_None);
	}
	// Also with the flag of the model
	const auto flagged = land_repaint::RepaintsOf(Filing::Plain, land_repaint::Flag {.afterFiling = 0}, 1);
	EXPECT_EQ(land_repaint::PaintsOf(flagged, Change {.moved = true, .otherLook = true}), k_None);
}

TEST(LandRepaintRules, AFilingAgainPaintsThePlacesThatTheThingAsksFor)
{
	const Change filedAgain {.filedAgain = true, .moved = true};
	EXPECT_EQ(land_repaint::PaintsOf(land_repaint::RepaintsOf(Filing::Outline, k_NoFlag, 2), filedAgain), k_Both);
	// A tree that the hand puts down in other cells paints only its new place
	EXPECT_EQ(land_repaint::PaintsOf(land_repaint::RepaintsOf(Filing::OneCell, k_NoFlag, 2), filedAgain),
	          (Paints {.oldPlace = false, .newPlace = true}));
	// A ball that rolls into other cells paints nothing
	EXPECT_EQ(land_repaint::PaintsOf(land_repaint::RepaintsOf(Filing::Plain, k_NoFlag, 2), filedAgain), k_None);
	// The largest ring of the beach temple puzzle paints its two places
	EXPECT_EQ(
	    land_repaint::PaintsOf(land_repaint::RepaintsOf(Filing::Plain, land_repaint::Flag {.afterFiling = 1}, 2), filedAgain),
	    k_Both);
}

TEST(LandRepaintRules, AModelThatBecomesReadyPaintsTheTwoPlaces)
{
	for (const auto filing : k_Filings)
	{
		EXPECT_EQ(land_repaint::PaintsOf(land_repaint::RepaintsOf(filing, k_NoFlag, 1), Change {.modelReady = true}), k_Both);
	}
}
