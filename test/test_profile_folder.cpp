/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <filesystem>
#include <fstream>
#include <functional>
#include <istream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#define LOCATOR_IMPLEMENTATIONS

#include "Common/PlayerProfiles.h"
#include "ECS/Systems/Implementations/HelpProfileSystem.h"
#include "ECS/Systems/Implementations/ProfileFolderSystem.h"
#include "FileSystem/FileSystemInterface.h"
#include "Help/HelpProfile.h"
#include "Help/HelpProfileFile.h"

using namespace openblack;

namespace
{

/// A file system in memory: it keeps the files and the folders that the game writes
class FakeFileSystem final: public filesystem::FileSystemInterface
{
public:
	[[nodiscard]] std::filesystem::path FindPath(const std::filesystem::path& path) const override
	{
		if (!files.contains(path))
		{
			throw std::runtime_error("not found");
		}
		return path;
	}
	std::unique_ptr<std::istream> GetData(const std::filesystem::path& /*unused*/) override { return nullptr; }
	[[nodiscard]] bool IsPathValid(const std::filesystem::path& path) override { return files.contains(path); }
	std::unique_ptr<filesystem::Stream> Open(const std::filesystem::path& /*unused*/,
	                                         filesystem::Stream::Mode /*unused*/) override
	{
		return nullptr;
	}
	[[nodiscard]] bool Exists(const std::filesystem::path& path) const override { return files.contains(path); }
	void SetGamePath(const std::filesystem::path& path) override { gamePath = path; }
	[[nodiscard]] const std::filesystem::path& GetGamePath() const override { return gamePath; }
	void AddAdditionalPath(const std::filesystem::path& /*unused*/) override {}
	std::vector<uint8_t> ReadAll(const std::filesystem::path& path) override { return files.at(path); }
	void Iterate(const std::filesystem::path& /*unused*/, bool /*unused*/,
	             const std::function<void(const std::filesystem::path&)>& /*unused*/) const override
	{
	}
	bool MakeFolder(const std::filesystem::path& path) override
	{
		for (auto folder = path; !folder.empty() && folder != folder.parent_path(); folder = folder.parent_path())
		{
			folders.insert(folder);
		}
		return true;
	}
	bool WriteAll(const std::filesystem::path& path, std::span<const uint8_t> bytes) override
	{
		// A file can only go into a folder that the game made
		if (!folders.contains(path.parent_path()))
		{
			return false;
		}
		files[path] = std::vector<uint8_t>(bytes.begin(), bytes.end());
		return true;
	}

	std::map<std::filesystem::path, std::vector<uint8_t>> files;
	std::set<std::filesystem::path> folders;
	std::filesystem::path gamePath;
};

/// A profile folder in memory, for the help system
class FakeProfileFolder final: public ecs::systems::ProfileFolderSystemInterface
{
public:
	[[nodiscard]] std::optional<std::filesystem::path> GetFolder() const override { return "profiles"; }
	[[nodiscard]] std::optional<std::filesystem::path> GetPlayerFolder(std::u16string_view player) const override
	{
		return std::filesystem::path("profiles") / player_profiles::ProfileKey(player);
	}
	std::optional<std::filesystem::path> MakeFolder(const std::filesystem::path& relative) override
	{
		return std::filesystem::path("profiles") / relative;
	}
	[[nodiscard]] std::optional<std::vector<uint8_t>> ReadPlayerFile(std::u16string_view player,
	                                                                 std::string_view file) const override
	{
		const auto found = files.find(Key(player, file));
		if (found == files.end())
		{
			return std::nullopt;
		}
		return found->second;
	}
	bool WritePlayerFile(std::u16string_view player, std::string_view file, std::span<const uint8_t> bytes) override
	{
		files[Key(player, file)] = std::vector<uint8_t>(bytes.begin(), bytes.end());
		return true;
	}

	[[nodiscard]] static std::string Key(std::u16string_view player, std::string_view file)
	{
		return player_profiles::ProfileKey(player) + "/" + std::string(file);
	}

