/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstdio>

#include <array>
#include <bit>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "Common/GameHeading.h"
#include "Creature/CreatureEyes.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureSpellCasting.h"
#include "Creature/CreatureThrow.h"
#include "OracleData.h"

using namespace openblack;

namespace
{
std::string HexBits(uint32_t bits)
{
	std::array<char, 16> text {};
	std::snprintf(text.data(), text.size(), "0x%08X", bits);
	return text.data();
}

/// Loads a table and finds its columns. For each row, the function gives our values for the result columns, from the
/// values of the input columns. The test compares their bits with the bits of the game.
void ExpectTheGame(std::string_view file, const std::vector<std::string>& inputs, const std::vector<std::string>& results,
                   const std::function<std::vector<uint32_t>(const std::vector<uint32_t>&)>& ours)
{
	const auto table = oracle::Table::Load(std::string(file));
	ASSERT_TRUE(table.has_value());
	const auto find = [&table](const std::vector<std::string>& names) {
		std::vector<size_t> columns;
		for (const auto& name : names)
		{
			const auto column = table->Column(name);
			EXPECT_TRUE(column.has_value()) << name;
			columns.push_back(column.value_or(0));
		}
		return columns;
	};
	const auto in = find(inputs);
	const auto out = find(results);
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
					ADD_FAILURE() << file << " row " << row << " " << results[i] << ": the game gives " << HexBits(game)
					              << ", ours gives " << HexBits(result[i]);
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

glm::vec2 Point(uint32_t x, uint32_t z)
{
	return {F(x), F(z)};
}

glm::vec2 Ahead(uint32_t heading)
{
	return creature_locomotion::DirectionOf(GameHeading(F(heading)));
}
} // namespace

// The turns that a knocked-out creature lies are the game's turns
TEST(OracleCreatureMaths2, TheTurnsOfAFaintAreTheTurnsOfTheGame)
{
	ExpectTheGame("creature_faint_turns.txt", {"size"}, {"turns"}, [](const std::vector<uint32_t>& v) {
		return std::vector<uint32_t> {creature_fight::FaintWaitTurns(F(v[0]), 100)};
	});
}

// The life after a fight has the bits of the game
TEST(OracleCreatureMaths2, TheLifeAfterAFightHasTheBitsOfTheGame)
{
	ExpectTheGame("creature_life_after_fight.txt", {"life", "health"}, {"new_life"}, [](const std::vector<uint32_t>& v) {
		return std::vector<uint32_t> {Bits(creature_fight::LifeAfterFight(F(v[0]), F(v[1])))};
	});
}

// The push apart on a blow puts both fighters at the bits of the game
TEST(OracleCreatureMaths2, ThePushOfABlowHasTheBitsOfTheGame)
{
	ExpectTheGame("creature_blow_push.txt", {"x", "z", "other_x", "other_z", "heading", "reach", "other_reach"},
	              {"new_x", "new_z", "new_other_x", "new_other_z", "moved"}, [](const std::vector<uint32_t>& v) {
		              const auto striker = Point(v[0], v[1]);
		              const auto opponent = Point(v[2], v[3]);
		              const auto pushed = creature_fight::BlowPushOf(striker, opponent, Ahead(v[4]), F(v[5]), F(v[6]));
		              const auto first = pushed.first.value_or(striker);
		              const auto second = pushed.second.value_or(opponent);
		              return std::vector<uint32_t> {Bits(first.x), Bits(first.y), Bits(second.x), Bits(second.y),
		                                            pushed.first.has_value() ? 1u : 0u};
	              });
}

// The pull apart in the stance puts both fighters at the bits of the game, and moves the same fighters
TEST(OracleCreatureMaths2, ThePullApartHasTheBitsOfTheGame)
{
	ExpectTheGame("creature_pull_apart.txt",
	              {"x", "z", "other_x", "other_z", "heading", "reach", "other_reach", "centre_x", "centre_z", "radius"},
	              {"new_x", "new_z", "moved", "new_other_x", "new_other_z", "other_moved"}, [](const std::vector<uint32_t>& v) {
		              const creature_fight::Arena arena {.centre = Point(v[7], v[8]), .radius = F(v[9])};
		              // The reach of the first fighter comes first in the sum
		              const auto pulled = creature_fight::PullApartOf(arena, Point(v[0], v[1]), Point(v[2], v[3]), Ahead(v[4]),
		                                                              F(v[5]) + F(v[6]));
		              // The game keeps a fighter that does not move where it is. The data then has the zero of our stub.
		              const auto first = pulled.first.value_or(glm::vec2(0.0f));
		              const auto second = pulled.second.value_or(glm::vec2(0.0f));
		              return std::vector<uint32_t> {Bits(first.x),  Bits(first.y),  pulled.first.has_value() ? 1u : 0u,
		                                            Bits(second.x), Bits(second.y), pulled.second.has_value() ? 1u : 0u};
	              });
}

// The wait before the next stroke is the game's wait in whole milliseconds
TEST(OracleCreatureMaths2, TheWaitForTheNextStrokeIsTheWaitOfTheGame)
{
	ExpectTheGame("creature_stroke_wait.txt", {"sum"}, {"wait"}, [](const std::vector<uint32_t>& v) {
		return std::vector<uint32_t> {static_cast<uint32_t>(static_cast<int32_t>(creature_feedback::StrokeWaitMs(F(v[0]))))};
	});
}

// The sum of the feedback after a slap has the bits of the game
TEST(OracleCreatureMaths2, TheSumAfterASlapHasTheBitsOfTheGame)
{
	ExpectTheGame("creature_slap_sum.txt", {"sum", "hard"}, {"new_sum"}, [](const std::vector<uint32_t>& v) {
		return std::vector<uint32_t> {Bits(creature_feedback::AfterSlap(F(v[0]), v[1] == 0))};
	});
}

// The sum of the feedback after a stroke has the bits of the game
TEST(OracleCreatureMaths2, TheSumAfterAStrokeHasTheBitsOfTheGame)
{
	ExpectTheGame("creature_stroke_sum.txt", {"sum"}, {"new_sum"}, [](const std::vector<uint32_t>& v) {
		return std::vector<uint32_t> {Bits(creature_feedback::AfterStroke(F(v[0])))};
	});
}

// The openness of the eyes after the creature stops an action has the bits of the game, also for a value that is not a
// number
TEST(OracleCreatureMaths2, TheRestedOpennessHasTheBitsOfTheGame)
{
	ExpectTheGame("creature_rested_openness.txt", {"exhaustion"}, {"openness"}, [](const std::vector<uint32_t>& v) {
		return std::vector<uint32_t> {Bits(creature_eyes::RestedOpenness(F(v[0])))};
	});
}

// The time that the eyes take to turn has the bits of the game, also for stoned eyes
TEST(OracleCreatureMaths2, TheLookTimeOfTheEyesHasTheBitsOfTheGame)
{
	ExpectTheGame("creature_eye_look_seconds.txt", {"openness", "stoned"}, {"seconds"}, [](const std::vector<uint32_t>& v) {
		const auto mode = v[1] != 0 ? creature_eyes::Mode::Stoned : creature_eyes::Mode::Calm;
		return std::vector<uint32_t> {Bits(creature_eyes::LookSeconds(F(v[0]), mode))};
	});
}

// The speed of a thrown thing has the bits of the game
TEST(OracleCreatureMaths2, TheSpeedOfAThrowHasTheBitsOfTheGame)
{
	ExpectTheGame("creature_throw_velocity.txt",
	              {"target_x", "target_y", "target_z", "release_x", "release_y", "release_z", "seconds"},
	              {"speed_x", "speed_y", "speed_z"}, [](const std::vector<uint32_t>& v) {
		              const auto speed =
		                  creature_throw::ReleaseVelocity({F(v[0]), F(v[1]), F(v[2])}, {F(v[3]), F(v[4]), F(v[5])}, F(v[6]));
		              return std::vector<uint32_t> {Bits(speed.x), Bits(speed.y), Bits(speed.z)};
	              });
}

// The stamina that a miracle costs in a fight has the bits of the game, also for a cost that is not a number
TEST(OracleCreatureMaths2, TheStaminaOfACastHasTheBitsOfTheGame)
{
	ExpectTheGame("creature_cast_stamina.txt", {"cost"}, {"stamina"}, [](const std::vector<uint32_t>& v) {
		return std::vector<uint32_t> {Bits(creature_spell_casting::StaminaCost(F(v[0])))};
	});
}

// The magnitude of a cast at a thing, at a creature and of a fire miracle has the bits of the game
TEST(OracleCreatureMaths2, TheMagnitudeOfACastHasTheBitsOfTheGame)
{
	ExpectTheGame("creature_cast_magnitude.txt", {"kind", "value"}, {"magnitude"}, [](const std::vector<uint32_t>& v) {
		const auto value = F(v[1]);
		const auto magnitude = v[0] == 0   ? creature_spell_casting::CastMagnitude(value, std::nullopt, false, 0.0f)
		                       : v[0] == 1 ? creature_spell_casting::CastMagnitude(0.0f, value, false, 0.0f)
		                                   : creature_spell_casting::CastMagnitude(0.0f, std::nullopt, true, value);
		return std::vector<uint32_t> {Bits(magnitude)};
	});
}
