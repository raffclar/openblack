/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <limits>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureMorph.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCopy.h"
#include "ECS/Registry.h"

using namespace openblack;
using A = creature::CreatureBody::Appearance;

TEST(CreatureMorph, AlignmentIsTheEvilGoodAxis)
{
	EXPECT_FLOAT_EQ(creature_morph::FromAttributes(-0.4f, 0.5f, 0.5f, 0.5f).evilGood, -0.4f);
	EXPECT_FLOAT_EQ(creature_morph::FromAttributes(3.0f, 0.5f, 0.5f, 0.5f).evilGood, 1.0f);
}

TEST(CreatureMorph, FatnessRunsFromThinToFat)
{
	EXPECT_FLOAT_EQ(creature_morph::FromAttributes(0.0f, 0.0f, 0.5f, 0.5f).thinFat, -1.0f);
	EXPECT_FLOAT_EQ(creature_morph::FromAttributes(0.0f, 0.5f, 0.5f, 0.5f).thinFat, 0.0f);
	EXPECT_FLOAT_EQ(creature_morph::FromAttributes(0.0f, 0.75f, 0.5f, 0.5f).thinFat, 0.5f);
}

TEST(CreatureMorph, StrengthCountsFourTimesTheSpecies)
{
	// 0.2 of the species' strength and 0.8 of the creature's
	EXPECT_FLOAT_EQ(creature_morph::FromAttributes(0.0f, 0.5f, 1.0f, 0.0f).weakStrong, 0.6f);
	EXPECT_FLOAT_EQ(creature_morph::FromAttributes(0.0f, 0.5f, 0.0f, 1.0f).weakStrong, -0.6f);
	EXPECT_FLOAT_EQ(creature_morph::FromAttributes(0.0f, 0.5f, 1.0f, 1.0f).weakStrong, 1.0f);
}

TEST(CreatureMorph, SizeIsKeptBetweenItsLimits)
{
	EXPECT_FLOAT_EQ(creature_morph::ClampScale(0.0f), 0.05f);
	EXPECT_FLOAT_EQ(creature_morph::ClampScale(1.5f), 1.5f);
	EXPECT_FLOAT_EQ(creature_morph::ClampScale(9.0f), 4.0f);
}

TEST(CreatureMorph, EachAxisPullsTowardsTheMeshOnItsSide)
{
	EXPECT_EQ(creature_morph::EvilGoodMesh(-0.1f), A::Evil);
	EXPECT_EQ(creature_morph::EvilGoodMesh(0.0f), A::Good);
	EXPECT_EQ(creature_morph::ThinFatMesh(-1.0f), A::Thin);
	EXPECT_EQ(creature_morph::ThinFatMesh(0.3f), A::Fat);
	EXPECT_EQ(creature_morph::WeakStrongMesh(-0.5f), A::Weak);
	EXPECT_EQ(creature_morph::WeakStrongMesh(1.0f), A::Strong);
}

TEST(CreatureMorph, BlendMovesTheBaseTowardsEachMesh)
{
	const glm::vec3 base {0.0f, 0.0f, 0.0f};
	const glm::vec3 evilGood {1.0f, 0.0f, 0.0f};
	const glm::vec3 thinFat {0.0f, 2.0f, 0.0f};
	const glm::vec3 weakStrong {0.0f, 0.0f, 4.0f};
	const auto blended =
	    creature_morph::Blend(base, evilGood, thinFat, weakStrong, {.evilGood = -0.5f, .thinFat = 0.25f, .weakStrong = 1.0f});
	EXPECT_FLOAT_EQ(blended.x, 0.5f);
	EXPECT_FLOAT_EQ(blended.y, 0.5f);
	EXPECT_FLOAT_EQ(blended.z, 4.0f);
	EXPECT_EQ(creature_morph::Blend(base, evilGood, thinFat, weakStrong, {}), base);
}

TEST(CreatureMorph, ShownFatnessFollowsByAHundredthATurn)
{
	EXPECT_FLOAT_EQ(creature_morph::EaseFatness(0.5f, 1.0f), 0.51f);
	EXPECT_FLOAT_EQ(creature_morph::EaseFatness(0.5f, 0.0f), 0.49f);
	EXPECT_FLOAT_EQ(creature_morph::EaseFatness(0.5f, 0.505f), 0.505f);
	// A fatness above 1 or below 0 (a new creature's spread start) is shown as 1 or 0
	EXPECT_FLOAT_EQ(creature_morph::EaseFatness(0.995f, 1.08f), 1.0f);
	EXPECT_FLOAT_EQ(creature_morph::EaseFatness(0.005f, -0.08f), 0.0f);
}

