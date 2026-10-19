/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The world files of a save go to temporary files first; a load counts its attempts in the file

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <filesystem>
#include <functional>
#include <istream>
#include <map>
#include <memory>
#include <set>
#include <span>
#include <vector>

#include <SaveSlots.h>
#include <WorldFile.h>
#include <gtest/gtest.h>

#include "ECS/SaveGameFiles.h"
#include "FileSystem/FileSystemInterface.h"

using namespace openblack;
using openblack::ecs::save_files::SaveFile;
using openblack::ecs::save_files::SaveWrite;
using openblack::ecs::save_files::TemporaryPath;
using openblack::ecs::save_files::WriteThroughTemporaries;
using WriteResult = openblack::filesystem::FileSystemInterface::WriteResult;

namespace
{

/// A file system in memory. A path can be set not to open, or to take only some of its bytes.
class FakeFiles final: public filesystem::FileSystemInterface
{
public:
	[[nodiscard]] std::filesystem::path FindPath(const std::filesystem::path& path) const override { return path; }
	std::unique_ptr<std::istream> GetData(const std::filesystem::path& /*unused*/) override { return nullptr; }
	[[nodiscard]] bool IsPathValid(const std::filesystem::path& /*unused*/) override { return true; }
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
	WriteResult WriteFile(const std::filesystem::path& path, std::span<const uint8_t> bytes) override
	{
		writes.push_back(path);
		if (notOpening.contains(path))
		{
			return WriteResult::NotOpened;
		}
		if (incomplete.contains(path))
		{
			files[path] = std::vector<uint8_t>(bytes.begin(), bytes.begin() + static_cast<ptrdiff_t>(bytes.size() / 2));
			return WriteResult::Incomplete;
		}
		files[path] = std::vector<uint8_t>(bytes.begin(), bytes.end());
		return WriteResult::Written;
	}
	bool WriteAt(const std::filesystem::path& path, uint64_t offset, std::span<const uint8_t> bytes) override
	{
		const auto found = files.find(path);
		if (found == files.end() || offset + bytes.size() > found->second.size())
		{
			return false;
		}
		std::ranges::copy(bytes, found->second.begin() + static_cast<ptrdiff_t>(offset));
		return true;
	}
	bool Rename(const std::filesystem::path& from, const std::filesystem::path& to) override
	{
		const auto found = files.find(from);
		if (found == files.end() || files.contains(to))
		{
			return false;
		}
		files[to] = std::move(found->second);
		files.erase(from);
		return true;
	}
	bool Remove(const std::filesystem::path& path) override { return files.erase(path) > 0; }

	std::map<std::filesystem::path, std::vector<uint8_t>> files;
	std::set<std::filesystem::path> notOpening;
	std::set<std::filesystem::path> incomplete;
	std::vector<std::filesystem::path> writes;
	std::filesystem::path gamePath;
};

const std::filesystem::path k_World = "Saved Games/3/3.obsav";
const std::filesystem::path k_Vanilla = "Saved Games/3/3.sav";
const std::filesystem::path k_Script = "Saved Games/3/Script_3.sav";
const std::vector<uint8_t> k_Old {1, 1, 1, 1};

std::vector<SaveFile> ThreeFiles()
{
	return {{.path = k_World, .bytes = {7, 7, 7, 7}}, {.path = k_Vanilla, .bytes = {8, 8}}, {.path = k_Script, .bytes = {9}}};
}

} // namespace

TEST(SafeSaveFiles, TheTemporaryFileIsTheNameWithTmpAtItsEnd)
{
	EXPECT_EQ(TemporaryPath(k_World), std::filesystem::path("Saved Games/3/3.obsav.tmp"));
	EXPECT_EQ(TemporaryPath(k_Script), std::filesystem::path("Saved Games/3/Script_3.sav.tmp"));
}

TEST(SafeSaveFiles, ACompleteSaveTakesThePlaceOfTheOldFiles)
{
	FakeFiles files;
	files.files[k_World] = k_Old;
	files.files[k_Script] = k_Old;
	EXPECT_EQ(WriteThroughTemporaries(files, ThreeFiles()), SaveWrite::Written);
	EXPECT_EQ(files.files.at(k_World), (std::vector<uint8_t> {7, 7, 7, 7}));
	EXPECT_EQ(files.files.at(k_Vanilla), (std::vector<uint8_t> {8, 8}));
	EXPECT_EQ(files.files.at(k_Script), (std::vector<uint8_t> {9}));
	EXPECT_FALSE(files.Exists(TemporaryPath(k_World)));
	EXPECT_FALSE(files.Exists(TemporaryPath(k_Script)));
	// Every file went to its temporary file first
	EXPECT_EQ(files.writes,
	          (std::vector<std::filesystem::path> {TemporaryPath(k_World), TemporaryPath(k_Vanilla), TemporaryPath(k_Script)}));
}

