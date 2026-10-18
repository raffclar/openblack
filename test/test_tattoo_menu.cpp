/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>
#include <vector>

#include <PhysiqueFile.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureHair.h"
#include "Creature/TattooEditor.h"
#include "Creature/TattooMenu.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Systems/CreatureHairStep.h"

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

namespace
{
using openblack::ecs::components::CreatureHair;
using openblack::ecs::systems::HairGroupShape;

// A mesh with two tufts: one of three strands and one of one strand
HairGroupShape FakeShape(size_t group)
{
	return {.strandCount = group == 0 ? 3u : 1u, .segmentCount = 4, .physics = {}, .colour = {10, 20, 30}, .textured = true};
}

std::optional<openblack::creature_hair::Root> FakeRoot(size_t /*group*/, size_t strand)
{
	return openblack::creature_hair::Root {.position = {static_cast<float>(strand), 2.0f, 0.0f},
	                                       .inwardNormal = {0.0f, -1.0f, 0.0f},
	                                       .direction = {0.0f, 1.0f, 0.0f}};
}
} // namespace

TEST(TattooMenuHair, TheMenuCreatureTakesItsGroupsAndStrandsFromItsMesh)
{
	CreatureHair hair;
	EXPECT_FALSE(hair.started);
	EXPECT_TRUE(hair.groups.empty());
	openblack::ecs::systems::StepHair(hair, 2, FakeShape, FakeRoot, 0.01f);
	ASSERT_EQ(hair.groups.size(), 2u);
	EXPECT_EQ(hair.groups[0].strands.size(), 3u);
	EXPECT_EQ(hair.groups[1].strands.size(), 1u);
	EXPECT_EQ(hair.groups[0].strands[0].positions.size(), 4u);
	EXPECT_EQ(hair.groups[0].colour, glm::ivec3(10, 20, 30));
	EXPECT_TRUE(hair.groups[0].textured);
	EXPECT_TRUE(hair.started);
}

TEST(TattooMenuHair, TheStrandsStartFreshAtEachOpening)
{
	CreatureHair first;
	openblack::ecs::systems::StepHair(first, 2, FakeShape, FakeRoot, 0.5f);
	const auto moved = first.groups[0].strands[1].positions;
	// The editor opens again: the creature is new and has new hair, so it moves as the first one did, not on from it
	CreatureHair second;
	EXPECT_FALSE(second.started);
	openblack::ecs::systems::StepHair(second, 2, FakeShape, FakeRoot, 0.5f);
	EXPECT_EQ(second.groups[0].strands[1].positions, moved);
	// Hair that goes on moves from where it was
	const auto liftedRoot = [](size_t group, size_t strand) {
		auto root = FakeRoot(group, strand);
		root->position.y += 1.0f;
		return root;
	};
	openblack::ecs::systems::StepHair(first, 2, FakeShape, liftedRoot, 0.5f);
	EXPECT_NE(first.groups[0].strands[1].positions, moved);
}

TEST(TattooMenuHair, TheHairMovesByTheRealFrameTimeToAtMostOneSecond)
{
	using openblack::creature_hair::StepSeconds;
	EXPECT_FLOAT_EQ(StepSeconds(10.0f), 0.01f);
	EXPECT_FLOAT_EQ(StepSeconds(1000.0f), 1.0f);
	EXPECT_FLOAT_EQ(StepSeconds(2500.0f), 1.0f);
	EXPECT_FLOAT_EQ(StepSeconds(-5.0f), 0.0f);
	EXPECT_FLOAT_EQ(StepSeconds(0.0f), 0.0f);
}

TEST(TattooMenuHair, TheHairIsDrawnAfterTheBody)
{
	EXPECT_EQ(DrawOrder(true, true), (std::vector<Layer> {Layer::Body, Layer::Hair}));
	EXPECT_EQ(DrawOrder(true, false), (std::vector<Layer> {Layer::Body}));
	EXPECT_EQ(DrawOrder(false, true), (std::vector<Layer> {Layer::Body}));
}
