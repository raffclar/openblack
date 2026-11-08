/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <gtest/gtest.h>

#include "3D/TempleSigns.h"

using namespace openblack;

TEST(TempleSigns, TheHighlightedLabelPulsesGreyOnceASecond)
{
	EXPECT_EQ(TempleSigns::HighlightGrey(0), 230);
	EXPECT_EQ(TempleSigns::HighlightGrey(100), 243);
	EXPECT_EQ(TempleSigns::HighlightGrey(256), 254);
	EXPECT_EQ(TempleSigns::HighlightGrey(768), 206);
	// The clock wraps each 1024 milliseconds
	EXPECT_EQ(TempleSigns::HighlightGrey(1024 + 100), 243);
}

TEST(TempleSigns, ALabelIsWrittenAtTheMiddleOfItsSignWithUnitAxes)
{
	const glm::mat4 frame(glm::vec4(2.0f, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f, 3.0f, 0.0f, 0.0f),
	                      glm::vec4(0.0f, 0.0f, 4.0f, 0.0f), glm::vec4(10.0f, 20.0f, 30.0f, 1.0f));
	const auto text = TempleSigns::FrameOf(frame, glm::vec3(-1.0f), glm::vec3(3.0f, 5.0f, 7.0f));
	// The middle of the box goes through the frame before its axes are made unit long
	EXPECT_EQ(text.origin, glm::vec3(12.0f, 26.0f, 42.0f));
	EXPECT_NEAR(text.axes[0].x, 1.0f, 1e-5f);
	// The y axis is turned round
	EXPECT_NEAR(text.axes[1].y, -1.0f, 1e-5f);
	EXPECT_NEAR(text.axes[2].z, 1.0f, 1e-5f);
	EXPECT_EQ(text.axes[0].y, 0.0f);
}