	std::map<std::string, std::vector<uint8_t>> files;
};

constexpr uint32_t k_FirstEvent = 1;
constexpr uint32_t k_LastEvent = 48;

} // namespace

TEST(ProfileFolderSystem, WithNoFolderTheGameKeepsNoFiles)
{
	FakeFileSystem files;
	ecs::systems::ProfileFolderSystem folder(files, std::nullopt);
	EXPECT_FALSE(folder.GetFolder().has_value());
	EXPECT_FALSE(folder.GetPlayerFolder(u"Ann").has_value());
	EXPECT_FALSE(folder.MakeFolder("custom_music").has_value());
	const std::vector<uint8_t> bytes {1, 2, 3};
	EXPECT_FALSE(folder.WritePlayerFile(u"Ann", "helpstats.dat", bytes));
	EXPECT_FALSE(folder.ReadPlayerFile(u"Ann", "helpstats.dat").has_value());
	EXPECT_TRUE(files.files.empty());
	EXPECT_TRUE(files.folders.empty());
}

TEST(ProfileFolderSystem, EachPlayerHasAFolderNamedByTheKeyOfTheProfile)
{
	FakeFileSystem files;
	const std::filesystem::path root = std::filesystem::path("work") / "Profiles";
	ecs::systems::ProfileFolderSystem folder(files, root);
	EXPECT_EQ(folder.GetFolder(), root);
	EXPECT_EQ(folder.GetPlayerFolder(u"Ann"), root / "_A_n_n");
	EXPECT_FALSE(folder.GetPlayerFolder(u"").has_value());
	// The game does not make a folder until it writes in it
	EXPECT_TRUE(files.folders.empty());
}

TEST(ProfileFolderSystem, TheGameMakesTheFoldersWhenItWritesTheFirstFile)
{
	FakeFileSystem files;
	const std::filesystem::path root = std::filesystem::path("work") / "Profiles";
	ecs::systems::ProfileFolderSystem folder(files, root);
	EXPECT_FALSE(folder.ReadPlayerFile(u"Ann", "helpstats.dat").has_value());

	const std::vector<uint8_t> bytes {1, 2, 3};
	ASSERT_TRUE(folder.WritePlayerFile(u"Ann", "helpstats.dat", bytes));
	EXPECT_TRUE(files.folders.contains(root));
	EXPECT_TRUE(files.folders.contains(root / "_A_n_n"));
	EXPECT_EQ(files.files.at(root / "_A_n_n" / "helpstats.dat"), bytes);
	EXPECT_EQ(folder.ReadPlayerFile(u"Ann", "helpstats.dat"), bytes);
	// Each player has their own file
	EXPECT_FALSE(folder.ReadPlayerFile(u"Matt", "helpstats.dat").has_value());

	EXPECT_EQ(folder.MakeFolder("custom_music"), root / "custom_music");
	EXPECT_TRUE(files.folders.contains(root / "custom_music"));
}

TEST(ProfileFolderSystem, DeletingAProfileEmptiesItsFolderButTheFolderStays)
{
	FakeFileSystem files;
	const auto root = std::filesystem::temp_directory_path() / "openblack_tests" / "empty_player_folder" / "Profiles";
	std::error_code error;
	std::filesystem::remove_all(root, error);
	const auto player = root / "_A_n_n";
	std::filesystem::create_directories(player / "Saved Games" / "Slot 1", error);
	std::ofstream(player / "helpstats.dat") << "help";
	std::ofstream(player / "Saved Games" / "Slot 1" / "save.sav") << "save";
	std::filesystem::create_directories(root / "_M_a_t_t", error);
	std::ofstream(root / "_M_a_t_t" / "helpstats.dat") << "help";

	ecs::systems::ProfileFolderSystem folder(files, root);
	EXPECT_TRUE(folder.EmptyPlayerFolder(u"Ann"));
	EXPECT_TRUE(std::filesystem::is_directory(player));
	EXPECT_TRUE(std::filesystem::is_empty(player));
	// The other players' folders stay as they are
	EXPECT_TRUE(std::filesystem::exists(root / "_M_a_t_t" / "helpstats.dat"));
	// A player with no folder has nothing to empty
	EXPECT_FALSE(folder.EmptyPlayerFolder(u"Zoe"));
	std::filesystem::remove_all(root.parent_path(), error);
}

