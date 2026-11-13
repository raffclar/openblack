/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The rules of the loose objects that the game makes as a creed or as a crop

#include <chrono>

#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/CreedObject.h"
#include "ECS/MobileObjectClassRules.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs;
namespace rules = openblack::ecs::mobile_object_class;

// The glow of a creed object draws the creed model at the object's place and turn, with the object's axes scaled by the
// scale of the glow
TEST(MobileObjectClassRules, TheCreedGlowTakesTheObjectsMatrixScaledByTheGlow)
{
	const auto object = glm::scale(
	    glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 2.0f, -4.0f)), 0.5f, glm::vec3(0.0f, 1.0f, 0.0f)),
	    glm::vec3(1.5f));
	const auto glow = rules::CreedGlowModel(object, 2.0f);
	for (int axis = 0; axis < 3; ++axis)
	{
		for (int row = 0; row < 3; ++row)
		{
			EXPECT_FLOAT_EQ(glow[axis][row], object[axis][row] * 2.0f);
		}
	}
	EXPECT_EQ(glow[3], object[3]);
	EXPECT_EQ(rules::CreedGlowModel(object, 1.0f), object);
}

// The glow of a creed object has the power of a new glow of a creature's creed power
TEST(MobileObjectClassRules, TheCreedGlowHasAPowerOfOne)
{
	EXPECT_EQ(rules::k_CreedGlowPower, 1.0f);
	EXPECT_EQ(components::CreedObject {}.glowScale, 1.0f);
}

// The texture of a creed glow slides by the real time of a frame, but not more often than the screen's refresh
TEST(MobileObjectClassRules, TheCreedGlowsSlideAtTheScreensRefresh)
{
	Registry registry;
	const auto creed = registry.Create();
	registry.Assign<components::CreedObject>(creed);
	// A frame shorter than a refresh leaves the texture where it is
	rules::SlideCreedGlows(registry, std::chrono::milliseconds(10));
	EXPECT_EQ(registry.Get<components::CreedObject>(creed).motion.slide, glm::vec2(0.0f));
	// The next frame makes a full refresh, and the texture slides by the time of both frames
	rules::SlideCreedGlows(registry, std::chrono::milliseconds(10));
	const auto slide = registry.Get<components::CreedObject>(creed).motion.slide;
	EXPECT_FLOAT_EQ(slide.x, 0.02f * 2.0f);
	EXPECT_FLOAT_EQ(slide.y, 0.02f * 0.2f);
}

// A crop's size is its life times the scale per life of its record
TEST(MobileObjectClassRules, ACropsSizeIsItsLifeTimesTheScalePerLife)
{
	EXPECT_FLOAT_EQ(rules::CropScale(1.0f, 0.8f), 0.8f);
	EXPECT_FLOAT_EQ(rules::CropScale(0.5f, 0.8f), 0.4f);
	EXPECT_FLOAT_EQ(rules::CropScale(2.0f, 1.25f), 2.5f);
}

// A crop goes from the world when its life is set to 0 or less, and stays with any life above 0
TEST(MobileObjectClassRules, ACropGoesWithNoLife)
{
	EXPECT_TRUE(rules::CropGoes(0.0f));
	EXPECT_TRUE(rules::CropGoes(-0.5f));
	EXPECT_FALSE(rules::CropGoes(0.001f));
	EXPECT_FALSE(rules::CropGoes(1.0f));
}
