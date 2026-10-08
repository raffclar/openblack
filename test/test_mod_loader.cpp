/*******************************************************************************
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
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "ModLoader.h"

namespace
{
/// A mods folder of its own in the temporary directory, removed with the test
class ModLoaderTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		const auto* test = ::testing::UnitTest::GetInstance()->current_test_info();
		_folder = std::filesystem::temp_directory_path() / (std::string("openblack_mod_loader_") + test->name());
		std::filesystem::remove_all(_folder);
		std::filesystem::create_directories(_folder);
	}
	void TearDown() override { std::filesystem::remove_all(_folder); }

	void WriteVersion(std::string_view text) const
	{
		std::ofstream file(_folder / "version.txt", std::ios::binary);
		file << text;
	}

	/// LoadMods on the folder, keeping every line it reports
	int Load()
	{
		const auto keep = [](const char* line, void* context) {
			static_cast<std::vector<std::string>*>(context)->emplace_back(line);
		};
		return ModLoader_LoadMods(_folder.string().c_str(), keep, &_lines);
	}

	std::filesystem::path _folder;
	std::vector<std::string> _lines;
};
} // namespace

TEST(ModLoader, VersionIsTheHeadersVersion)
{
	EXPECT_EQ(ModLoader_Version(), k_ModLoaderVersion);
}

TEST_F(ModLoaderTest, NoVersionFileReadsNoMods)
{
	EXPECT_EQ(Load(), static_cast<int>(ModLoaderResult::NoVersionFile));
	ASSERT_EQ(_lines.size(), 1u);
	EXPECT_EQ(_lines[0], "version.txt missing: no mods read");
}

TEST_F(ModLoaderTest, WrongVersionReadsNoMods)
{
	WriteVersion(std::to_string(k_ModLoaderVersion + 1));
	EXPECT_EQ(Load(), static_cast<int>(ModLoaderResult::VersionMismatch));
	ASSERT_EQ(_lines.size(), 1u);
	EXPECT_NE(_lines[0].find("no mods read"), std::string::npos);
}

TEST_F(ModLoaderTest, NotANumberReadsNoMods)
{
	WriteVersion("1 beta");
	EXPECT_EQ(Load(), static_cast<int>(ModLoaderResult::UnreadableVersion));
	ASSERT_EQ(_lines.size(), 1u);
	WriteVersion("");
	EXPECT_EQ(Load(), static_cast<int>(ModLoaderResult::UnreadableVersion));
	EXPECT_EQ(_lines.size(), 2u);
}

TEST_F(ModLoaderTest, RightVersionMatchesAndLoadsNothingYet)
{
	WriteVersion(" " + std::to_string(k_ModLoaderVersion) + "\r\n");
	EXPECT_EQ(Load(), static_cast<int>(ModLoaderResult::Matched));
	ASSERT_EQ(_lines.size(), 1u);
	EXPECT_NE(_lines[0].find("no mods are loaded yet"), std::string::npos);
}

TEST(ModLoader, NoFolderOrNoLogIsSafe)
{
	EXPECT_EQ(ModLoader_LoadMods(nullptr, nullptr, nullptr), static_cast<int>(ModLoaderResult::NoVersionFile));
}
