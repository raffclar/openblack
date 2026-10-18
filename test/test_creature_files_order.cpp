/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <array>
#include <bit>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Common/GameHeading.h"
#include "Creature/CreatureAnimation.h"
#include "Creature/CreatureFart.h"
#include "Creature/CreatureGiving.h"
#include "Creature/CreatureHeadLook.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureMarks.h"
#include "Creature/CreaturePhysiology.h"
#include "Creature/CreatureRestAndHome.h"
#include "Creature/CreatureStomp.h"
#include "Creature/TattooEditor.h"

// Each case is worked from the game's instructions with one float step a time: the order of the sums, the float
// constants, and the FPU's sine and cosine before they are rounded. The inputs are cases where the order of the
// sums changes the bits, so a sum in another order fails the test.

using namespace openblack;

namespace
{
constexpr float Bits(uint32_t bits)
{
	return std::bit_cast<float>(bits);
}

uint32_t BitsOf(float value)
{
	return std::bit_cast<uint32_t>(value);
}

void ExpectBits(const glm::vec3& actual, const glm::vec3& expected)
{
	EXPECT_EQ(BitsOf(actual.x), BitsOf(expected.x));
	EXPECT_EQ(BitsOf(actual.y), BitsOf(expected.y));
	EXPECT_EQ(BitsOf(actual.z), BitsOf(expected.z));
}
} // namespace

// The hand calls the giving off when it is further than 15 sizes away. The distance adds the squares from z to x: the
// hand here is exactly at the limit, and the sum from x to z would put it past the limit.
TEST(CreatureFilesOrder, TheGivingIsCalledOffByTheDistanceFromZToX)
{
	const glm::vec3 target {Bits(0x3FA742D3u), Bits(0xC10DB189u), Bits(0xC1112331u)};
	EXPECT_FALSE(creature_giving::CallsOff(glm::vec3(0.0f), target, Bits(0x3F5980DFu)));
}

// The head's pitch: the unit direction adds the squares of y, z, then x, and the arcsine is the C library's
TEST(CreatureFilesOrder, TheHeadPitchIsTheGamesArcsineOfTheUnitDirection)
{
	const glm::vec3 target {Bits(0x41A98792u), Bits(0xC06142ECu), Bits(0x40736BA4u)};
	const auto angles = creature_head_look::AnglesTowards(glm::vec3(0.0f), GameHeading(0.0f), target);
	EXPECT_EQ(BitsOf(angles.pitch), 0xBE25EFB4u);
}

// The stop on the way into the temple: the squares of y, x, then z, the heart's route radius before the 1.3, and the
// way to the heart as the offset times one over the length
TEST(CreatureFilesOrder, TheTempleStopIsInTheGamesOrder)
{
	const glm::vec3 standing {Bits(0x44D3CFD5u), Bits(0x41B5A5B7u), Bits(0x43C11074u)};
	const glm::vec3 heart {Bits(0x4444820Au), Bits(0x42B48380u), Bits(0x44E8830Du)};
	const auto stop = creature_mind::TempleStop(standing, heart, Bits(0x40A298F5u));
	EXPECT_EQ(BitsOf(stop.x), 0x4444CB2Fu);
	EXPECT_EQ(BitsOf(stop.y), 0x44E847B6u);
}

// A part of the body knocks only when its move is more than three quarters of the size. The squares add x, z, then
// y: this move is exactly at the limit, and the sum from x to z would put it past the limit.
TEST(CreatureFilesOrder, TheMoveOfAPartAddsItsSquaresXZY)
{
	const glm::vec3 move {Bits(0xBFD15EEBu), Bits(0x3FCE7451u), Bits(0xBF7E5462u)};
	EXPECT_FALSE(creature_stomp::MovedEnough(glm::vec3(0.0f), move, Bits(0x405592BDu)));
}

