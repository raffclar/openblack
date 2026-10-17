/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Audio/AxisLocks.h"
#include "Audio/HelpSpeech.h"

using namespace openblack::audio;

namespace
{
// Fake header values: the lock bits, a pitch bit that is not a lock, and a header position
constexpr uint32_t k_LockX = 0x4;
constexpr uint32_t k_LockY = 0x8;
constexpr uint32_t k_LockZ = 0x10;
constexpr uint32_t k_PitchOverride = 0x1;
constexpr std::array<float, 3> k_HeaderPosition {100.0f, 20.0f, -300.0f};
const glm::vec3 k_Caller {1.0f, 2.0f, 3.0f};

uint8_t Mask(LockedAxis axis)
{
	return static_cast<uint8_t>(axis);
}
} // namespace

TEST(AxisLocks, EachHeaderBitLocksItsAxis)
{
	EXPECT_EQ(LockedAxesOf(0), 0);
	EXPECT_EQ(LockedAxesOf(k_PitchOverride), 0);
	EXPECT_EQ(LockedAxesOf(k_LockX), Mask(LockedAxis::X));
	EXPECT_EQ(LockedAxesOf(k_LockY), Mask(LockedAxis::Y));
	EXPECT_EQ(LockedAxesOf(k_LockZ), Mask(LockedAxis::Z));
	EXPECT_EQ(LockedAxesOf(k_LockX | k_LockY | k_LockZ | k_PitchOverride),
	          Mask(LockedAxis::X) | Mask(LockedAxis::Y) | Mask(LockedAxis::Z));
}

TEST(AxisLocks, OnlyAnXOrYLockPlacesASoundWithoutAPosition)
{
	EXPECT_FALSE(LocksPlaceSound(0));
	EXPECT_TRUE(LocksPlaceSound(k_LockX));
	EXPECT_TRUE(LocksPlaceSound(k_LockY));
	EXPECT_FALSE(LocksPlaceSound(k_LockZ));
	EXPECT_TRUE(LocksPlaceSound(k_LockZ | k_LockY));
}

TEST(AxisLocks, ALockedAxisStartsAtTheHeaderValue)
{
	EXPECT_EQ(LockedStartPosition(0, k_HeaderPosition, k_Caller), k_Caller);
	EXPECT_EQ(LockedStartPosition(k_LockX, k_HeaderPosition, k_Caller), glm::vec3(100.0f, 2.0f, 3.0f));
	EXPECT_EQ(LockedStartPosition(k_LockY, k_HeaderPosition, k_Caller), glm::vec3(1.0f, 20.0f, 3.0f));
	EXPECT_EQ(LockedStartPosition(k_LockZ, k_HeaderPosition, k_Caller), glm::vec3(1.0f, 2.0f, -300.0f));
	// A caller without a position gives the origin for the axes that are not locked
	EXPECT_EQ(LockedStartPosition(k_LockX, k_HeaderPosition, glm::vec3(0.0f)), glm::vec3(100.0f, 0.0f, 0.0f));
}

TEST(AxisLocks, AMoveKeepsTheLockedAxes)
{
	const glm::vec3 current {100.0f, 2.0f, 3.0f};
	const glm::vec3 target {50.0f, 60.0f, 70.0f};
	EXPECT_EQ(MoveWithLockedAxes(0, current, target), target);
	EXPECT_EQ(MoveWithLockedAxes(Mask(LockedAxis::X), current, target), glm::vec3(100.0f, 60.0f, 70.0f));
	EXPECT_EQ(MoveWithLockedAxes(Mask(LockedAxis::Y), current, target), glm::vec3(50.0f, 2.0f, 70.0f));
	EXPECT_EQ(MoveWithLockedAxes(Mask(LockedAxis::Z), current, target), glm::vec3(50.0f, 60.0f, 3.0f));
	EXPECT_EQ(MoveWithLockedAxes(LockedAxesOf(k_LockX | k_LockY | k_LockZ), current, target), current);
}

TEST(AxisLocks, SpeechVoicesAndAdvisorsHaveTheirOwnOwnerNumbers)
{
	EXPECT_EQ(OwnerNumberOf(SpeechVoice::First), 9999u);
	EXPECT_EQ(OwnerNumberOf(SpeechVoice::Second), 9997u);
	EXPECT_EQ(k_AdvisorOwnerNumber, 9996u);
}