TEST(FileSystemInterface, TheGameFolderIsOnlyTheFolderAndTheFoldersInIt)
{
	FakeFileSystem files;
	const auto base = std::filesystem::temp_directory_path() / "openblack_tests";
	EXPECT_FALSE(files.InGameFolder(base / "anything"));
	files.SetGamePath(base / "game");
	EXPECT_TRUE(files.InGameFolder(base / "game"));
	EXPECT_TRUE(files.InGameFolder(base / "game" / "Profiles" / "_A_n_n" / "helpstats.dat"));
	EXPECT_TRUE(files.InGameFolder(base / "work" / ".." / "game" / "Scripts"));
	EXPECT_FALSE(files.InGameFolder(base / "games" / "Profiles"));
	EXPECT_FALSE(files.InGameFolder(base / "work" / "Profiles"));
}

TEST(HelpProfileFile, AShortFileChangesOnlyTheCountsAtItsStart)
{
	help::profile::HelpProfile written;
	written.Trigger(k_FirstEvent);
	written.Trigger(k_LastEvent);
	auto bytes = help::profile::ToFileBytes(written);
	ASSERT_EQ(bytes.size(), helpstats::k_FileSize);

	// A file of the full length gives every count
	help::profile::HelpProfile read;
	help::profile::FromFileBytes(read, bytes);
	EXPECT_EQ(help::profile::ToFileBytes(read), bytes);

	// A shorter file gives only the counts that it holds
	help::profile::HelpProfile older;
	older.Trigger(k_LastEvent);
	older.EndTurn();
	older.Trigger(k_LastEvent);
	const auto olderLast = older.Count(k_LastEvent).Total();
	help::profile::FromFileBytes(older, std::span<const uint8_t>(bytes).first(sizeof(helpstats::EventRecord) * 2));
	EXPECT_EQ(older.Count(k_FirstEvent).Total(), written.Count(k_FirstEvent).Total());
	EXPECT_EQ(older.Count(k_LastEvent).Total(), olderLast);

	// The bytes after the record do not change the counts
	bytes.resize(bytes.size() + 16, 0xFF);
	help::profile::HelpProfile longer;
	help::profile::FromFileBytes(longer, bytes);
	EXPECT_EQ(longer.Count(k_LastEvent).Total(), written.Count(k_LastEvent).Total());
}

TEST(HelpProfileSystem, TheHelpRecordGoesToTheFolderOfThePlayerAndComesBack)
{
	FakeProfileFolder folder;
	ecs::systems::HelpProfileSystem first;
	first.Get().Trigger(k_FirstEvent);
	ASSERT_TRUE(first.SaveRecord(folder, u"Ann"));
	const auto file = folder.files.at(FakeProfileFolder::Key(u"Ann", help::profile::k_FileName));
	EXPECT_EQ(file.size(), helpstats::k_FileSize);

	ecs::systems::HelpProfileSystem later;
	later.LoadRecord(folder, u"Ann");
	EXPECT_EQ(later.Get().Count(k_FirstEvent).Total(), first.Get().Count(k_FirstEvent).Total());
	EXPECT_EQ(help::profile::ToFileBytes(later.Get()), file);
}

TEST(HelpProfileSystem, WithNoFileTheRecordStaysAsItIs)
{
	FakeProfileFolder folder;
	ecs::systems::HelpProfileSystem system;
	system.Get().Trigger(k_LastEvent);
	const auto before = help::profile::ToFileBytes(system.Get());
	// A player with no file takes the record of the player before
	system.LoadRecord(folder, u"Matt");
	EXPECT_EQ(help::profile::ToFileBytes(system.Get()), before);
}
