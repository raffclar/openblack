/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Graphics/ObjectSnow.h"

using namespace openblack;

namespace
{
constexpr glm::vec3 k_Wall {0.4f, 0.3f, 0.2f};
constexpr glm::vec3 k_Snow {0.9f, 0.95f, 1.0f};
} // namespace

TEST(ObjectSnow, TheSnowShowsWhereItsAlphaIsAtLeast255LessTheLevel)
{
	for (const int32_t level : {5, 100, 200, 250})
	{
		EXPECT_EQ(object_snow::Threshold(level), 255 - level);
		EXPECT_TRUE(object_snow::Shows(level, 255 - level)) << level;
		EXPECT_FALSE(object_snow::Shows(level, 254 - level)) << level;
	}
	// At level 250 the alpha 5 shows the snow, and the alpha 4 does not
	EXPECT_TRUE(object_snow::Shows(250, 5));
	EXPECT_FALSE(object_snow::Shows(250, 4));
}

TEST(ObjectSnow, AFullLevelCoversAllAndNoLevelCoversNothing)
{
	EXPECT_TRUE(object_snow::Shows(255, 0));
	EXPECT_EQ(object_snow::Threshold(300), 0);
	EXPECT_FALSE(object_snow::Shows(0, 255));
	EXPECT_FALSE(object_snow::Shows(-1, 255));
}

TEST(ObjectSnow, TheSnowReplacesTheColourWithNoBlend)
{
	// The alpha of the snow texture only selects the texels. It does not mix the two colours.
	EXPECT_EQ(object_snow::Covered(k_Wall, k_Snow, 100, 155), k_Snow);
	EXPECT_EQ(object_snow::Covered(k_Wall, k_Snow, 100, 200), k_Snow);
	EXPECT_EQ(object_snow::Covered(k_Wall, k_Snow, 100, 154), k_Wall);
}