TEST(CreatureMorph, SmallChangesDoNotRedrawTheBody)
{
	const creature_morph::Morph drawn {.evilGood = 0.1f, .thinFat = 0.2f, .weakStrong = 0.3f};
	const auto refresh = creature_morph::RefreshDrawn(drawn, {.evilGood = 0.12f, .thinFat = 0.22f, .weakStrong = 0.32f});
	EXPECT_FALSE(refresh.animations);
	EXPECT_FALSE(refresh.vertices);
	EXPECT_FLOAT_EQ(refresh.drawn.evilGood, 0.1f);
}

TEST(CreatureMorph, ANewAlignmentRedrawsEveryAxis)
{
	const creature_morph::Morph drawn {.evilGood = 0.0f, .thinFat = 0.0f, .weakStrong = 0.0f};
	const auto refresh = creature_morph::RefreshDrawn(drawn, {.evilGood = 0.5f, .thinFat = 0.01f, .weakStrong = 0.02f});
	EXPECT_TRUE(refresh.animations);
	EXPECT_TRUE(refresh.vertices);
	EXPECT_FLOAT_EQ(refresh.drawn.evilGood, 0.5f);
	EXPECT_FLOAT_EQ(refresh.drawn.thinFat, 0.01f);
	EXPECT_FLOAT_EQ(refresh.drawn.weakStrong, 0.02f);
}

TEST(CreatureMorph, ANewFatnessRedrawsFatnessAndStrength)
{
	const creature_morph::Morph drawn {.evilGood = 0.0f, .thinFat = 0.0f, .weakStrong = 0.0f};
	const auto refresh = creature_morph::RefreshDrawn(drawn, {.evilGood = 0.02f, .thinFat = -0.03f, .weakStrong = 0.01f});
	EXPECT_TRUE(refresh.animations);
	EXPECT_FLOAT_EQ(refresh.drawn.evilGood, 0.0f);
	EXPECT_FLOAT_EQ(refresh.drawn.thinFat, -0.03f);
	EXPECT_FLOAT_EQ(refresh.drawn.weakStrong, 0.01f);
}

TEST(CreatureMorph, ANewStrengthOnlyReshapesTheVertices)
{
	const creature_morph::Morph drawn {.evilGood = 0.0f, .thinFat = 0.0f, .weakStrong = 0.0f};
	const auto refresh = creature_morph::RefreshDrawn(drawn, {.evilGood = 0.0f, .thinFat = 0.0f, .weakStrong = -1.0f});
	EXPECT_FALSE(refresh.animations);
	EXPECT_TRUE(refresh.vertices);
	EXPECT_FLOAT_EQ(refresh.drawn.weakStrong, -1.0f);
}

// A new body starts at the middle of each axis. Its first turn draws only the axes that are far enough from the middle.
TEST(CreatureMorph, TheFirstTurnOfANewBodyKeepsAnAxisNearTheMiddleAtTheMiddle)
{
	const auto refresh = creature_morph::RefreshDrawn({}, {.evilGood = 0.0f, .thinFat = 0.02f, .weakStrong = 0.5f});
	EXPECT_TRUE(refresh.vertices);
	EXPECT_FLOAT_EQ(refresh.drawn.evilGood, 0.0f);
	EXPECT_FLOAT_EQ(refresh.drawn.thinFat, 0.0f);
	EXPECT_FLOAT_EQ(refresh.drawn.weakStrong, 0.5f);
}

TEST(CreatureMorph, MissingMeshesFallBackOnTheBase)
{
	const auto base = creature::GetIdFromType(CreatureType::Tiger, A::Base);
	const auto evil = creature::GetIdFromType(CreatureType::Tiger, A::Evil);
	const auto meshes = creature_morph::MeshesOf(CreatureType::Tiger, {.evilGood = -0.5f, .thinFat = 0.5f},
	                                             [evil](entt::id_type id) { return id == evil; });
	EXPECT_EQ(meshes.base, base);
	EXPECT_EQ(meshes.evilGood, evil);
	EXPECT_EQ(meshes.thinFat, base);
	EXPECT_EQ(meshes.weakStrong, base);
}

