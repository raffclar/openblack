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

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureThrow.h"

// Bit tests worked from the game's instructions with exact float steps (scratchpad cfo3/bits.py). Each input gives other
// bits when the old rule runs.

using namespace openblack;

namespace
{
glm::vec3 FromBits(uint32_t x, uint32_t y, uint32_t z)
{
	return {std::bit_cast<float>(x), std::bit_cast<float>(y), std::bit_cast<float>(z)};
}
} // namespace

// The flight time multiplies the ground distance by one over g as the game keeps it. A division by g gives one more bit
TEST(CreatureFilesOrder3, TheFlightTimeMultipliesByTheGamesOneOverG)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(creature_throw::k_FlightTimePerUnit), 0x3DD0C43Du);
	const auto creature = FromBits(0x452FB0A1u, 0x44D89773u, 0x44EB171Au);
	const auto target = FromBits(0x450B7571u, 0x433BC16Eu, 0x44E25CF3u);
	EXPECT_EQ(std::bit_cast<uint32_t>(creature_throw::FlightTime(creature, target)), 0x40F6E079u);
}

// The skin test's way is the point one unit along the line of sight less the camera, not the unit way itself
TEST(CreatureFilesOrder3, TheSightWayIsThePointOneUnitAlongLessTheCamera)
{
	const auto camera = FromBits(0x4512C6E8u, 0x437416DBu, 0x452E933Fu);
	const auto line = FromBits(0xBC888B4Du, 0xBF772522u, 0x3F729B7Fu);
	const auto way = creature_feedback::SightWay(camera, line);
	EXPECT_EQ(std::bit_cast<uint32_t>(way.x), 0xBC480000u);
	EXPECT_EQ(std::bit_cast<uint32_t>(way.y), 0xBF36AD00u);
	EXPECT_EQ(std::bit_cast<uint32_t>(way.z), 0x3F335000u);
}
