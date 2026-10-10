/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <utility>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <gtest/gtest.h>

#include "Audio/ListenerFrame.h"
#include "Camera/Camera.h"

using namespace openblack;

TEST(ListenerFrame, LooksAlongTheForwardWithTheRightAsTheWorldHasIt)
{
	// Looking north, along z, with y up: x, east, is to the right in the game's left-handed world
	const glm::vec3 eye {10.0f, 5.0f, 20.0f};
	const glm::vec3 forward {0.0f, 0.0f, 1.0f};
	const glm::vec3 up {0.0f, 1.0f, 0.0f};
	const auto east = audio::ToListenerFrame(eye + glm::vec3 {3.0f, 0.0f, 0.0f}, eye, forward, up);
	EXPECT_FLOAT_EQ(east.x, 3.0f);
	EXPECT_FLOAT_EQ(east.y, 0.0f);
	EXPECT_FLOAT_EQ(east.z, 0.0f);
	const auto ahead = audio::ToListenerFrame(eye + glm::vec3 {0.0f, 2.0f, 4.0f}, eye, forward, up);
	EXPECT_FLOAT_EQ(ahead.x, 0.0f);
	EXPECT_FLOAT_EQ(ahead.y, 4.0f);
	EXPECT_FLOAT_EQ(ahead.z, 2.0f);
}

TEST(ListenerFrame, WhatIsOnTheRightOfTheScreenIsHeardOnTheRight)
{
	constexpr glm::vec2 k_Screen {1280.0f, 720.0f};
	for (const auto& [origin, focus] : {std::pair {glm::vec3 {1000.0f, 200.0f, 1000.0f}, glm::vec3 {1300.0f, 0.0f, 1400.0f}},
	                                    std::pair {glm::vec3 {500.0f, 80.0f, 900.0f}, glm::vec3 {300.0f, 10.0f, 600.0f}}})
	{
		Camera camera;
		camera.SetProjectionMatrixPerspective(70.0f, k_Screen.x / k_Screen.y, 1.0f, 10000.0f);
		camera.SetOrigin(origin);
		camera.SetFocus(focus);
		// A point a little to the side of where the camera looks
		for (const float side : {-40.0f, 40.0f})
		{
			const auto point = focus + camera.GetRight() * side;
			glm::vec3 screen;
			ASSERT_TRUE(camera.ProjectWorldToScreen(point, glm::vec4(0.0f, 0.0f, k_Screen), screen));
			const auto heard = audio::ToListenerFrame(point, camera.GetOrigin(), camera.GetForward(), camera.GetUp());
			EXPECT_EQ(screen.x > k_Screen.x / 2.0f, heard.x > 0.0f) << "side " << side;
			EXPECT_GT(heard.y, 0.0f);
		}
	}
}