TEST(CreatureMorph, ACreatureOfSizeOneIsFifteenUnitsTall)
{
	std::array<glm::mat4, 2> rest {glm::mat4(1.0f), glm::mat4(1.0f)};
	rest[0][3].y = 40.0f;
	rest[1][3].y = -20.0f;
	EXPECT_FLOAT_EQ(creature_morph::RestHeight(rest), 60.0f);
	EXPECT_FLOAT_EQ(creature_morph::DrawnScale(2.0f, 60.0f), 0.5f);
	// Bones all above the origin still reach down to it
	rest[1][3].y = 10.0f;
	EXPECT_FLOAT_EQ(creature_morph::RestHeight(rest), 40.0f);
	EXPECT_FLOAT_EQ(creature_morph::DrawnScale(2.0f, 0.0f), 2.0f);
}

TEST(CreatureMorph, ACopyLooksAsStrongAsTheOtherTimesTheScale)
{
	EXPECT_FLOAT_EQ(creature_morph::CopiedWeakStrong(0.4f, 1.5f), 0.6f);
	// A strong look never goes past fully strong
	EXPECT_FLOAT_EQ(creature_morph::CopiedWeakStrong(0.8f, 2.0f), 1.0f);
	// A weak look is divided by the scale, and a copy never looks weak
	EXPECT_FLOAT_EQ(creature_morph::CopiedWeakStrong(-0.5f, 2.0f), 0.0f);
	EXPECT_FLOAT_EQ(creature_morph::CopiedWeakStrong(-0.5f, -2.0f), 0.25f);
}

TEST(CreatureMorph, TheStrengthLookIsTheWeakStrongAxis)
{
	EXPECT_FLOAT_EQ(creature_morph::StrengthLook(0.5f, 0.5f), 0.0f);
	EXPECT_FLOAT_EQ(creature_morph::StrengthLook(1.0f, 0.5f), 0.8f);
	EXPECT_FLOAT_EQ(creature_morph::StrengthLook(0.25f, 1.0f), -0.2f);
	EXPECT_FLOAT_EQ(creature_morph::StrengthLook(1.0f, 1.0f), 1.0f);
	EXPECT_FLOAT_EQ(creature_morph::FromAttributes(0.0f, 0.5f, 0.25f, 1.0f).weakStrong,
	                creature_morph::StrengthLook(0.25f, 1.0f));
}

TEST(CreatureMorph, TheBodyTakesTheLooksOfTheCreatureAsItIsNow)
{
	const auto looks = creature_morph::TakeLooks(0.5f, 1.0f, -0.4f, 0.25f, 1.0f);
	EXPECT_FLOAT_EQ(looks.shownFatness, creature_morph::EaseFatness(0.5f, 1.0f));
	EXPECT_FLOAT_EQ(looks.evilGood, -0.4f);
	EXPECT_FLOAT_EQ(looks.weakStrong, creature_morph::StrengthLook(0.25f, 1.0f));
}

TEST(CreatureMorph, TheLookOfAFadedStrengthIsWeaker)
{
	// A strength that only fades still gives a weaker look
	const auto strong = creature_morph::TakeLooks(0.5f, 0.5f, 0.0f, 1.0f, 0.5f);
	const auto faded = creature_morph::TakeLooks(0.5f, 0.5f, 0.0f, 0.9f, 0.5f);
	EXPECT_LT(faded.weakStrong, strong.weakStrong);
}

TEST(CreatureMorph, TheLookOfTheAlignmentStaysOnItsAxis)
{
	EXPECT_EQ(creature_morph::TakeLooks(0.5f, 0.5f, -1.5f, 0.5f, 0.5f).evilGood, -1.0f);
	EXPECT_EQ(creature_morph::TakeLooks(0.5f, 0.5f, 1.5f, 0.5f, 0.5f).evilGood, 1.0f);
}

TEST(CreatureMorph, OnlyANewAlignmentPaintsTheSkinAgain)
{
	const creature_morph::Morph drawn {};
	EXPECT_TRUE(creature_morph::RefreshDrawn(drawn, {.evilGood = -0.03f, .thinFat = 0.0f, .weakStrong = 0.0f}).skin);
	EXPECT_FALSE(creature_morph::RefreshDrawn(drawn, {.evilGood = 0.02f, .thinFat = 0.5f, .weakStrong = 0.0f}).skin);
	EXPECT_FALSE(creature_morph::RefreshDrawn(drawn, {.evilGood = 0.0f, .thinFat = 0.0f, .weakStrong = 1.0f}).skin);
}

