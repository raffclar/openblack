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
#include <iterator>
#include <set>
#include <string>
#include <system_error>

#include <InspectorDiscovery.h>
#include <gtest/gtest.h>

using namespace openblack::inspector;
using namespace openblack::inspector::discovery;

namespace
{

/// A fresh folder of the test's own, removed after it
class Folder
{
public:
	explicit Folder(const std::string& name)
	    : _path(std::filesystem::temp_directory_path() /
	            ("openblack-inspector-test-" + std::to_string(CurrentProcessId()) + "-" + name))
	{
		std::error_code error;
		std::filesystem::remove_all(_path, error);
		std::filesystem::create_directories(_path, error);
	}
	~Folder()
	{
		std::error_code error;
		std::filesystem::remove_all(_path, error);
	}
	Folder(const Folder&) = delete;
	Folder& operator=(const Folder&) = delete;
	Folder(Folder&&) = delete;
	Folder& operator=(Folder&&) = delete;

	[[nodiscard]] const std::filesystem::path& Path() const { return _path; }

private:
	std::filesystem::path _path;
};

GameRecord Record(uint32_t pid, uint16_t port)
{
	return {
	    .port = port,
	    .pid = pid,
	    .worktree = "C:/projects/ob-wt-example",
	    .executable = "C:/projects/ob-wt-example/build/bin/openblack.exe",
	    .buildType = "Debug",
	    .land = "Land1",
	    .startTime = 1760000000,
	};
}

void WriteFile(const std::filesystem::path& path, const std::string& text)
{
	std::ofstream stream(path, std::ios::binary);
	stream << text;
}

std::string ReadFile(const std::filesystem::path& path)
{
	std::ifstream stream(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

} // namespace

TEST(InspectorDiscovery, RecordRoundTripsThroughJson)
{
	const auto record = Record(4321, 47801);
	const auto json = ToJson(record);
	EXPECT_EQ(json["port"], 47801);
	EXPECT_EQ(json["pid"], 4321);
	EXPECT_EQ(json["build_type"], "Debug");

	const auto read = FromJson(json);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->port, record.port);
	EXPECT_EQ(read->pid, record.pid);
	EXPECT_EQ(read->worktree, record.worktree);
	EXPECT_EQ(read->executable, record.executable);
	EXPECT_EQ(read->buildType, record.buildType);
	EXPECT_EQ(read->land, record.land);
	EXPECT_EQ(read->startTime, record.startTime);
}

TEST(InspectorDiscovery, ARecordNeedsAPortAndAPid)
{
	EXPECT_FALSE(FromJson(Json {{"pid", 12}}).has_value());
	EXPECT_FALSE(FromJson(Json {{"port", 47800}}).has_value());
	EXPECT_FALSE(FromJson(Json {{"port", 0}, {"pid", 12}}).has_value());
	EXPECT_FALSE(FromJson(Json {{"port", 70000}, {"pid", 12}}).has_value());
	EXPECT_FALSE(FromJson(Json::array()).has_value());
	EXPECT_TRUE(FromJson(Json {{"port", 47800}, {"pid", 12}}).has_value());
}

TEST(InspectorDiscovery, TheFileIsWrittenRewrittenAndRemoved)
{
	const Folder folder("file");
	const auto path = FileFor(folder.Path(), 4321);
	EXPECT_EQ(path.filename(), "4321.json");
	{
		DiscoveryFile file(folder.Path(), Record(4321, 47801));
		EXPECT_TRUE(file.Written());
		ASSERT_TRUE(std::filesystem::exists(path));
		auto read = FromJson(Parse(ReadFile(path)).value_or(Json()));
		ASSERT_TRUE(read.has_value());
		EXPECT_EQ(read->port, 47801);
		EXPECT_EQ(read->land, "Land1");

		file.SetLand("Land2");
		read = FromJson(Parse(ReadFile(path)).value_or(Json()));
		ASSERT_TRUE(read.has_value());
		EXPECT_EQ(read->land, "Land2");
		// Nothing is left aside from the writing
		EXPECT_EQ(std::distance(std::filesystem::directory_iterator(folder.Path()), std::filesystem::directory_iterator()), 1);
	}
	EXPECT_FALSE(std::filesystem::exists(path));
}

TEST(InspectorDiscovery, TheFolderIsMadeWhenMissing)
{
	const Folder folder("missing");
	const auto inner = folder.Path() / "inner";
	const DiscoveryFile file(inner, Record(55, 47802));
	EXPECT_TRUE(file.Written());
	EXPECT_TRUE(std::filesystem::exists(FileFor(inner, 55)));
}

// Games whose processes have gone left their files behind: those are removed, files that aren't a game's are left
TEST(InspectorDiscovery, ReadingSkipsAndRemovesStaleFiles)
{
	const Folder folder("stale");
	WriteFile(FileFor(folder.Path(), 100), Dump(ToJson(Record(100, 47801))));
	WriteFile(FileFor(folder.Path(), 200), Dump(ToJson(Record(200, 47802))));
	WriteFile(FileFor(folder.Path(), 300), "not json");
	// A file whose record names another process than its name
	WriteFile(FileFor(folder.Path(), 400), Dump(ToJson(Record(401, 47804))));
	WriteFile(folder.Path() / "frame_12.png", "picture");
	WriteFile(folder.Path() / "notes.json", "{}");

	const std::set<uint32_t> running = {100, 300, 400};
	const auto games = ReadGames(folder.Path(), [&running](uint32_t pid) { return running.contains(pid); });

	ASSERT_EQ(games.size(), 1);
	EXPECT_EQ(games[0].pid, 100);
	EXPECT_EQ(games[0].port, 47801);
	EXPECT_TRUE(std::filesystem::exists(FileFor(folder.Path(), 100)));
	EXPECT_FALSE(std::filesystem::exists(FileFor(folder.Path(), 200)));
	EXPECT_TRUE(std::filesystem::exists(FileFor(folder.Path(), 300)));
	EXPECT_TRUE(std::filesystem::exists(folder.Path() / "frame_12.png"));
	EXPECT_TRUE(std::filesystem::exists(folder.Path() / "notes.json"));
}

// Pictures taken without a path go in each game's own folder, apart from the discovery files, so two games taking a
// picture at the same frame never write the same file
TEST(InspectorDiscovery, EachGameHasItsOwnShotsFolder)
{
	const std::filesystem::path folder = "discovery";
	const auto first = ShotsFolder(folder, 100) / "frame_12.png";
	const auto second = ShotsFolder(folder, 200) / "frame_12.png";
	EXPECT_EQ(first, folder / "shots" / "100" / "frame_12.png");
	EXPECT_EQ(second, folder / "shots" / "200" / "frame_12.png");
	EXPECT_NE(first, second);
	EXPECT_NE(first.parent_path(), folder);
}

TEST(InspectorDiscovery, AGameRemovesItsShotsFolderAsItGoes)
{
	const Folder folder("shots-own");
	const auto own = ShotsFolder(folder.Path(), 4321);
	const auto other = ShotsFolder(folder.Path(), 1234);
	{
		const DiscoveryFile file(folder.Path(), Record(4321, 47801));
		std::filesystem::create_directories(own);
		std::filesystem::create_directories(other);
		WriteFile(own / "frame_12.png", "picture");
		WriteFile(other / "frame_12.png", "picture");
	}
	EXPECT_FALSE(std::filesystem::exists(own));
	EXPECT_TRUE(std::filesystem::exists(other / "frame_12.png"));
}

// The shots folder of a game whose process has gone is removed by whoever reads the games, with or without its file
TEST(InspectorDiscovery, ReadingRemovesTheShotsOfGamesThatHaveGone)
{
	const Folder folder("shots-stale");
	WriteFile(FileFor(folder.Path(), 100), Dump(ToJson(Record(100, 47801))));
	WriteFile(FileFor(folder.Path(), 200), Dump(ToJson(Record(200, 47802))));
	for (const uint32_t pid : {100U, 200U, 300U})
	{
		std::filesystem::create_directories(ShotsFolder(folder.Path(), pid));
		WriteFile(ShotsFolder(folder.Path(), pid) / "frame_1.png", "picture");
	}
	const auto shots = folder.Path() / "shots";
	std::filesystem::create_directories(shots / "notes");
	WriteFile(shots / "400", "a file, not a game's folder");

	const std::set<uint32_t> running = {100};
	const auto games = ReadGames(folder.Path(), [&running](uint32_t pid) { return running.contains(pid); });

	ASSERT_EQ(games.size(), 1);
	EXPECT_TRUE(std::filesystem::exists(ShotsFolder(folder.Path(), 100) / "frame_1.png"));
	EXPECT_FALSE(std::filesystem::exists(ShotsFolder(folder.Path(), 200)));
	EXPECT_FALSE(std::filesystem::exists(ShotsFolder(folder.Path(), 300)));
	EXPECT_TRUE(std::filesystem::exists(shots / "notes"));
	EXPECT_TRUE(std::filesystem::exists(shots / "400"));
}

TEST(InspectorDiscovery, ReadingAMissingFolderFindsNothing)
{
	const Folder folder("none");
	EXPECT_TRUE(ReadGames(folder.Path() / "absent", [](uint32_t) { return true; }).empty());
}

TEST(InspectorDiscovery, ThisProcessIsAlive)
{
	EXPECT_TRUE(ProcessAlive(CurrentProcessId()));
	EXPECT_FALSE(ProcessAlive(0));
	EXPECT_FALSE(ExecutablePath().empty());
}

// A checkout has a .git folder, a worktree a .git file: either marks the nearest folder above
TEST(InspectorDiscovery, TheWorktreeIsTheNearestFolderWithGit)
{
	const Folder folder("worktree");
	const auto checkout = folder.Path() / "checkout";
	std::filesystem::create_directories(checkout / ".git");
	std::filesystem::create_directories(checkout / "build" / "bin");
	const auto worktree = checkout / "build" / "worktree";
	std::filesystem::create_directories(worktree / "bin");
	WriteFile(worktree / ".git", "gitdir: elsewhere");

	auto found = FindWorktree(checkout / "build" / "bin");
	ASSERT_TRUE(found.has_value());
	EXPECT_TRUE(std::filesystem::equivalent(*found, checkout));
	found = FindWorktree(worktree / "bin");
	ASSERT_TRUE(found.has_value());
	EXPECT_TRUE(std::filesystem::equivalent(*found, worktree));
}
