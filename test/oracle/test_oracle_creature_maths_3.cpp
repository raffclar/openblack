/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Common/GameHeading.h"
#include "Creature/CreatureHair.h"
#include "Creature/CreatureLook.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreaturePointing.h"
#include "Creature/CreatureThrow.h"
#include "OracleData.h"

using namespace openblack;

namespace
{
/// Loads a table and finds its columns. For each row, the function gives the bits of our results, from the values of
/// the input columns. The test compares them with the result columns of the game, in the same order.
void ExpectTheBitsOfTheGame(std::string_view file, const std::vector<std::string>& inputs,
                            const std::vector<std::string>& results,
                            const std::function<std::vector<uint32_t>(const std::vector<uint32_t>&)>& ours)
{
	const auto table = oracle::Table::Load(std::string(file));
	ASSERT_TRUE(table.has_value()) << file;
	std::vector<size_t> in;
	std::vector<size_t> out;
	for (const auto& name : inputs)
	{
		const auto column = table->Column(name);
		ASSERT_TRUE(column.has_value()) << name;
		in.push_back(*column);
	}
	for (const auto& name : results)
	{
		const auto column = table->Column(name);
		ASSERT_TRUE(column.has_value()) << name;
		out.push_back(*column);
	}
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		std::vector<uint32_t> values;
		for (const auto column : in)
		{
			values.push_back(table->Bits(row, column));
		}
		const auto result = ours(values);
		ASSERT_EQ(result.size(), out.size());
		for (size_t i = 0; i < out.size(); ++i)
		{
			const auto game = table->Bits(row, out[i]);
			if (result[i] != game)
			{
				++different;
				if (different <= 20)
				{
					ADD_FAILURE() << file << " row " << row << " " << results[i] << ": the game gives "
					              << oracle::Hex(std::bit_cast<float>(game)) << ", ours gives "
					              << oracle::Hex(std::bit_cast<float>(result[i]));
				}
			}
		}
	}
	EXPECT_EQ(different, 0u);
}

float F(uint32_t bits)
{
	return std::bit_cast<float>(bits);
}

uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}

glm::vec3 Vec(const std::vector<uint32_t>& v, size_t first)
{
	return {F(v[first]), F(v[first + 1]), F(v[first + 2])};
}
} // namespace

// The aim of a point and the weights of the four point clips have the bits of the game
TEST(OracleCreatureMaths3, TheAimOfAPointHasTheBitsOfTheGame)
{
	ExpectTheBitsOfTheGame("creature_point_aim.txt", {"x", "y", "z", "size", "yaw", "tx", "ty", "tz", "mirrored"},
	                       {"right", "high", "w0", "w1", "w2", "w3"}, [](const std::vector<uint32_t>& v) {
		                       const auto place = Vec(v, 0);
		                       const auto origin =
		                           place + glm::vec3(0.0f, creature_pointing::k_OriginHeightPerSize * F(v[3]), 0.0f);
		                       const auto aim = creature_pointing::AimAt(origin, GameHeading(F(v[4])), Vec(v, 5));
		                       const auto mirrored = v[8] != 0;
		                       const auto weights = creature_pointing::Weights(aim, mirrored);
		                       const auto across = mirrored ? -aim.across : aim.across;
		                       const auto right =
		                           (across + creature_pointing::k_AcrossLimit) * creature_pointing::k_AcrossScale;
		                       const auto high = aim.rise + creature_pointing::k_RiseLimit;
		                       return std::vector<uint32_t> {Bits(right),      Bits(high),       Bits(weights[0]),
		                                                     Bits(weights[1]), Bits(weights[2]), Bits(weights[3])};
	                       });
}

