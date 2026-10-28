/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The glows on a creature's body: their powers, how their textures slide, the circling glow, and where they are placed

#include <cstdint>

#include <algorithm>
#include <array>
#include <limits>
#include <vector>

#include <glm/glm.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureHandGlow.h"

using namespace openblack;
using creature_hand_glow::Place;

TEST(CreatureHandGlow, APowerMakesAGlowAndNoPowerRemovesIt)
{
	creature_hand_glow::Glows glows;
	creature_hand_glow::Set(glows, Place::RightHand, 1.5f, 2.0f);
	ASSERT_TRUE(glows.at(2).has_value());
	EXPECT_FLOAT_EQ(glows.at(2)->power, 2.0f);
	EXPECT_FLOAT_EQ(glows.at(2)->scale, 1.5f);
	EXPECT_FALSE(glows.at(0).has_value());

	// A glow that has a new power stays, with its new scale
	creature_hand_glow::Set(glows, Place::RightHand, 1.0f, 3.0f);
	ASSERT_TRUE(glows.at(2).has_value());
	EXPECT_FLOAT_EQ(glows.at(2)->scale, 1.0f);
	EXPECT_FLOAT_EQ(creature_hand_glow::Zoom(*glows.at(2)), 6.0f);

	creature_hand_glow::Set(glows, Place::RightHand, 1.0f, 0.0f);
	EXPECT_FALSE(glows.at(2).has_value());
}

TEST(CreatureHandGlow, TheTextureSlidesByRealTimeAndWrapsRound)
{
	creature_hand_glow::Motion glow;
	creature_hand_glow::Slide(glow, 16);
	const float seconds = 16.0f * 0.001f;
	EXPECT_EQ(glow.slide.x, seconds * 2.0f);
	EXPECT_EQ(glow.slide.y, seconds * 0.2f);
	// Half a second slides it one whole sheet across, which comes back to where it was
	const auto before = glow.slide.x;
	creature_hand_glow::Slide(glow, 500);
	EXPECT_NEAR(glow.slide.x, before, 1e-6f);
	EXPECT_LT(glow.slide.x, 1.0f);
}

TEST(CreatureHandGlow, TheCirclingGlowTurnsByTheGameTimeAboveTheBody)
{
	creature_hand_glow::Motion glow;
	creature_hand_glow::Turn(glow, 1000);
	EXPECT_FLOAT_EQ(glow.turn, 1.0f);
	creature_hand_glow::Turn(glow, 0);
	EXPECT_FLOAT_EQ(glow.turn, 1.0f);

	const auto axes = creature_hand_glow::CirclingAxes(0.0f);
	EXPECT_EQ(axes, glm::mat3(1.0f));
	const auto quarter = creature_hand_glow::CirclingAxes(1.5707964f);
	EXPECT_NEAR(quarter[0].z, 1.0f, 1e-6f);
	EXPECT_NEAR(quarter[2].x, -1.0f, 1e-6f);

	const auto point = creature_hand_glow::CirclingPoint({10.0f, 2.0f, -3.0f}, 2.0f);
	EXPECT_FLOAT_EQ(point.x, 10.0f);
	EXPECT_FLOAT_EQ(point.y, 2.0f + (1.3f * 2.0f * 15.0f));
	EXPECT_FLOAT_EQ(point.z, -3.0f);
}

TEST(CreatureHandGlow, AHandGlowIsAtTheEndsOfTheBranchesInTheOrderWalked)
{
	// Bone 1 is the hand under the root. Its children are 2 and 3, and 2 has a child 4. Bone 5 comes after the hand
	// under the root.
	constexpr auto k_None = std::numeric_limits<uint32_t>::max();
	const std::vector<uint32_t> parents {k_None, 0, 1, 1, 2, 0};
	EXPECT_EQ(creature_hand_glow::BranchEnds(parents, 1), (std::vector<uint32_t> {4, 3, 5}));
	EXPECT_EQ(creature_hand_glow::BranchEnds(parents, 4), (std::vector<uint32_t> {4}));
	EXPECT_TRUE(creature_hand_glow::BranchEnds(parents, 9).empty());

	const std::array<glm::vec3, 2> points {glm::vec3(1.0f, 2.0f, 3.0f), glm::vec3(3.0f, 4.0f, 5.0f)};
	EXPECT_EQ(creature_hand_glow::Middle(points), glm::vec3(2.0f, 3.0f, 4.0f));
}

