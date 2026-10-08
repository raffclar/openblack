/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What each kind of object is to the physics: which are hit by thrown things and which can fly.

#include <gtest/gtest.h>

#include "ECS/Physics/PhysicsClassRules.h"

using namespace openblack;
namespace rules = openblack::ecs::physics::class_rules;

TEST(PhysicsClassRules, buildingsAreHitWhileBuiltAndStanding)
{
	EXPECT_TRUE(rules::StandingBuilding(1.0f, 1.0f));
	// both limits are strict
	EXPECT_FALSE(rules::StandingBuilding(0.1f, 1.0f));
	EXPECT_FALSE(rules::StandingBuilding(1.0f, 0.01f));
	EXPECT_TRUE(rules::StandingBuilding(0.11f, 0.02f));
	// a site barely started is not in the way, however much life it has
	EXPECT_FALSE(rules::AbodeIsObstacle(AbodeNumber::Totem, 0.05f, 1.0f));
	EXPECT_TRUE(rules::AbodeIsObstacle(AbodeNumber::SpellDispenser, 0.5f, 0.5f));
}

TEST(PhysicsClassRules, townCentreAlwaysAndSomeBuildingsNever)
{
	EXPECT_TRUE(rules::AbodeIsObstacle(AbodeNumber::TownCentre, 0.0f, 0.0f));
	EXPECT_FALSE(rules::AbodeIsObstacle(AbodeNumber::Graveyard, 1.0f, 1.0f));
	EXPECT_FALSE(rules::AbodeIsObstacle(AbodeNumber::FootballPitch, 1.0f, 1.0f));
	EXPECT_FALSE(rules::AbodeIsObstacle(AbodeNumber::Field, 1.0f, 1.0f));
}

TEST(PhysicsClassRules, templeHeartComparesAtDoublePrecision)
{
	// 0.1f is a little more than the double 0.1, so a heart exactly a tenth built (as a float) is already hit
	EXPECT_TRUE(rules::CitadelHeartIsObstacle(0.1f));
	EXPECT_FALSE(rules::CitadelHeartIsObstacle(0.0999f));
	EXPECT_TRUE(rules::CitadelHeartIsObstacle(1.0f));
}

TEST(PhysicsClassRules, staticsHitByKind)
{
	const auto none = MobileStaticInfo::None;
	// rocks and idols by their mobile type, whatever their build state
	EXPECT_TRUE(
	    rules::MobileStaticIsObstacle(MobileStaticInfo::Boulder1Chalk, MobileStaticInfo::Rock, MeshId::Dummy, 0.0f, 0.0f));
	EXPECT_TRUE(rules::MobileStaticIsObstacle(MobileStaticInfo::Idol, MobileStaticInfo::Idol, MeshId::Dummy, 0.0f, 0.0f));
	// the statics as heavy as rocks, fences and toys
	EXPECT_TRUE(rules::MobileStaticIsObstacle(MobileStaticInfo::GateTotemApe, none, MeshId::Dummy, 0.0f, 0.0f));
	EXPECT_TRUE(rules::MobileStaticIsObstacle(MobileStaticInfo::WeepingStone, none, MeshId::Dummy, 0.0f, 0.0f));
	EXPECT_TRUE(
	    rules::MobileStaticIsObstacle(MobileStaticInfo::Boulder1Chalk, none, MeshId::BuildingCelticFenceShort, 0.0f, 0.0f));
	EXPECT_TRUE(rules::MobileStaticIsObstacle(MobileStaticInfo::Boulder1Chalk, none, MeshId::ObjectToyDice, 0.0f, 0.0f));
	// lanterns and the bonfire never; the singing stone's base always
	EXPECT_FALSE(rules::MobileStaticIsObstacle(MobileStaticInfo::StreetLantern, none, MeshId::Dummy, 1.0f, 1.0f));
	EXPECT_FALSE(rules::MobileStaticIsObstacle(MobileStaticInfo::CountryLantern, none, MeshId::Dummy, 1.0f, 1.0f));
	EXPECT_FALSE(rules::MobileStaticIsObstacle(MobileStaticInfo::Bonfire, none, MeshId::Dummy, 1.0f, 1.0f));
	EXPECT_TRUE(rules::MobileStaticIsObstacle(MobileStaticInfo::SingingStoneBase, none, MeshId::Dummy, 0.0f, 0.0f));
	// anything else as a building is
	EXPECT_TRUE(rules::MobileStaticIsObstacle(MobileStaticInfo::Boulder1Chalk, none, MeshId::Dummy, 1.0f, 1.0f));
	EXPECT_FALSE(rules::MobileStaticIsObstacle(MobileStaticInfo::Boulder1Chalk, none, MeshId::Dummy, 1.0f, 0.0f));
}

