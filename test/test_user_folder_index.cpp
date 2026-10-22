/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "FileSystem/UserFolderIndex.h"

using openblack::filesystem::LowerName;
using openblack::filesystem::UserFolderIndex;

TEST(UserFolderIndex, FindsFilesWithAnyLetterCase)
{
	const auto index = UserFolderIndex::Parse("0\t120\tData/AllMeshes.g3d\n1\t7\tScripts/Land1.txt\n");
	const auto found = index.Find("data/ALLMESHES.G3D");
	ASSERT_TRUE(found.has_value());
	const auto& node = index.Nodes()[*found];
	EXPECT_EQ(node.name, "AllMeshes.g3d");
	EXPECT_EQ(node.file, 0u);
	EXPECT_EQ(node.size, 120u);
	const auto script = index.Find("SCRIPTS/land1.TXT");
	ASSERT_TRUE(script.has_value());
	EXPECT_EQ(index.Nodes()[*script].file, 1u);
	EXPECT_FALSE(index.Find("Data/Missing.g3d").has_value());
}

TEST(UserFolderIndex, MakesOneFolderForPathsThatDifferInCase)
{
	const auto index = UserFolderIndex::Parse("0\t1\tAudio/SFX/a.sad\n1\t2\taudio/Music/b.sad\n");
	const auto audio = index.Find("AUDIO");
	ASSERT_TRUE(audio.has_value());
	EXPECT_EQ(index.Nodes()[*audio].name, "Audio");
	EXPECT_FALSE(index.Nodes()[*audio].file.has_value());
	EXPECT_EQ(index.Nodes()[*audio].children.size(), 2u);
	EXPECT_EQ(index.Nodes()[0].children.size(), 1u);
}

TEST(UserFolderIndex, KeepsTheFirstOfTwoFilesThatDifferInCase)
{
	const auto index = UserFolderIndex::Parse("4\t10\tData/x.txt\n5\t20\tdata/X.TXT\n");
	const auto found = index.Find("data/x.txt");
	ASSERT_TRUE(found.has_value());
	EXPECT_EQ(index.Nodes()[*found].file, 4u);
	EXPECT_EQ(index.Nodes()[*found].size, 10u);
}

TEST(UserFolderIndex, IgnoresLinesThatItCannotRead)
{
	const auto index = UserFolderIndex::Parse("\nx\t1\ta.txt\n1\ty\tb.txt\n2\t3\n3\t4\t../c.txt\n6\t5\td.txt\r\n");
	EXPECT_FALSE(index.Find("a.txt").has_value());
	EXPECT_FALSE(index.Find("b.txt").has_value());
	EXPECT_FALSE(index.Find("c.txt").has_value());
	const auto found = index.Find("D.TXT");
	ASSERT_TRUE(found.has_value());
	EXPECT_EQ(index.Nodes()[*found].name, "d.txt");
}

TEST(UserFolderIndex, FindsTheRootAndFolders)
{
	const auto index = UserFolderIndex::Parse("0\t1\tData/Landscape/Land1.lnd\n");
	EXPECT_EQ(index.Find(""), 0u);
	EXPECT_EQ(index.Find("./"), 0u);
	const auto folder = index.Find("data/landscape/");
	ASSERT_TRUE(folder.has_value());
	EXPECT_EQ(index.Nodes()[*folder].name, "Landscape");
}

TEST(UserFolderIndex, LowerNameChangesOnlyCapitalLetters)
{
	EXPECT_EQ(LowerName("Land1.TXT"), "land1.txt");
	EXPECT_EQ(LowerName("Plug Ins_[x]"), "plug ins_[x]");
}
