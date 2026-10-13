/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>

#include <array>
#include <filesystem>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "Graphics/PipelineCache.h"

using openblack::graphics::PipelineCache;

namespace
{

/// A folder of its own for each test, removed again after it
class PipelineCacheFolder: public ::testing::Test
{
protected:
	void SetUp() override
	{
		const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
		_folder = std::filesystem::temp_directory_path() / "openblack_test_pipeline_cache" / info->name();
		std::filesystem::remove_all(_folder);
	}
	void TearDown() override { std::filesystem::remove_all(_folder); }

	[[nodiscard]] std::filesystem::path File() const { return _folder / "cache" / "pipelines.bin"; }

private:
	std::filesystem::path _folder;
};

std::vector<std::byte> Bytes(std::initializer_list<int> values)
{
	std::vector<std::byte> bytes;
	for (const auto value : values)
	{
		bytes.push_back(static_cast<std::byte>(value));
	}
	return bytes;
}

} // namespace

TEST_F(PipelineCacheFolder, WithNoFileThereAreNoPipelines)
{
	const PipelineCache cache(File());
	EXPECT_EQ(cache.ReadSize(), 0);
	std::array<std::byte, 4> data {};
	EXPECT_FALSE(cache.Read(data));
}

TEST_F(PipelineCacheFolder, TheLastCacheWrittenIsKeptForTheNextRun)
{
	{
		PipelineCache cache(File());
		cache.Write(Bytes({1, 2}));
		cache.Write(Bytes({1, 2, 3}));
		EXPECT_EQ(cache.ReadSize(), 3);
	}
	const PipelineCache next(File());
	ASSERT_EQ(next.ReadSize(), 3);
	std::vector<std::byte> data(3);
	ASSERT_TRUE(next.Read(data));
	EXPECT_EQ(data, Bytes({1, 2, 3}));
}

TEST_F(PipelineCacheFolder, ADriverThatStartsAgainReplacesTheKeptPipelines)
{
	{
		PipelineCache cache(File());
		cache.Write(Bytes({1, 2, 3, 4}));
	}
	{
		PipelineCache cache(File());
		cache.Write(Bytes({9}));
	}
	const PipelineCache next(File());
	std::vector<std::byte> data(1);
	ASSERT_TRUE(next.Read(data));
	EXPECT_EQ(data, Bytes({9}));
}

TEST_F(PipelineCacheFolder, AReadOfTheWrongSizeGetsNothing)
{
	PipelineCache cache(File());
	cache.Write(Bytes({1, 2, 3}));
	std::vector<std::byte> data(2);
	EXPECT_FALSE(cache.Read(data));
}

TEST_F(PipelineCacheFolder, ARunWithNoNewPipelinesWritesNoFile)
{
	{
		const PipelineCache cache(File());
	}
	EXPECT_FALSE(std::filesystem::exists(File()));
}