// The angle from the heading to a point, as seen from the head, has the bits of the game
TEST(OracleCreatureMaths3, TheTurnBeforeAPointHasTheBitsOfTheGame)
{
	ExpectTheBitsOfTheGame(
	    "creature_point_turn.txt", {"hx", "hy", "hz", "yaw", "tx", "ty", "tz"}, {"angle"}, [](const std::vector<uint32_t>& v) {
		    return std::vector<uint32_t> {Bits(creature_pointing::TurnAngle(Vec(v, 0), GameHeading(F(v[3])), Vec(v, 4)))};
	    });
}

namespace
{
creature_throw::HurlHands HandsOf(const std::vector<uint32_t>& v)
{
	return {.flat = Vec(v, 0), .high = Vec(v, 3), .flatSpeed = F(v[6]), .highSpeed = F(v[7])};
}
} // namespace

// The speed of the hand in the flat and the high throw has the bits of the game
TEST(OracleCreatureMaths3, TheSpeedOfTheThrowHandHasTheBitsOfTheGame)
{
	ExpectTheBitsOfTheGame("creature_hurl_hand_speed.txt", {"ax", "ay", "az", "bx", "by", "bz"}, {"speed"},
	                       [](const std::vector<uint32_t>& v) {
		                       return std::vector<uint32_t> {Bits(creature_throw::HurlHandSpeed(Vec(v, 0), Vec(v, 3)))};
	                       });
}

// The point where the hand lets go of a thrown thing has the bits of the game
TEST(OracleCreatureMaths3, TheReleasePointOfAThrowHasTheBitsOfTheGame)
{
	ExpectTheBitsOfTheGame(
	    "creature_hurl_release.txt",
	    {"fx", "fy", "fz", "hx", "hy", "hz", "fs", "hs", "x", "y", "z", "scale", "mirrored", "heading", "slope"},
	    {"rx", "ry", "rz"}, [](const std::vector<uint32_t>& v) {
		    const auto point =
		        creature_throw::HurlReleasePoint(HandsOf(v), Vec(v, 8), GameHeading(F(v[13])), F(v[11]), v[12] != 0, F(v[14]));
		    return std::vector<uint32_t> {Bits(point.x), Bits(point.y), Bits(point.z)};
	    });
}

// The speed along the ground and the height speed at the start of a throw have the bits of the game
TEST(OracleCreatureMaths3, TheLiftOfAThrowHasTheBitsOfTheGame)
{
	ExpectTheBitsOfTheGame(
	    "creature_hurl_lift.txt",
	    {"fx", "fy", "fz", "hx", "hy", "hz", "fs", "hs", "x", "y", "z", "scale", "mirrored", "tx", "ty", "tz", "seconds"},
	    {"speed", "rise"}, [](const std::vector<uint32_t>& v) {
		    const auto lift = creature_throw::LiftOf(HandsOf(v), Vec(v, 8), F(v[11]), v[12] != 0, Vec(v, 13), F(v[16]));
		    return std::vector<uint32_t> {Bits(lift.speed), Bits(lift.rise)};
	    });
}

// The weights of the two throw clips and the step of a turn have the bits of the game
TEST(OracleCreatureMaths3, TheBlendAndTheStepOfAThrowHaveTheBitsOfTheGame)
{
	ExpectTheBitsOfTheGame(
	    "creature_hurl_step.txt", {"fs", "hs", "speed", "rise", "scale", "turn_ms", "clip_step", "time", "throw_ms"},
	    {"w91", "w92", "step"}, [](const std::vector<uint32_t>& v) {
		    const creature_throw::HurlHands hands {.flatSpeed = F(v[0]), .highSpeed = F(v[1])};
		    const creature_throw::HurlLift lift {.speed = F(v[2]), .rise = F(v[3])};
		    const auto high = creature_throw::HurlHighWeight(hands, lift);
		    const auto step =
		        creature_throw::HurlStepMs(hands, lift, high, F(v[4]), static_cast<int32_t>(v[5]), static_cast<int32_t>(v[6]),
		                                   static_cast<int32_t>(v[7]), static_cast<int32_t>(v[8]));
		    return std::vector<uint32_t> {Bits(1.0f - high), Bits(high), static_cast<uint32_t>(step)};
	    });
}