// The blow on a building: the unit move from the x, z, y length, at the length times 10 times 0.4
TEST(CreatureFilesOrder, TheBlowOnABuildingIsInTheGamesOrder)
{
	const glm::vec3 move {Bits(0x3F7C160Cu), Bits(0xBEC7E1B2u), Bits(0x402B618Du)};
	const auto blow = creature_stomp::TrampleBlow(glm::vec3(1.0f), move, 2.0f);
	ExpectBits(blow.velocity, {Bits(0x407C160Eu), Bits(0xBFC7E1B4u), Bits(0x412B618Fu)});
	const auto still = creature_stomp::TrampleBlow(glm::vec3(1.0f), glm::vec3(0.0f), 2.0f);
	ExpectBits(still.velocity, glm::vec3(0.0f));
}

// The reach on the ground compares the squares: a thing at exactly the reach is out of it
TEST(CreatureFilesOrder, TheReachOnTheGroundComparesSquares)
{
	using creature_stomp::Victim;
	// The building's reach is 3 + 1 = 4, the living's reach is 1 + 3 x 0.3
	EXPECT_FALSE(creature_stomp::InReach(Victim::Building, 16.0f, 3.0f, 1.0f));
	EXPECT_TRUE(creature_stomp::InReach(Victim::Building, std::nextafter(16.0f, 0.0f), 3.0f, 1.0f));
	const float living = 1.0f + (3.0f * creature_stomp::k_KnockReachShare);
	EXPECT_FALSE(creature_stomp::InReach(Victim::Living, living * living, 3.0f, 1.0f));
	EXPECT_TRUE(creature_stomp::InReach(Victim::Living, std::nextafter(living * living, 0.0f), 3.0f, 1.0f));
}

// The tattoo site faces the camera by a dot product of two unit directions, each sum from z to x
TEST(CreatureFilesOrder, TheTattooSiteFacingAddsFromZToX)
{
	const std::array<glm::vec3, 3> corners {
	    glm::vec3 {Bits(0xBEDEB47Du), Bits(0xBF268983u), Bits(0xBF242079u)},
	    glm::vec3 {Bits(0x3F370BE6u), Bits(0xBF37950Bu), Bits(0xBF2AAD3Fu)},
	    glm::vec3 {Bits(0xBF3ECA38u), Bits(0x3F20A18Eu), Bits(0xBEEC6B3Au)},
	};
	const glm::vec3 place {Bits(0xBF7A419Cu), Bits(0xBDC3CD6Bu), Bits(0x3F2182F4u)};
	const glm::vec3 camera {Bits(0xC06E2103u), Bits(0x40EA6A17u), Bits(0xC09FB008u)};
	EXPECT_EQ(BitsOf(creature_tattoo_editor::FacingCamera(corners, place, camera)), 0xBF2E741Au);
}

// The editor's orbit turns the offset across with the FPU's sine before it is rounded
TEST(CreatureFilesOrder, TheTattooOrbitTurnsWithTheWideSine)
{
	creature_tattoo_editor::Orbit orbit {
	    .eye = {Bits(0x4134DDA3u), Bits(0x41CFC3BAu), Bits(0x41F820F9u)},
	    .focus = {Bits(0xBE391B05u), Bits(0x3FF7A62Du), Bits(0x3F1C5C00u)},
	    .turnSpeed = Bits(0xC0219F5Fu),
	    .tipSpeed = Bits(0x3ABB2006u),
	};
	creature_tattoo_editor::Turn(orbit, 50.0f);
	ExpectBits(orbit.eye, {Bits(0x41472862u), Bits(0x41CFC527u), Bits(0x41F47988u)});
}

// The blended move is as long as the blend of the lengths, each length a sum from z to x
TEST(CreatureFilesOrder, TheBlendedMoveAddsFromZToX)
{
	const glm::vec3 from {Bits(0xBDF55F76u), Bits(0x3F40F907u), Bits(0x4095E8DAu)};
	const glm::vec3 to {Bits(0x4060C4A2u), Bits(0xBEE82D7Cu), Bits(0x3EE769ADu)};
	ExpectBits(creature_animation::BlendMoves(from, to, Bits(0x3F57A24Fu)),
	           {Bits(0x405FD3E0u), Bits(0xBEA048C8u), Bits(0x3FAA7B04u)});
}

