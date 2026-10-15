/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <gtest/gtest.h>

#include "Help/StoryHistory.h"

using namespace openblack::help;

TEST(StoryHistory, TheNewestLineComesFirst)
{
	StoryHistory history;
	EXPECT_EQ(history.Count(), 0u);
	EXPECT_FALSE(history.Back(0).has_value());
	history.Add({.text = 10, .interaction = 1, .number = 0.0f, .narrator = 2});
	history.Add({.text = 11, .interaction = 0, .number = 5.0f, .narrator = 3});
	ASSERT_EQ(history.Count(), 2u);
	EXPECT_EQ(history.Back(0)->text, 11u);
	EXPECT_FLOAT_EQ(history.Back(0)->number, 5.0f);
	EXPECT_EQ(history.Back(1)->text, 10u);
	EXPECT_EQ(history.Back(1)->narrator, 2);
	EXPECT_FALSE(history.Back(2).has_value());
	EXPECT_FALSE(history.Back(-1).has_value());
}

TEST(StoryHistory, AFullHistoryPushesOutItsOldestLine)
{
	StoryHistory history;
	for (uint32_t i = 0; i < StoryHistory::k_Capacity + 3; ++i)
	{
		history.Add({.text = i});
	}
	EXPECT_EQ(history.Count(), StoryHistory::k_Capacity);
	EXPECT_EQ(history.Back(0)->text, StoryHistory::k_Capacity + 2);
	// The three oldest lines are gone
	EXPECT_EQ(history.Back(static_cast<int32_t>(StoryHistory::k_Capacity) - 1)->text, 3u);
	history.Clear();
	EXPECT_EQ(history.Count(), 0u);
	EXPECT_FALSE(history.Back(0).has_value());
}

TEST(StoryHistory, EachNarratorHasItsCaption)
{
	EXPECT_EQ(StoryCaption(2), "HELP_TEXT_DIALOG_ADDITION_131");
	EXPECT_EQ(StoryCaption(3), "HELP_TEXT_DIALOG_ADDITION_128");
	EXPECT_EQ(StoryCaption(7), "HELP_TEXT_DIALOG_ADDITION_79");
	EXPECT_EQ(StoryCaption(11), "HELP_TEXT_DIALOG_ADDITION_15");
	EXPECT_EQ(StoryCaption(12), "HELP_TEXT_DIALOG_ADDITION_134");
	// No narrator, the default narrator and the animals all give "This was said:"
	EXPECT_EQ(StoryCaption(0), "HELP_TEXT_DIALOG_ADDITION_135");
	EXPECT_EQ(StoryCaption(1), "HELP_TEXT_DIALOG_ADDITION_135");
	EXPECT_EQ(StoryCaption(15), "HELP_TEXT_DIALOG_ADDITION_135");
}

TEST(StoryHistory, TheFirstLineShownStaysInTheHistory)
{
	EXPECT_EQ(ClampStoryStart(3, 10), 3);
	EXPECT_EQ(ClampStoryStart(10, 10), 9);
	EXPECT_EQ(ClampStoryStart(-1, 10), 0);
	// An empty history starts at the newest line
	EXPECT_EQ(ClampStoryStart(4, 0), 0);
}
