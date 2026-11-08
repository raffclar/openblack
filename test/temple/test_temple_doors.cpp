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

#include <bit>
#include <vector>

#include <glm/gtc/constants.hpp>
#include <gtest/gtest.h>

#include "3D/TempleDoors.h"
#include "Audio/Sound.h"

using namespace openblack;

namespace
{
constexpr auto k_Open = static_cast<entt::id_type>(audio::SoundId::G_CitadelDoorOpen_01);
constexpr auto k_Close = static_cast<entt::id_type>(audio::SoundId::G_CitadelDoorClose_02);

/// How far the leaf at a joint has turned about the up axis
float TurnOf(const TempleDoors& doors, uint32_t joint)
{
	const auto& matrix = doors.GetJoints()[joint];
	return std::atan2(matrix[2][0], matrix[0][0]);
}

void RunFor(TempleDoors& doors, float seconds)
{
	for (float elapsed = 0.0f; elapsed < seconds - 1e-4f; elapsed += 0.05f)
	{
		doors.Update(0.05f);
	}
}
} // namespace

TEST(TempleDoors, LeadEachRoomThroughItsPairOfLeaves)
{
	EXPECT_EQ(TempleDoors::LeafOf(TempleRoom::Options), 3u);
	EXPECT_EQ(TempleDoors::LeafOf(TempleRoom::CreatureCave), 5u);
	EXPECT_EQ(TempleDoors::LeafOf(TempleRoom::Challenge), 7u);
	EXPECT_EQ(TempleDoors::LeafOf(TempleRoom::SaveGame), 9u);
	EXPECT_EQ(TempleDoors::LeafOf(TempleRoom::Credits), 11u);
	EXPECT_EQ(TempleDoors::LeafOf(TempleRoom::Multi), 13u);
	EXPECT_FALSE(TempleDoors::LeafOf(TempleRoom::Main).has_value());
}

TEST(TempleDoors, WaitThenSwingOpenWithASound)
{
	std::vector<entt::id_type> sounds;
	TempleDoors doors([&sounds](entt::id_type sound) { sounds.push_back(sound); });
	doors.Open(5, -0.5f, 0.4f);

	// Before the start of its swing, it waits shut
	RunFor(doors, 1.2f);
	EXPECT_TRUE(sounds.empty());
	EXPECT_FLOAT_EQ(TurnOf(doors, 5), 0.0f);

	// Then it creaks open, its leaves turning opposite ways
	RunFor(doors, 0.1f);
	ASSERT_EQ(sounds.size(), 1u);
	EXPECT_EQ(sounds[0], k_Open);
	RunFor(doors, 1.5f);
	EXPECT_GT(TurnOf(doors, 5), 0.5f);
	EXPECT_NEAR(TurnOf(doors, 6), -TurnOf(doors, 5), 1e-5f);
	// The other doorways stay shut
	EXPECT_FLOAT_EQ(TurnOf(doors, 3), 0.0f);

	// Fully open, the leaves are turned twice the half swing, and stay
	RunFor(doors, 2.0f);
	EXPECT_NEAR(TurnOf(doors, 5), 2.0f * TempleDoors::k_HalfSwing, 1e-4f);
	EXPECT_EQ(sounds.size(), 1u);
}

TEST(TempleDoors, ShutAtOnceWithASoundAndForget)
{
	std::vector<entt::id_type> sounds;
	TempleDoors doors([&sounds](entt::id_type sound) { sounds.push_back(sound); });
	// Shutting no doorway makes no sound
	doors.FastClose();
	EXPECT_TRUE(sounds.empty());

	doors.Open(7, 0.5f, 0.4f);
	doors.Update(0.0f);
	EXPECT_GT(TurnOf(doors, 7), 0.0f);
	doors.FastClose();
	ASSERT_EQ(sounds.size(), 1u);
	EXPECT_EQ(sounds[0], k_Close);
	doors.Update(0.05f);
	EXPECT_FLOAT_EQ(TurnOf(doors, 7), 0.0f);
	EXPECT_FALSE(doors.GetLeaf().has_value());
}

TEST(TempleDoors, TurnRoundToCloseFromAsFarOpenAsTheyAre)
{
	std::vector<entt::id_type> sounds;
	TempleDoors doors([&sounds](entt::id_type sound) { sounds.push_back(sound); });
	doors.Open(9, 0.3f, 0.0f);
	doors.Update(0.0f);
	const float open = TurnOf(doors, 9);
	doors.Close(1.6f);
	EXPECT_EQ(sounds.back(), k_Close);
	doors.Update(0.0f);
	EXPECT_NEAR(TurnOf(doors, 9), open, 1e-5f);
	// Closing again while it closes does nothing
	doors.Close(1.6f);
	EXPECT_EQ(sounds.size(), 1u);
	RunFor(doors, 0.5f);
	EXPECT_FALSE(doors.GetLeaf().has_value());
	EXPECT_FLOAT_EQ(TurnOf(doors, 9), 0.0f);
}

TEST(TempleDoors, TurnByTheGameRoundingOfEachStep)
{
	TempleDoors doors([](entt::id_type) {});
	doors.Open(5, 1.0f, 0.0f);
	doors.Update(0.0f);
	// Fully open, the turn is the float of two times the game's double half swing, one step above two times the float
	// half swing. The leaves keep their up axis exactly.
	const auto& first = doors.GetJoints()[5];
	EXPECT_EQ(std::bit_cast<uint32_t>(first[0][0]), 0x3E31D0CFu);
	EXPECT_EQ(std::bit_cast<uint32_t>(first[2][0]), 0x3F7C1C5Du);
	EXPECT_EQ(std::bit_cast<uint32_t>(first[0][2]), 0xBF7C1C5Du);
	EXPECT_EQ(first[1][1], 1.0f);
	const auto& second = doors.GetJoints()[6];
	EXPECT_EQ(std::bit_cast<uint32_t>(second[0][0]), 0x3E31D0CFu);
	EXPECT_EQ(std::bit_cast<uint32_t>(second[2][0]), 0xBF7C1C5Du);
	EXPECT_EQ(second[1][1], 1.0f);
}
