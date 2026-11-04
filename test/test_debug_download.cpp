/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <filesystem>

#include <gtest/gtest.h>

#include "Common/DebugDownload.h"

using namespace openblack;

namespace
{
const std::filesystem::path k_Profiles = "/work/Profiles";
}

TEST(DebugDownload, FileNextToProfilesIsOutside)
{
	EXPECT_TRUE(debug_download::IsOutsideFolder("/work/shot.png", k_Profiles));
	EXPECT_TRUE(debug_download::IsOutsideFolder("/work/ProfilesOther/shot.png", k_Profiles));
}

TEST(DebugDownload, FileInProfilesIsInside)
{
	EXPECT_FALSE(debug_download::IsOutsideFolder("/work/Profiles/Player/save.sav", k_Profiles));
	EXPECT_FALSE(debug_download::IsOutsideFolder("/work/Profiles/./Player/../save.sav", k_Profiles));
}

TEST(DebugDownload, FileReachedThroughParentIsOutside)
{
	EXPECT_TRUE(debug_download::IsOutsideFolder("/work/Profiles/../mind.erc", k_Profiles));
}

TEST(DebugDownload, OtherRootIsOutside)
{
	EXPECT_TRUE(debug_download::IsOutsideFolder("/tmp/mind.erc", k_Profiles));
}
