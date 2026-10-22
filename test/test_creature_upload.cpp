/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <filesystem>
#include <variant>
#include <vector>

#include <LhpFile.h>
#include <LobbyApiJson.h>
#include <MindFile.h>
#include <PhysiqueFile.h>
#include <gtest/gtest.h>

// Enable this define because the test makes its own file system
#define LOCATOR_IMPLEMENTATIONS
#include "FileSystem/DefaultFileSystem.h"
#include "Online/CreatureUpload.h"

using namespace openblack;

namespace
{

/// A folder of its own for each test, with a package, a mind file and a physique file made for the test
class CreatureUploadTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
		_folder = std::filesystem::temp_directory_path() / "openblack_creature_upload" / info->name();
		std::error_code error;
		std::filesystem::remove_all(_folder, error);
		std::filesystem::create_directories(_folder);
		_paths = {
		    .package = _folder / "creature.lhp",
		    .mind = _folder / "_A_b_c.chl",
		    .physique = _folder / "_A_b_c_physique.chl",
		};
		lhp::Package package;
		package.stamp = 7;
		package.blocks.push_back(lhp::Block {.name = "Creature", .data = {1, 2, 3}});
		_package = lhp::Write(package);
		_mind = creaturemind::Write(creaturemind::MindFileData {});
		_physique = creaturemind::WritePhysique(creaturemind::PhysiqueFileData {.alignment = 0.5f});
	}
	void TearDown() override
	{
		std::error_code error;
		std::filesystem::remove_all(_folder, error);
	}
	void WriteAll()
	{
		ASSERT_TRUE(_files.WriteAll(_paths.package, _package));
		ASSERT_TRUE(_files.WriteAll(_paths.mind, _mind));
		ASSERT_TRUE(_files.WriteAll(_paths.physique, _physique));
	}

	std::filesystem::path _folder;
	creature_upload::Paths _paths;
	filesystem::DefaultFileSystem _files;
	std::vector<uint8_t> _package;
	std::vector<uint8_t> _mind;
	std::vector<uint8_t> _physique;
};

} // namespace

TEST_F(CreatureUploadTest, TheThreeFilesGoWithTheirNames)
{
	WriteAll();
	const auto gathered = creature_upload::Gather(_paths);
	ASSERT_TRUE(std::holds_alternative<lobby_api::CreatureUpload>(gathered));
	const auto& upload = std::get<lobby_api::CreatureUpload>(gathered);
	EXPECT_EQ(upload.package, _package);
	EXPECT_EQ(upload.mind, _mind);
	EXPECT_EQ(upload.physique, _physique);
	EXPECT_EQ(upload.mindFileName, "_A_b_c.chl");
	EXPECT_EQ(upload.physiqueFileName, "_A_b_c_physique.chl");
}

TEST_F(CreatureUploadTest, EachMissingOrBrokenFileIsNamed)
{
	EXPECT_EQ(std::get<creature_upload::Problem>(creature_upload::Gather(_paths)), creature_upload::Problem::NoPackage);
	ASSERT_TRUE(_files.WriteAll(_paths.package, std::vector<uint8_t> {'n', 'o'}));
	EXPECT_EQ(std::get<creature_upload::Problem>(creature_upload::Gather(_paths)), creature_upload::Problem::BadPackage);
	ASSERT_TRUE(_files.WriteAll(_paths.package, _package));
	EXPECT_EQ(std::get<creature_upload::Problem>(creature_upload::Gather(_paths)), creature_upload::Problem::NoMind);
	ASSERT_TRUE(_files.WriteAll(_paths.mind, std::vector<uint8_t> {1}));
	EXPECT_EQ(std::get<creature_upload::Problem>(creature_upload::Gather(_paths)), creature_upload::Problem::BadMind);
	ASSERT_TRUE(_files.WriteAll(_paths.mind, _mind));
	EXPECT_EQ(std::get<creature_upload::Problem>(creature_upload::Gather(_paths)), creature_upload::Problem::NoPhysique);
	ASSERT_TRUE(_files.WriteAll(_paths.physique, std::vector<uint8_t> {1}));
	EXPECT_EQ(std::get<creature_upload::Problem>(creature_upload::Gather(_paths)), creature_upload::Problem::BadPhysique);
}

TEST_F(CreatureUploadTest, AClanCreatureKeepsTheFilesThroughTheJson)
{
	const auto creature = creature_upload::PackClanCreature(-0.25f, _mind, _physique);
	ASSERT_TRUE(creature.has_value());
	const auto back = lobby_api::ParseClanCreature(lobby_api::ToJson(*creature));
	ASSERT_TRUE(back.has_value());
	EXPECT_TRUE(creature_upload::IsValidClanCreature(*back));
	EXPECT_EQ(back->mind, _mind);
	EXPECT_EQ(back->physique, _physique);
	EXPECT_EQ(back->alignment, -0.25f);

	EXPECT_FALSE(creature_upload::PackClanCreature(0.0f, std::vector<uint8_t> {1}, _physique).has_value());
	auto broken = *back;
	broken.physique.resize(1);
	EXPECT_FALSE(creature_upload::IsValidClanCreature(broken));
}