TEST(CreatureHandGlow, TheModelIsScaledWithItsBackAxisInMirrorImage)
{
	const auto unit = creature_hand_glow::UnitAxes(glm::mat3(2.0f));
	for (int i = 0; i < 3; ++i)
	{
		EXPECT_NEAR(glm::length(unit[i]), 1.0f, 1e-5f);
	}

	const auto model = creature_hand_glow::ModelMatrix(glm::mat3(1.0f), {1.0f, 2.0f, 3.0f}, 2.0f, 1.5f);
	EXPECT_EQ(model[0], glm::vec4(3.0f, 0.0f, 0.0f, 0.0f));
	EXPECT_EQ(model[1], glm::vec4(0.0f, 3.0f, 0.0f, 0.0f));
	EXPECT_EQ(model[2], glm::vec4(0.0f, 0.0f, -3.0f, 0.0f));
	EXPECT_EQ(model[3], glm::vec4(1.0f, 2.0f, 3.0f, 1.0f));
}

TEST(CreatureHandGlow, AScriptNumberNamesAPlaceOnlyWhenItIsInRange)
{
	EXPECT_EQ(creature_hand_glow::PlaceOf(0), Place::LeftHand);
	EXPECT_EQ(creature_hand_glow::PlaceOf(1), Place::Belly);
	EXPECT_EQ(creature_hand_glow::PlaceOf(2), Place::RightHand);
	EXPECT_EQ(creature_hand_glow::PlaceOf(3), Place::Circling);
	EXPECT_FALSE(creature_hand_glow::PlaceOf(-1).has_value());
	EXPECT_FALSE(creature_hand_glow::PlaceOf(4).has_value());
}

TEST(CreatureHandGlow, TheHumStartsAndStopsOnlyWhenTheGlowsGainOrLoseAllPower)
{
	using creature_hand_glow::Hum;
	creature_hand_glow::Glows glows;
	EXPECT_FALSE(creature_hand_glow::AnyActive(glows));

	EXPECT_EQ(creature_hand_glow::SetCreedPower(glows, Place::LeftHand, 1.0f, 0.5f), Hum::Start);
	EXPECT_EQ(creature_hand_glow::SetCreedPower(glows, Place::RightHand, 2.0f, 1.0f), Hum::Unchanged);
	ASSERT_TRUE(glows.at(2).has_value());
	EXPECT_FLOAT_EQ(glows.at(2)->scale, 2.0f);
	EXPECT_EQ(creature_hand_glow::SetCreedPower(glows, Place::LeftHand, 1.0f, 0.0f), Hum::Unchanged);
	EXPECT_EQ(creature_hand_glow::SetCreedPower(glows, Place::RightHand, 1.0f, 0.0f), Hum::Stop);
	EXPECT_FALSE(glows.at(2).has_value());

	// A glow with a power below 0 stays on the body, but it has no power, so the creature does not hum
	EXPECT_EQ(creature_hand_glow::SetCreedPower(glows, Place::Belly, 1.0f, -1.0f), Hum::Unchanged);
	EXPECT_TRUE(glows.at(1).has_value());
	EXPECT_FALSE(creature_hand_glow::AnyActive(glows));

	// A glow that an animation event gave counts as power before the script's change
	creature_hand_glow::Set(glows, Place::Circling, 1.0f, 2.0f);
	EXPECT_EQ(creature_hand_glow::SetCreedPower(glows, Place::LeftHand, 1.0f, 1.0f), Hum::Unchanged);
}

TEST(CreatureHandGlow, ASaveKeepsTheScaleAndThePowerOfEachPlaceAndZeroForNoGlow)
{
	creature_hand_glow::Glows glows;
	creature_hand_glow::Set(glows, Place::Belly, 0.7f, -1.0f);
	EXPECT_EQ(creature_hand_glow::SavedPowerOf(glows, Place::Belly),
	          (creature_hand_glow::SavedPower {.scale = 0.7f, .power = -1.0f}));
	EXPECT_EQ(creature_hand_glow::SavedPowerOf(glows, Place::LeftHand), creature_hand_glow::SavedPower {});
}