TEST(SafeSaveFiles, AWriteThatFailsKeepsTheOldFilesAndTheTemporaryOnes)
{
	FakeFiles files;
	files.files[k_World] = k_Old;
	files.files[k_Vanilla] = k_Old;
	files.incomplete.insert(TemporaryPath(k_Vanilla));
	EXPECT_EQ(WriteThroughTemporaries(files, ThreeFiles()), SaveWrite::Failed);
	EXPECT_EQ(files.files.at(k_World), k_Old);
	EXPECT_EQ(files.files.at(k_Vanilla), k_Old);
	EXPECT_EQ(files.files.at(TemporaryPath(k_World)), (std::vector<uint8_t> {7, 7, 7, 7}));
	EXPECT_EQ(files.files.at(TemporaryPath(k_Vanilla)), (std::vector<uint8_t> {8}));
	// No write after the one that failed
	EXPECT_FALSE(files.Exists(TemporaryPath(k_Script)));
	EXPECT_EQ(files.writes.size(), 2u);
}

TEST(SafeSaveFiles, OnlyTheFirstFileNotOpeningMeansNothingWasWritten)
{
	FakeFiles first;
	first.files[k_World] = k_Old;
	first.notOpening.insert(TemporaryPath(k_World));
	EXPECT_EQ(WriteThroughTemporaries(first, ThreeFiles()), SaveWrite::NotOpened);
	EXPECT_EQ(first.files.at(k_World), k_Old);
	EXPECT_EQ(first.writes.size(), 1u);

	FakeFiles later;
	later.notOpening.insert(TemporaryPath(k_Script));
	EXPECT_EQ(WriteThroughTemporaries(later, ThreeFiles()), SaveWrite::Failed) << "the player hears of it";
	EXPECT_FALSE(later.Exists(k_World));
}

TEST(SafeSaveFiles, ACompleteWorldFileHasNoLoadAttempts)
{
	gamesave::WorldFile world;
	world.chunks.push_back({.tag = gamesave::k_GameTag, .bytes = {1, 2, 3}});
	const auto bytes = gamesave::Write(world);
	EXPECT_EQ(gamesave::ReadAttempts(bytes), 0u);
	EXPECT_TRUE(gamesave::MayLoad(0));
	EXPECT_TRUE(gamesave::MayLoad(1));
	EXPECT_FALSE(gamesave::MayLoad(2)) << "the game stopped two times as it loaded the file";
	EXPECT_FALSE(gamesave::MayLoad(3));
}

TEST(SafeSaveFiles, ALoadCountsItsAttemptInTheFile)
{
	gamesave::WorldFile world;
	world.chunks.push_back({.tag = gamesave::k_GameTag, .bytes = {1, 2, 3}});
	FakeFiles files;
	files.files[k_World] = gamesave::Write(world);
	// Each load adds one before it reads the world
	ASSERT_TRUE(files.WriteAt(k_World, gamesave::k_AttemptsOffset, gamesave::AttemptsBytes(1)));
	EXPECT_EQ(gamesave::ReadAttempts(files.files.at(k_World)), 1u);
	ASSERT_TRUE(files.WriteAt(k_World, gamesave::k_AttemptsOffset, gamesave::AttemptsBytes(2)));
	const auto read = gamesave::ReadWorldFile(files.files.at(k_World));
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->attempts, 2u);
	EXPECT_EQ(read->chunks, world.chunks) << "the count changes nothing else";
	EXPECT_FALSE(gamesave::MayLoad(read->attempts));
}

TEST(SafeSaveFiles, AFileOfTheVersionBeforeHasNoCount)
{
	gamesave::WorldFile old {.version = 3, .attempts = 5, .chunks = {{.tag = gamesave::k_GameTag, .bytes = {4}}}};
	const auto bytes = gamesave::Write(old);
	EXPECT_EQ(gamesave::ReadAttempts(bytes), 0u);
	const auto read = gamesave::ReadWorldFile(bytes);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->attempts, 0u);
	EXPECT_EQ(read->chunks, old.chunks);
	EXPECT_FALSE(gamesave::ReadAttempts(std::vector<uint8_t> {1, 2, 3}).has_value());
}

TEST(SafeSaveFiles, ThePreviousAndNextLandsLoadFromTheirOwnLandsFolders)
{
	using gamesave::LoadSlotFolder;
	using gamesave::SlotFolder;
	// A save into the previous land's slot on land 1 goes into the folder of land 1, which land 2 loads from
	EXPECT_EQ(SlotFolder(gamesave::k_PreviousLandSlot, 1), LoadSlotFolder(gamesave::k_PreviousLandSlot, 2));
	EXPECT_EQ(SlotFolder(gamesave::k_NextLandSlot, 3), LoadSlotFolder(gamesave::k_NextLandSlot, 2));
	EXPECT_EQ(LoadSlotFolder(15, 2), SlotFolder(15, 2));
	EXPECT_EQ(LoadSlotFolder(4, 2), SlotFolder(4, 2));
	EXPECT_EQ(gamesave::SaveBeforeRoomLoad(gamesave::k_PreviousLandSlot), gamesave::k_NextLandSlot);
	EXPECT_EQ(gamesave::SaveBeforeRoomLoad(gamesave::k_NextLandSlot), gamesave::k_PreviousLandSlot);
	EXPECT_FALSE(gamesave::SaveBeforeRoomLoad(15).has_value());
	EXPECT_FALSE(gamesave::SaveBeforeRoomLoad(19).has_value());
}
