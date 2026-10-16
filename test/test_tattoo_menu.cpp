/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>

#include <PhysiqueFile.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Creature/TattooEditor.h"
#include "Creature/TattooMenu.h"

using namespace openblack;
using namespace openblack::tattoo_menu;

namespace
{
float HalfStrength(CreatureType /*species*/)
{
	return 0.5f;
}
} // namespace

TEST(TattooMenu, StandInIsAHalfFatHalfStrongApe)
{
	const auto shown = StandIn();
	EXPECT_EQ(shown.species, CreatureType::GiantApe);
	EXPECT_FLOAT_EQ(shown.morph.evilGood, 0.0f);
	EXPECT_FLOAT_EQ(shown.morph.thinFat, 0.5f);
	EXPECT_FLOAT_EQ(shown.morph.weakStrong, 0.5f);
	EXPECT_TRUE(shown.wounds.empty());
	EXPECT_TRUE(shown.blood.empty());
}

TEST(TattooMenu, NoFileShowsTheStandIn)
{
	EXPECT_EQ(FromPhysique(std::nullopt, HalfStrength), StandIn());
}

TEST(TattooMenu, UnknownSpeciesShowsTheStandIn)
{
	creaturemind::PhysiqueFileData physique;
	physique.speciesRow = 1000;
	EXPECT_EQ(FromPhysique(physique, HalfStrength), StandIn());
}

TEST(TattooMenu, FileGivesSpeciesShapeAndMarks)
{
	creaturemind::PhysiqueFileData physique;
	physique.speciesRow = static_cast<uint32_t>(CreatureType::Cow);
	physique.drawnSize = 3.0f;
	physique.alignment = -2.0f;
	physique.fatness = 1.0f;
	physique.strength = 1.0f;
	physique.wounds = {0x01020304u};
	physique.blood = {0x05060708u, 0x0A0B0C0Du};
	const auto shown = FromPhysique(physique, HalfStrength);
	EXPECT_EQ(shown.species, CreatureType::Cow);
	// The alignment is held at fully evil, a fat of 1 is fully fat, and the strength counts with the species' own
	const auto expected = creature_morph::FromAttributes(-2.0f, 1.0f, 1.0f, 0.5f);
	EXPECT_FLOAT_EQ(shown.morph.evilGood, -1.0f);
	EXPECT_FLOAT_EQ(shown.morph.thinFat, 1.0f);
	EXPECT_FLOAT_EQ(shown.morph.weakStrong, expected.weakStrong);
	ASSERT_EQ(shown.wounds.size(), 1u);
	EXPECT_EQ(shown.wounds.front(), creature_marks::WoundFromWord(0x01020304u));
	ASSERT_EQ(shown.blood.size(), 2u);
	EXPECT_EQ(shown.blood.back(), creature_marks::BloodFromWord(0x0A0B0C0Du));
}

TEST(TattooMenu, RowZeroIsTheApe)
{
	creaturemind::PhysiqueFileData physique;
	physique.speciesRow = 0;
	EXPECT_EQ(FromPhysique(physique, HalfStrength).species, CreatureType::GiantApe);
}

TEST(TattooMenu, ButtonNeedsFileShownCreatureAndNoRestart)
{
	EXPECT_TRUE(ButtonShown(true, true, false));
	EXPECT_FALSE(ButtonShown(false, true, false));
	EXPECT_FALSE(ButtonShown(true, false, false));
	EXPECT_FALSE(ButtonShown(true, true, true));
}

TEST(TattooMenu, PointerPointIsTenIntoTheScreen)
{
	const auto view =
	    glm::lookAtLH(creature_tattoo_editor::k_MenuEye, creature_tattoo_editor::k_MenuFocus, glm::vec3(0.0f, 1.0f, 0.0f));
	const auto projection = glm::perspectiveLH_ZO(glm::radians(60.0f), 4.0f / 3.0f, 1.0f, 1000.0f);
	// The middle of the screen is straight ahead, ten from the eye
	const auto middle = PointerPoint(view, projection, {800.0f, 600.0f}, {400.0f, 300.0f});
	ASSERT_TRUE(middle.has_value());
	const auto ahead = glm::normalize(creature_tattoo_editor::k_MenuFocus - creature_tattoo_editor::k_MenuEye);
	const auto expected = creature_tattoo_editor::k_MenuEye + (ahead * k_PointerDepth);
	EXPECT_NEAR(middle->x, expected.x, 1e-3f);
	EXPECT_NEAR(middle->y, expected.y, 1e-3f);
	EXPECT_NEAR(middle->z, expected.z, 1e-3f);
	// A pixel off the middle is still ten deep along the camera's forward axis
	const auto corner = PointerPoint(view, projection, {800.0f, 600.0f}, {0.0f, 0.0f});
	ASSERT_TRUE(corner.has_value());
	EXPECT_NEAR(glm::dot(*corner - creature_tattoo_editor::k_MenuEye, ahead), k_PointerDepth, 1e-3f);
	// Up and to the left of the eye
	EXPECT_GT(corner->y, expected.y);
	EXPECT_FALSE(PointerPoint(view, projection, {0.0f, 600.0f}, {0.0f, 0.0f}).has_value());
}
