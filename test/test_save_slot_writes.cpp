/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// All the folder and file work of a save goes to the writer's thread, in the game's order

#include <cstddef>
#include <cstdint>

#include <filesystem>
#include <functional>
#include <istream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/SaveFileWriter.h"
#include "ECS/SaveGameFiles.h"
#include "FileSystem/FileSystemInterface.h"

using namespace openblack;
using openblack::ecs::save_files::BackgroundWriter;
using openblack::ecs::save_files::SaveFile;
using openblack::ecs::save_files::SaveWrite;
using openblack::ecs::save_files::SlotWrite;
using openblack::ecs::save_files::TemporaryPath;
using openblack::ecs::save_files::WriteSlot;
using WriteResult = openblack::filesystem::FileSystemInterface::WriteResult;

namespace
{

/// A file system in memory that keeps a list of every folder and file call, in order, and the threads that made them
class LoggingFiles final: public filesystem::FileSystemInterface
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
	bool MakeFolder(const std::filesystem::path& path) override
	{
		Log("folder " + path.generic_string());
		if (badFolders.contains(path))
		{
			return false;
		}
		folders.insert(path);
		return true;
	}
	WriteResult WriteFile(const std::filesystem::path& path, std::span<const uint8_t> bytes) override
	{
		Log("write " + path.generic_string());
		if (notOpening.contains(path) || !folders.contains(path.parent_path()))
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
	bool WriteAt(const std::filesystem::path& /*unused*/, uint64_t /*unused*/, std::span<const uint8_t> /*unused*/) override
	{
		return false;
	}
	bool Rename(const std::filesystem::path& from, const std::filesystem::path& to) override
	{
		Log("rename " + from.generic_string());
		const auto found = files.find(from);
		if (found == files.end() || files.contains(to))
		{
			return false;
		}
		files[to] = std::move(found->second);
		files.erase(from);
		return true;
	}
	bool Remove(const std::filesystem::path& path) override
	{
		Log("remove " + path.generic_string());
		return files.erase(path) > 0;
	}

	void Log(std::string line)
	{
		const std::lock_guard lock(mutex);
		calls.push_back(std::move(line));
		threads.insert(std::this_thread::get_id());
	}

	std::map<std::filesystem::path, std::vector<uint8_t>> files;
	std::set<std::filesystem::path> folders;
	std::set<std::filesystem::path> badFolders;
	std::set<std::filesystem::path> notOpening;
	std::set<std::filesystem::path> incomplete;
	std::vector<std::string> calls;
	std::set<std::thread::id> threads;
	std::mutex mutex;
	std::filesystem::path gamePath;
};

const std::filesystem::path k_Slot = "Profiles/p/Saved Games/15/1";
const std::filesystem::path k_Minds = "Profiles/p/Saved Games/15/1/Creatures";
const std::filesystem::path k_World = k_Slot / "15.obsav";
const std::filesystem::path k_Vanilla = k_Slot / "15.sav";
const std::filesystem::path k_Info = k_Slot / "15.inf";
const std::filesystem::path k_Mind = k_Minds / "SavedCreature0.chl";
const std::vector<uint8_t> k_Old {1, 1, 1, 1};

/// A save of slot 15 with a picture step, an information file, one mind file and two world files. Each step writes
/// through the file system that it is given, so that the log shows it in its place.
SlotWrite FifteenWithOneCreature(std::shared_ptr<int> worldFilesMade)
{
	return {
	    .folders = {k_Slot, k_Minds},
	    .pictures = [](filesystem::FileSystemInterface& files) { return files.WriteAll(k_Slot / "picture", {}); },
	    .info = {.path = k_Info, .bytes = {5}},
	    .world = k_World,
	    .withWorld = {[](filesystem::FileSystemInterface& files) { return files.WriteAll(k_Mind, std::vector<uint8_t> {6}); }},
	    .worldFiles =
	        [worldFilesMade]() {
		        ++*worldFilesMade;
		        return std::vector<SaveFile> {{.path = k_World, .bytes = {7, 7}}, {.path = k_Vanilla, .bytes = {8}}};
	        },
	};
}

std::string Write(const std::filesystem::path& path)
{
	return "write " + path.generic_string();
}

} // namespace