TEST(CreatureMorph, ACopyKeepsTheTargetsOfItsCreatureOnEachAxis)
{
	const auto copied = creature_morph::CopiedTargets({.evilGood = -1.5f, .thinFat = 0.25f, .weakStrong = 1.0f});
	EXPECT_EQ(copied.evilGood, -1.0f);
	EXPECT_EQ(copied.thinFat, 0.25f);
	EXPECT_EQ(copied.weakStrong, 1.0f);
	EXPECT_EQ(creature_morph::CopiedTargets({.evilGood = -1.0f, .thinFat = 2.0f, .weakStrong = 0.0f}).evilGood, -1.0f);
	EXPECT_EQ(creature_morph::CopiedTargets({.evilGood = 0.0f, .thinFat = 2.0f, .weakStrong = 0.0f}).thinFat, 1.0f);
	// A value that is not a number is not above -1
	const auto nan = std::numeric_limits<float>::quiet_NaN();
	EXPECT_EQ(creature_morph::CopiedTargets({.evilGood = nan, .thinFat = 0.0f, .weakStrong = 0.0f}).evilGood, -1.0f);
}

TEST(CreatureMorph, ANewCopyDrawsItsTargetsFromTheMiddle)
{
	const auto looks = creature_morph::FirstLooksOfCopy({.evilGood = 0.6f, .thinFat = 0.02f, .weakStrong = -0.4f});
	EXPECT_EQ(looks.targets, (creature_morph::Morph {.evilGood = 0.6f, .thinFat = 0.02f, .weakStrong = -0.4f}));
	// A new alignment draws every axis, also one near the middle
	EXPECT_EQ(looks.drawn, looks.targets);
	EXPECT_TRUE(looks.painted);
}

TEST(CreatureMorph, ACopyOfACreatureNearTheMiddleKeepsItsUnpaintedSkin)
{
	const auto looks = creature_morph::FirstLooksOfCopy({.evilGood = 0.029f, .thinFat = 0.5f, .weakStrong = 0.01f});
	EXPECT_FALSE(looks.painted);
	EXPECT_EQ(looks.drawn.evilGood, 0.0f);
	EXPECT_EQ(looks.drawn.thinFat, 0.5f);
	EXPECT_EQ(looks.drawn.weakStrong, 0.01f);
	EXPECT_EQ(looks.targets.evilGood, 0.029f);
}

TEST(CreatureMorph, ACopyWithLooksOfItsOwnIsDrawnWithItsOwnShape)
{
	using namespace openblack::ecs::components;
	ecs::Registry land;
	const auto creature = land.Create();
	land.Assign<CreatureMorph>(creature, CreatureMorph {.drawn = {.evilGood = 0.9f, .thinFat = 0.0f, .weakStrong = 0.0f}});

	ecs::Registry copies;
	const auto plain = copies.Create();
	copies.Assign<CreatureCopy>(plain, CreatureCopy {.creature = creature, .size = 1.0f});
	const auto* shape = ShapeOfCopy(copies, plain, land);
	ASSERT_NE(shape, nullptr);
	EXPECT_EQ(shape->drawn.evilGood, 0.9f);
	EXPECT_EQ(HairAlignmentOfCopy(copies.Get<const CreatureCopy>(plain), *shape), 0.9f);

	const auto kept = copies.Create();
	copies.Assign<CreatureCopy>(kept, CreatureCopy {.creature = creature,
	                                                .size = 1.0f,
	                                                .targets = creature_morph::Morph {.evilGood = 0.02f},
	                                                .paintedAlignment = std::nullopt});
	copies.Assign<CreatureMorph>(kept, CreatureMorph {.drawn = {}});
	shape = ShapeOfCopy(copies, kept, land);
	ASSERT_NE(shape, nullptr);
	EXPECT_EQ(shape->drawn.evilGood, 0.0f);
	// Hair that was not painted has the look of a creature that is neither evil nor good
	EXPECT_EQ(HairAlignmentOfCopy(copies.Get<const CreatureCopy>(kept), *shape), 0.0f);
	copies.Get<CreatureCopy>(kept).paintedAlignment = 0.02f;
	EXPECT_EQ(HairAlignmentOfCopy(copies.Get<const CreatureCopy>(kept), *shape), 0.02f);
}