TEST(PhysicsClassRules, plainObjectStaticsNeverFly)
{
	EXPECT_FALSE(rules::MobileStaticCanBecomePhysicsObject(MobileStaticInfo::StreetLantern));
	EXPECT_FALSE(rules::MobileStaticCanBecomePhysicsObject(MobileStaticInfo::CountryLantern));
	EXPECT_FALSE(rules::MobileStaticCanBecomePhysicsObject(MobileStaticInfo::SingingStoneBase));
	EXPECT_FALSE(rules::MobileStaticCanBecomePhysicsObject(MobileStaticInfo::Bonfire));
	EXPECT_TRUE(rules::MobileStaticCanBecomePhysicsObject(MobileStaticInfo::SingingStone_1));
	EXPECT_TRUE(rules::MobileStaticCanBecomePhysicsObject(MobileStaticInfo::Boulder1Chalk));
}

TEST(PhysicsClassRules, mobileObjects)
{
	EXPECT_TRUE(rules::MobileObjectIsObstacle(MobileObjectInfo::Whale));
	EXPECT_FALSE(rules::MobileObjectCanBecomePhysicsObject(MobileObjectInfo::Whale));
	EXPECT_FALSE(rules::MobileObjectIsObstacle(MobileObjectInfo::Creed));
	EXPECT_FALSE(rules::MobileObjectCanBecomePhysicsObject(MobileObjectInfo::Creed));
	EXPECT_FALSE(rules::MobileObjectCanBecomePhysicsObject(MobileObjectInfo::HanoiPuzzleBase));
	EXPECT_TRUE(rules::MobileObjectCanBecomePhysicsObject(MobileObjectInfo::HanoiPuzzlePart1));
	EXPECT_TRUE(rules::MobileObjectCanBecomePhysicsObject(MobileObjectInfo::Champi));
}

TEST(PhysicsClassRules, animatedStatics)
{
	EXPECT_TRUE(rules::AnimatedStaticIsObstacle(AnimatedStaticInfo::NorseGate));
	EXPECT_TRUE(rules::AnimatedStaticIsObstacle(AnimatedStaticInfo::GateStonePlinth));
	EXPECT_TRUE(rules::AnimatedStaticIsObstacle(AnimatedStaticInfo::PiperCaveEntrance));
	EXPECT_TRUE(rules::AnimatedStaticIsObstacle(AnimatedStaticInfo::PhoneBox));
	EXPECT_FALSE(rules::AnimatedStaticIsObstacle(AnimatedStaticInfo::ChessKingTeamA));
	EXPECT_FALSE(rules::AnimatedStaticIsObstacle(AnimatedStaticInfo::None));

	EXPECT_EQ(rules::AnimatedStaticCollisionMesh(AnimatedStaticInfo::NorseGate, 0, 0, 0), MeshId::NorseGatePhys1);
	EXPECT_EQ(rules::AnimatedStaticCollisionMesh(AnimatedStaticInfo::NorseGate, 1, 0, 0), MeshId::NorseGatePhys2);
	EXPECT_EQ(rules::AnimatedStaticCollisionMesh(AnimatedStaticInfo::GateStonePlinth, 0, 0, 0), MeshId::GateTotemPlinthePhys1);
	EXPECT_EQ(rules::AnimatedStaticCollisionMesh(AnimatedStaticInfo::GateStonePlinth, 0, 1, 0), MeshId::GateTotemPlinthePhys2);
	EXPECT_EQ(rules::AnimatedStaticCollisionMesh(AnimatedStaticInfo::GateStonePlinth, 0, 1, 1), MeshId::GateTotemPlinthePhys3);
	// an open plinth is empty again
	EXPECT_EQ(rules::AnimatedStaticCollisionMesh(AnimatedStaticInfo::GateStonePlinth, 1, 1, 1), MeshId::GateTotemPlinthePhys1);
	EXPECT_EQ(rules::AnimatedStaticCollisionMesh(AnimatedStaticInfo::PhoneBox, 0, 0, 0), MeshId::GateTotemPlinthePhys1);
	EXPECT_EQ(rules::AnimatedStaticCollisionMesh(AnimatedStaticInfo::PiperCaveEntrance, 0, 0, 0), MeshId::PiperEntrancePhys1);
	EXPECT_FALSE(rules::AnimatedStaticCollisionMesh(AnimatedStaticInfo::ChessPionTeamA, 0, 0, 0).has_value());
}
