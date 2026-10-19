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
#include <iterator>
#include <string>
#include <system_error>

#include <entt/entity/registry.hpp>
#include <gtest/gtest.h>

#include "Magic/VortexCrossingFile.h"

using namespace openblack;

namespace
{

struct FakeVortex
{
};

class VortexCrossingFile: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_folder = std::filesystem::temp_directory_path() /
		          ("openblack_crossing_" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
		std::filesystem::create_directories(_folder);
		_path = _folder / "vortex.txt";
	}

	void TearDown() override
	{
		std::error_code error;
		std::filesystem::remove_all(_folder, error);
	}

	[[nodiscard]] std::string Contents() const
	{
		std::ifstream file(_path, std::ios::binary);
		return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
	}

	std::filesystem::path _folder;
	std::filesystem::path _path;
};

/// Sends the destroy signal of the fake vortex to the file, as the vortex system does
struct Listener
{
	vortex::CrossingFile* file;
	void OnGone(entt::registry& /*registry*/, entt::entity entity) const { file->OwnerGone(entity); }
};

} // namespace

TEST_F(VortexCrossingFile, EachNewVortexEmptiesTheFile)
{
	vortex::CrossingFile file;
	ASSERT_TRUE(file.Start(entt::entity {1}, _path));
	file.Write("first\n");
	EXPECT_EQ(Contents(), "first\n");
	ASSERT_TRUE(file.Start(entt::entity {2}, _path));
	file.Write("second\n");
	EXPECT_EQ(Contents(), "second\n");
	EXPECT_EQ(file.Owner(), entt::entity {2});
}

TEST_F(VortexCrossingFile, EachThingIsWrittenOnce)
{
	vortex::CrossingFile file;
	EXPECT_FALSE(file.MarkWritten(entt::entity {5})) << "a closed file takes nothing";
	ASSERT_TRUE(file.Start(entt::entity {1}, _path));
	EXPECT_TRUE(file.MarkWritten(entt::entity {5}));
	EXPECT_FALSE(file.MarkWritten(entt::entity {5}));
	// A new vortex starts a new list
	ASSERT_TRUE(file.Start(entt::entity {2}, _path));
	EXPECT_TRUE(file.MarkWritten(entt::entity {5}));
}

TEST_F(VortexCrossingFile, OnlyTheOwnerClosesTheFile)
{
	vortex::CrossingFile file;
	ASSERT_TRUE(file.Start(entt::entity {3}, _path));
	file.OwnerGone(entt::entity {4});
	EXPECT_TRUE(file.IsOpen());
	file.OwnerGone(entt::entity {3});
	EXPECT_FALSE(file.IsOpen());
	EXPECT_TRUE(file.Owner() == entt::null);
	// Nothing goes into a closed file
	file.Write("late\n");
	EXPECT_EQ(Contents(), "");
}

TEST_F(VortexCrossingFile, TheFileClosesWhenTheLandClearsItsThings)
{
	entt::registry registry;
	vortex::CrossingFile file;
	Listener listener {.file = &file};
	const entt::scoped_connection connection = registry.on_destroy<FakeVortex>().connect<&Listener::OnGone>(listener);
	const auto other = registry.create();
	registry.emplace<FakeVortex>(other);
	const auto owner = registry.create();
	registry.emplace<FakeVortex>(owner);
	ASSERT_TRUE(file.Start(owner, _path));
	file.Write("tree\n");
	registry.destroy(other);
	EXPECT_TRUE(file.IsOpen());
	// The land change clears every thing; the file closes with its vortex
	registry.clear();
	EXPECT_FALSE(file.IsOpen());
	EXPECT_EQ(Contents(), "tree\n");
}

TEST_F(VortexCrossingFile, ALoadWritesOnAfterTheSavedLines)
{
	vortex::CrossingFile file;
	ASSERT_TRUE(file.Start(entt::entity {1}, _path));
	file.Write("tree\n");
	ASSERT_TRUE(file.MarkWritten(entt::entity {7}));
	// The load clears the land, puts the saved copy back and opens the file again for the same vortex
	file.Close();
	ASSERT_TRUE(file.Resume(entt::entity {1}, _path));
	EXPECT_EQ(file.Owner(), entt::entity {1});
	file.Write("pot\n");
	EXPECT_EQ(Contents(), "tree\npot\n");
	// The things written before the load are not known
	EXPECT_TRUE(file.MarkWritten(entt::entity {7}));
}

TEST_F(VortexCrossingFile, NoFileToOpenAgainLeavesItClosed)
{
	vortex::CrossingFile file;
	EXPECT_FALSE(file.Resume(entt::entity {1}, _folder / "none.txt"));
	EXPECT_FALSE(file.IsOpen());
	EXPECT_FALSE(std::filesystem::exists(_folder / "none.txt"));
}
