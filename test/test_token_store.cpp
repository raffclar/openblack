/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

// Enable this define because the test makes its own file system
#define LOCATOR_IMPLEMENTATIONS
#include "FileSystem/DefaultFileSystem.h"
#if !defined(__EMSCRIPTEN__)
#include "Online/Desktop/OsTokenStore.h"
#endif
#include "Online/TokenStoreInterface.h"

using namespace openblack;
using namespace openblack::online;

namespace
{

/// A folder of its own for each test, removed after it
class TokenStoreTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
		_folder = std::filesystem::temp_directory_path() / "openblack_token_store" / info->name();
		std::error_code error;
		std::filesystem::remove_all(_folder, error);
	}
	void TearDown() override
	{
		std::error_code error;
		std::filesystem::remove_all(_folder, error);
	}

	std::filesystem::path _folder;
	filesystem::DefaultFileSystem _files;
};

} // namespace

#if !defined(__EMSCRIPTEN__)
TEST_F(TokenStoreTest, TheFileHasTheMagicAndTheVersion)
{
	const std::vector<uint8_t> encrypted {9, 8, 7};
	const auto file = token_file::Pack(encrypted);
	EXPECT_EQ(file, (std::vector<uint8_t> {'O', 'B', 'L', 'T', 1, 9, 8, 7}));
	EXPECT_EQ(token_file::Unpack(file), encrypted);
	auto other = file;
	other[4] = 2;
	EXPECT_FALSE(token_file::Unpack(other).has_value());
	EXPECT_FALSE(token_file::Unpack(std::vector<uint8_t> {'O', 'B', 'L', 'T', 1}).has_value());
	EXPECT_FALSE(token_file::Unpack(std::vector<uint8_t> {'X', 'B', 'L', 'T', 1, 0}).has_value());
}
#endif

TEST_F(TokenStoreTest, AStoreThatKeepsNothing)
{
	NoTokenStore store;
	EXPECT_FALSE(store.Save("token"));
	EXPECT_FALSE(store.Load().has_value());
}

#if defined(_WIN32)
TEST_F(TokenStoreTest, WindowsEncryptsTheTokenForTheUserAndReadsItBack)
{
	const std::string token = "refresh-0123456789abcdef0123456789abcdef";
	const auto store = MakeTokenStore(_files, _folder);
	EXPECT_FALSE(store->Load().has_value());
	ASSERT_TRUE(store->Save(token));
	const auto file = _folder / k_TokenFileName;
	ASSERT_TRUE(std::filesystem::exists(file));

	// The file does not hold the token as plain text
	const auto bytes = _files.ReadAll(file);
	EXPECT_TRUE(std::ranges::search(bytes, token).empty());

	// A new store, as in the next run of the game, reads it back
	const auto again = MakeTokenStore(_files, _folder);
	EXPECT_EQ(again->Load(), token);

	// A new token replaces the old token
	ASSERT_TRUE(again->Save("second"));
	EXPECT_EQ(store->Load(), "second");
	EXPECT_FALSE(std::filesystem::exists(_folder / (std::string(k_TokenFileName) + ".new")));

	again->Clear();
	EXPECT_FALSE(std::filesystem::exists(file));
	EXPECT_FALSE(store->Load().has_value());
}

TEST_F(TokenStoreTest, WindowsRefusesAChangedFile)
{
	const auto store = MakeTokenStore(_files, _folder);
	ASSERT_TRUE(store->Save("token"));
	const auto file = _folder / k_TokenFileName;
	auto bytes = _files.ReadAll(file);
	bytes.back() ^= 0xFF;
	ASSERT_TRUE(_files.WriteAll(file, bytes));
	EXPECT_FALSE(store->Load().has_value());
}

TEST_F(TokenStoreTest, WindowsDecryptsOnlyWithTheGamesExtraBytes)
{
	const auto encrypted = DpapiTokenStore::Protect("token");
	ASSERT_TRUE(encrypted.has_value());
	EXPECT_EQ(DpapiTokenStore::Unprotect(*encrypted), "token");
	auto changed = *encrypted;
	changed[changed.size() / 2] ^= 0x01;
	EXPECT_FALSE(DpapiTokenStore::Unprotect(changed).has_value());
}
#endif

TEST_F(TokenStoreTest, WithNoProfileFolderNothingIsKept)
{
	const auto store = MakeTokenStore(_files, std::nullopt);
	EXPECT_FALSE(store->Save("token"));
	EXPECT_FALSE(store->Load().has_value());
}