// How much a thing that the eye sweep meets catches the creature's eye has the bits of the game
TEST(OracleCreatureMaths3, TheCatchOfASightHasTheBitsOfTheGame)
{
	ExpectTheBitsOfTheGame("creature_look_catch.txt", {"size", "weight", "interest", "distance", "turns", "ms"}, {"score"},
	                       [](const std::vector<uint32_t>& v) {
		                       const creature_look::Target target {.watchedTurns = v[4]};
		                       const auto range = creature_look::LookRange(F(v[0]));
		                       return std::vector<uint32_t> {
		                           Bits(creature_look::Catches(target, F(v[1]), F(v[2]), F(v[3]), range, 1000 / v[5]))};
	                       });
}

// How much the watched sight holds the creature's eye has the bits of the game, also for a distance that is not a number
TEST(OracleCreatureMaths3, TheHoldOfTheWatchedSightHasTheBitsOfTheGame)
{
	ExpectTheBitsOfTheGame("creature_look_hold.txt", {"size", "kind", "weight", "interest", "distance"}, {"hold"},
	                       [](const std::vector<uint32_t>& v) {
		                       const creature_look::Target target {
		                           .id = 1, .sight = static_cast<creature_look::Sight>(v[1]), .interest = F(v[3])};
		                       const auto range = creature_look::LookRange(F(v[0]));
		                       return std::vector<uint32_t> {Bits(creature_look::Hold(target, F(v[4]), range, F(v[2])))};
	                       });
}

// The scale of a creature's hair has the bits of the game, also for a size that is not above 0 or is not a number
TEST(OracleCreatureMaths3, TheScaleOfTheHairHasTheBitsOfTheGame)
{
	ExpectTheBitsOfTheGame("creature_hair_scale.txt", {"size"}, {"scale"}, [](const std::vector<uint32_t>& v) {
		return std::vector<uint32_t> {Bits(creature_hair::HairScale(F(v[0])))};
	});
}

// The seconds of the hair step are the milliseconds of the frame times 0.001, as the game makes them
TEST(OracleCreatureMaths3, TheSecondsOfTheHairStepHaveTheBitsOfTheGame)
{
	ExpectTheBitsOfTheGame("creature_hair_seconds.txt", {"ms"}, {"seconds"}, [](const std::vector<uint32_t>& v) {
		return std::vector<uint32_t> {Bits(creature_hair::StepSeconds(static_cast<float>(v[0])))};
	});
}

// The kept size of a creature and its drawn scale have the bits of the game, also for a size that is not a number
TEST(OracleCreatureMaths3, TheKeptSizeAndTheDrawnScaleHaveTheBitsOfTheGame)
{
	ExpectTheBitsOfTheGame("creature_size_scale.txt", {"size", "rest"}, {"kept", "scale"}, [](const std::vector<uint32_t>& v) {
		const auto kept = creature_morph::ClampScale(F(v[0]));
		return std::vector<uint32_t> {Bits(kept), Bits(creature_morph::DrawnScale(kept, F(v[1])))};
	});
}

// The velocity of a thing that a creature lets go of has the bits of the game
TEST(OracleCreatureMaths3, TheVelocityOfALetGoThingHasTheBitsOfTheGame)
{
	ExpectTheBitsOfTheGame("creature_toss_velocity.txt", {"yaw", "mirrored", "vx", "vy", "vz", "scale"}, {"x", "y", "z"},
	                       [](const std::vector<uint32_t>& v) {
		                       const auto velocity = creature_throw::TossVelocity(Vec(v, 2), v[1] != 0, GameHeading(F(v[0])),
		                                                                          F(v[5]), creature_throw::k_DiscardSpeedShare);
		                       return std::vector<uint32_t> {Bits(velocity.x), Bits(velocity.y), Bits(velocity.z)};
	                       });
}