TEST(CreatureHandGlow, ALoadStartsWithNoGlowsAndGivesEachPlaceItsSavedPower)
{
	using creature_hand_glow::Hum;
	using creature_hand_glow::SavedPower;
	constexpr std::array<SavedPower, creature_hand_glow::k_PlaceCount> k_Saved {
	    SavedPower {.scale = 1.0f, .power = 2.0f},
	    SavedPower {},
	    SavedPower {.scale = 3.0f, .power = -0.5f},
	    SavedPower {.scale = 5.0f, .power = 0.1f},
	};

	// A creature with no glows gets the saved glows and starts to hum
	creature_hand_glow::Glows glows;
	EXPECT_EQ(creature_hand_glow::LoadCreedPowers(glows, k_Saved), Hum::Start);
	ASSERT_TRUE(glows.at(0).has_value());
	EXPECT_EQ(glows.at(0)->scale, 1.0f);
	EXPECT_EQ(glows.at(0)->power, 2.0f);
	EXPECT_FALSE(glows.at(1).has_value());
	ASSERT_TRUE(glows.at(2).has_value());
	EXPECT_EQ(glows.at(2)->power, -0.5f);
	ASSERT_TRUE(glows.at(3).has_value());

	// The load removes the glows that the creature had before, and the hum continues
	creature_hand_glow::Set(glows, Place::Belly, 1.0f, 1.0f);
	EXPECT_EQ(creature_hand_glow::LoadCreedPowers(glows, k_Saved), Hum::Unchanged);
	EXPECT_FALSE(glows.at(1).has_value());

	// A save with no power stops the hum and leaves no glows
	constexpr std::array<SavedPower, creature_hand_glow::k_PlaceCount> k_None {};
	EXPECT_EQ(creature_hand_glow::LoadCreedPowers(glows, k_None), Hum::Stop);
	EXPECT_TRUE(std::ranges::none_of(glows, [](const auto& glow) { return glow.has_value(); }));
	EXPECT_EQ(creature_hand_glow::LoadCreedPowers(glows, k_None), Hum::Unchanged);

	// The load also restores a glow with a power below 0, but the creature does not hum
	constexpr std::array<SavedPower, creature_hand_glow::k_PlaceCount> k_Negative {
	    SavedPower {}, SavedPower {.scale = 1.0f, .power = -1.0f}, SavedPower {}, SavedPower {}};
	EXPECT_EQ(creature_hand_glow::LoadCreedPowers(glows, k_Negative), Hum::Unchanged);
	EXPECT_TRUE(glows.at(1).has_value());
}

TEST(CreatureHandGlow, TheCreedEventsSetTheLeftHandThenTheRightHandThenTheBelly)
{
	EXPECT_FALSE(creature_hand_glow::CreedEventOf(94).has_value());
	EXPECT_FALSE(creature_hand_glow::CreedEventOf(110).has_value());
	EXPECT_FALSE(creature_hand_glow::CreedEventOf(3).has_value());

	constexpr std::array<Place, 3> k_Places {Place::LeftHand, Place::RightHand, Place::Belly};
	// The powers are exact: two times 0, a quarter, a half, three quarters and 1
	constexpr std::array<float, 5> k_Powers {0.0f, 0.5f, 1.0f, 1.5f, 2.0f};
	for (int32_t event = 95; event <= 109; ++event)
	{
		const auto glow = creature_hand_glow::CreedEventOf(event);
		ASSERT_TRUE(glow.has_value()) << event;
		EXPECT_EQ(glow->place, k_Places.at(static_cast<size_t>((event - 95) / 5))) << event;
		EXPECT_EQ(glow->power, k_Powers.at(static_cast<size_t>((event - 95) % 5))) << event;
	}

	// The first event of a place removes its glow
	creature_hand_glow::Glows glows;
	creature_hand_glow::Set(glows, Place::RightHand, 1.0f, 1.0f);
	const auto off = creature_hand_glow::CreedEventOf(100);
	creature_hand_glow::Set(glows, off->place, creature_hand_glow::k_CreedEventScale, off->power);
	EXPECT_FALSE(glows.at(2).has_value());
}