TEST(SaveSlotWrites, TheWriterDoesTheWorkOfASaveInTheGamesOrder)
{
	LoggingFiles files;
	files.folders.insert(k_Slot);
	files.files[k_World] = k_Old;
	auto made = std::make_shared<int>(0);
	const auto result = WriteSlot(files, FifteenWithOneCreature(made));
	EXPECT_EQ(result.world, SaveWrite::Written);
	EXPECT_EQ(result.otherFailures, 0U);
	EXPECT_EQ(*made, 1);
	// The folders, the pictures and the information file come first. The temporary world file opens before the mind
	// files, and the world files take the place of the old files last.
	const std::vector<std::string> expected {
	    "folder " + k_Slot.generic_string(),
	    "folder " + k_Minds.generic_string(),
	    Write(k_Slot / "picture"),
	    Write(k_Info),
	    Write(TemporaryPath(k_World)),
	    Write(k_Mind),
	    Write(TemporaryPath(k_World)),
	    Write(TemporaryPath(k_Vanilla)),
	    "remove " + k_World.generic_string(),
	    "rename " + TemporaryPath(k_World).generic_string(),
	    "remove " + k_Vanilla.generic_string(),
	    "rename " + TemporaryPath(k_Vanilla).generic_string(),
	};
	EXPECT_EQ(files.calls, expected);
	EXPECT_EQ(files.files.at(k_World), (std::vector<uint8_t> {7, 7}));
	EXPECT_FALSE(files.Exists(TemporaryPath(k_World)));
}

TEST(SaveSlotWrites, ATemporaryWorldFileThatDoesNotOpenStopsTheSaveWithNoBox)
{
	LoggingFiles files;
	files.files[k_World] = k_Old;
	files.notOpening.insert(TemporaryPath(k_World));
	auto made = std::make_shared<int>(0);
	const auto result = WriteSlot(files, FifteenWithOneCreature(made));
	EXPECT_EQ(result.world, SaveWrite::NotOpened);
	// The information file was written before, as in the game. No mind file and no world file come after.
	EXPECT_TRUE(files.Exists(k_Info));
	EXPECT_FALSE(files.Exists(k_Mind));
	EXPECT_EQ(*made, 0);
	EXPECT_EQ(files.calls.back(), Write(TemporaryPath(k_World)));
	EXPECT_EQ(files.files.at(k_World), k_Old);
}

TEST(SaveSlotWrites, AWriteThatFailsKeepsTheOldFilesAndTheTemporaryOnes)
{
	LoggingFiles files;
	files.files[k_World] = k_Old;
	files.files[k_Vanilla] = k_Old;
	files.incomplete.insert(TemporaryPath(k_Vanilla));
	auto made = std::make_shared<int>(0);
	const auto result = WriteSlot(files, FifteenWithOneCreature(made));
	EXPECT_EQ(result.world, SaveWrite::Failed);
	EXPECT_EQ(files.files.at(k_World), k_Old);
	EXPECT_EQ(files.files.at(k_Vanilla), k_Old);
	EXPECT_TRUE(files.Exists(TemporaryPath(k_World)));
	EXPECT_TRUE(files.Exists(TemporaryPath(k_Vanilla)));
	// No old file went and no temporary file took a name
	for (const auto& call : files.calls)
	{
		EXPECT_FALSE(call.starts_with("remove ") || call.starts_with("rename ")) << call;
	}
}

TEST(SaveSlotWrites, AFolderThatCannotBeMadeDoesNotStopTheOtherWrites)
{
	LoggingFiles files;
	files.badFolders.insert(k_Minds);
	auto made = std::make_shared<int>(0);
	const auto result = WriteSlot(files, FifteenWithOneCreature(made));
	// The mind file has no folder, so it is not written. The game does not look at mind files, so the world is written.
	EXPECT_EQ(result.world, SaveWrite::Written);
	EXPECT_EQ(result.otherFailures, 1U);
	EXPECT_FALSE(files.Exists(k_Mind));
	EXPECT_EQ(files.files.at(k_World), (std::vector<uint8_t> {7, 7}));
}

TEST(SaveSlotWrites, NoFolderOrFileCallIsOnTheThreadThatQueuesTheSave)
{
	LoggingFiles files;
	auto made = std::make_shared<int>(0);
	auto order = std::make_shared<std::vector<std::string>>();
	{
		BackgroundWriter writer;
		writer.Queue("slot", [&files, made, order]() {
			const auto result = WriteSlot(files, FifteenWithOneCreature(made));
			order->push_back("slot");
			return result.world == SaveWrite::Written;
		});
		// The profiles file of an autosave comes after the files of the save
		writer.Queue("profiles", [&files, order]() {
			order->push_back("profiles");
			return files.WriteAll(k_Slot / "Profiles.txt", std::vector<uint8_t> {9});
		});
		writer.Flush();
		EXPECT_EQ(writer.Failures(), 0U);
	}
	EXPECT_EQ(*order, (std::vector<std::string> {"slot", "profiles"}));
	ASSERT_FALSE(files.calls.empty());
	EXPECT_FALSE(files.threads.contains(std::this_thread::get_id()));
	EXPECT_EQ(files.calls.back(), Write(k_Slot / "Profiles.txt"));
}
