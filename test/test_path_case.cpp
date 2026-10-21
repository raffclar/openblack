/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include "FileSystem/FileSystemInterface.h"

using openblack::filesystem::FileSystemInterface;

namespace
{
// A folder of its own under the temporary folder, removed at the end of the test
class PathCaseTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_root = std::filesystem::temp_directory_path() /
		        ("openblack_path_case_" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
		std::filesystem::remove_all(_root);
		std::filesystem::create_directories(_root / "Scripts" / "Quests");
		std::ofstream(_root / "Scripts" / "Land1.txt") << "land";
	}
	void TearDown() override { std::filesystem::remove_all(_root); }

	std::filesystem::path _root;
};
} // namespace

TEST_F(PathCaseTest, FindsAFileWrittenInAnotherLetterCase)
{
	const auto found = FileSystemInterface::FindIgnoringCase(_root, "scripts/LAND1.TXT");
	ASSERT_TRUE(found.has_value());
	EXPECT_TRUE(std::filesystem::equivalent(*found, _root / "Scripts" / "Land1.txt"));
}

TEST_F(PathCaseTest, PassesOverTheCurrentFolderPartOfAScriptPath)
{
	const auto path = FileSystemInterface::FixPath(".\\scripts\\land1.txt");
	const auto found = FileSystemInterface::FindIgnoringCase(_root, path);
	ASSERT_TRUE(found.has_value());
	EXPECT_TRUE(std::filesystem::equivalent(*found, _root / "Scripts" / "Land1.txt"));
}

TEST_F(PathCaseTest, FindsAFolder)
{
	const auto found = FileSystemInterface::FindIgnoringCase(_root, "SCRIPTS/quests");
	ASSERT_TRUE(found.has_value());
	EXPECT_TRUE(std::filesystem::is_directory(*found));
}

TEST_F(PathCaseTest, FindsNothingForAMissingPartOrAClimbOutOfTheRoot)
{
	EXPECT_FALSE(FileSystemInterface::FindIgnoringCase(_root, "scripts/land2.txt").has_value());
	EXPECT_FALSE(FileSystemInterface::FindIgnoringCase(_root, "nothing/land1.txt").has_value());
	EXPECT_FALSE(FileSystemInterface::FindIgnoringCase(_root / "Scripts", "../Scripts/Land1.txt").has_value());
}
