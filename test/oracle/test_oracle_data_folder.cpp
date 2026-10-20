/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdlib>

#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include "OracleData.h"

#include "../TestTempFolder.h"

using namespace openblack;

namespace
{

void SetFolder(const std::string& value)
{
#ifdef _WIN32
	_putenv_s("OPENBLACK_ORACLE_DATA", value.c_str());
#else
	setenv("OPENBLACK_ORACLE_DATA", value.c_str(), 1);
#endif
}

/// Puts the variable back as it was when the test ends
class RestoreFolder
{
public:
	RestoreFolder()
	{
		// NOLINTNEXTLINE(concurrency-mt-unsafe): the tests run on one thread
		if (const char* old = std::getenv("OPENBLACK_ORACLE_DATA"))
		{
			_old = old;
		}
	}
	~RestoreFolder() { SetFolder(_old); }

private:
	std::string _old;
};

} // namespace

// If the folder came from a compile-time constant, these tests would find the real data under any variable value
TEST(OracleDataFolder, FollowsTheEnvironmentVariable)
{
	const RestoreFolder restore;
	const auto folder = openblack::test::UniqueTempPath("oracle_data_folder_test");
	std::filesystem::create_directories(folder);
	{
		std::ofstream file(folder / "only_here.txt");
		file << "columns a\n0000003f\n";
	}
	SetFolder(folder.string());

	const auto found = oracle::DataFolder();
	ASSERT_TRUE(found.has_value());
	EXPECT_EQ(*found, folder);
	const auto table = oracle::Table::Load("only_here.txt");
	ASSERT_TRUE(table.has_value());
	EXPECT_EQ(table->Rows(), 1U);
	EXPECT_EQ(table->Bits(0, 0), 0x3fU);
	std::filesystem::remove_all(folder);
}

TEST(OracleDataFolder, EmptyFolderHasNoPinnedFiles)
{
	const RestoreFolder restore;
	const auto folder = openblack::test::UniqueTempPath("oracle_data_folder_empty");
	std::filesystem::create_directories(folder);
	SetFolder(folder.string());

	EXPECT_FALSE(oracle::Table::Load("sigmoid.txt").has_value());
	std::filesystem::remove_all(folder);
}
