/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Creature mind files through the resource cache's loader: read through the file system, never throwing

#include <MindFile.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "FileSystem/FileSystemInterface.h"
#include "Resources/Loaders.h"
#include "creature/SyntheticCreatureBlock.h"
#include "support/TestServices.h"

using namespace openblack;
using namespace openblack::creaturemind;
using namespace openblack::test::creature_block;

namespace
{
class CreatureMindLoaderTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (!spdlog::get("game"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("game");
		}
		_folder.emplace(std::string("openblack_test_creature_mind_loader_") +
		                ::testing::UnitTest::GetInstance()->current_test_info()->name());
		Locator::filesystem::value().SetGamePath(_folder->Path());
	}

	[[nodiscard]] std::shared_ptr<creature::CreatureMind> Load(const std::string& name) const
	{
		return resources::CreatureMindLoader {}(resources::CreatureMindLoader::FromDiskTag {}, _folder->Path() / name);
	}

	const test::ScopedDefaultFileSystem _fileSystem;
	std::optional<TempFolder> _folder;
};
} // namespace

TEST_F(CreatureMindLoaderTest, AMissingFileCannotBeOpened)
{
	std::shared_ptr<creature::CreatureMind> mind;
	EXPECT_NO_THROW(mind = Load("nobody.chr"));
	ASSERT_NE(mind, nullptr);
	EXPECT_EQ(mind->result, MindResult::ErrCantOpen);
	EXPECT_FALSE(mind->Loaded());
}

TEST_F(CreatureMindLoaderTest, AWrittenMindIsReadBack)
{
	MindFileData data;
	data.speciesRow = 4;
	_folder->Write("mind.chr", Write(data));
	const auto mind = Load("mind.chr");
	EXPECT_TRUE(mind->Loaded());
	EXPECT_EQ(mind->data.version, k_CurrentVersion);
	EXPECT_EQ(mind->data.speciesRow, 4u);
}

TEST_F(CreatureMindLoaderTest, ATruncatedFileIsReported)
{
	auto bytes = Write(MindFileData {});
	bytes.resize(bytes.size() / 2);
	_folder->Write("short.chr", bytes);
	EXPECT_EQ(Load("short.chr")->result, MindResult::ErrTruncated);
}

TEST_F(CreatureMindLoaderTest, AFileWithNoSpeciesRowIsNotAMind)
{
	// as the computer players' version 17 templates, which have something else where the species row is
	auto bytes = Write(MindFileData {});
	bytes[4] = 200;
	_folder->Write("template.chr", bytes);
	const auto mind = Load("template.chr");
	EXPECT_EQ(mind->result, MindResult::ErrNotAMindFile);
	EXPECT_FALSE(mind->Loaded());
}