// The walk's breath period divides by the inverse of the square root of the size
TEST(CreatureFilesOrder, TheWalkBreathDividesByTheInverseRoot)
{
	EXPECT_EQ(BitsOf(creature_animation::WalkBreathPeriod(Bits(0x3F626A1Bu), false)), 0x4034909Du);
}

// The direction of a heading: the FPU's sine and cosine, each rounded to a float
TEST(CreatureFilesOrder, TheDirectionOfAHeadingIsTheRoundedWideSineAndCosine)
{
	const auto direction = creature_locomotion::DirectionOf(GameHeading(Bits(0x3FEAF9BAu)));
	EXPECT_EQ(BitsOf(direction.x), 0x3F77112Au);
	EXPECT_EQ(BitsOf(direction.y), 0x3E86129Du);
}

// The lump of a poo goes back along the heading turned by a half turn. The FPU's wide sine and cosine go into the
// products with 5 and -5: a sine rounded to a float first gives other bits on both parts here.
TEST(CreatureFilesOrder, ThePooLumpGoesBackWithTheWideSineAndCosine)
{
	const auto lump = creature_physiology::PooLumpOf(glm::vec3(0.0f), GameHeading(Bits(0x3D3F44BBu)), 1.0f);
	ExpectBits(lump.velocity, {Bits(0xBE6EFFD0u), 0.0f, Bits(0x409FD35Au)});
}

// The puffs of a fart go back along the heading turned by a half turn, by the spread (size x 15). With every draw at
// half its limit, each puff moves only by that back way: the wide sine and cosine go into the products with the spread.
TEST(CreatureFilesOrder, TheFartPuffsGoBackWithTheWideSineAndCosine)
{
	const auto random = [](float limit) { return limit * 0.5f; };
	const auto velocities = creature_fart::PuffVelocities(GameHeading(Bits(0xBFEA1B99u)), Bits(0x3F67DFC1u), random);
	for (const auto& velocity : velocities)
	{
		ExpectBits(velocity, {Bits(0x41522D75u), 0.0f, Bits(0xC05E004Eu)});
	}
}

// A burn's line starts at the groin plus a draw less half the range of the draw. The draw is added to the groin
// first: the draw less the eighth first gives other bits here.
TEST(CreatureFilesOrder, TheBurnPointAddsTheDrawToTheGroinFirst)
{
	const float groin = Bits(0xC3CF0EC3u);
	const float reach = Bits(0x4109399Eu);
	const auto draw = [](float) { return Bits(0x3F3C12AAu); };
	const auto from = creature_marks::scar::BurnFrom(glm::vec3(groin), reach, draw);
	EXPECT_EQ(BitsOf(from.x), 0xC3CF39F4u);
	EXPECT_EQ(BitsOf(from.z), 0xC3CF39F4u);
}

// A kick's second blow on a building goes sideways from the heading by the side draw: (draw - 5) x 0.1 of a quarter
// turn. Its velocity is the wide sine x 2 and the wide cosine x -2, each rounded one time.
TEST(CreatureFilesOrder, TheSideBlowOfAKickUsesTheHeading)
{
	const creature_stomp::BuildingKick kick {
	    .creature = glm::vec3(0.0f),
	    .foot = glm::vec3(0.0f),
	    .direction = {0.0f, 0.6f, -0.8f},
	    .heading = GameHeading(Bits(0x3F1A2B3Cu)),
	    .creatureSize = 1.0f,
	    .buildingRadius = 10.0f,
	    .sideDraw = 7,
	    .firstHeightDraw = 0,
	    .secondHeightDraw = 0,
	};
	const auto blows = creature_stomp::KickBlows(kick, 0.0f);
	ExpectBits(blows[1].velocity, {Bits(0x3FCB1C8Au), 0.0f, Bits(0xBF9BD371u)});
	// The first blow goes the way of the kick at twice its length
	ExpectBits(blows[0].velocity, {0.0f, 1.2f, -1.6f});
}
