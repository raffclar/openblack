/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "InspectorJson.h"

/// How tools find the games whose inspector is running: while it listens, each game keeps a small JSON file named after
/// its process in a shared folder under the temporary directory, saying which port it is on and what it is. A clean exit
/// removes the file; a file whose process has gone is stale, ignored and removed by whoever reads it. Pictures taken
/// without a path go in the game's own folder, shots/<pid>, which goes the same way as the file.
namespace openblack::inspector::discovery
{

/// What a running game says of itself
struct GameRecord
{
	uint16_t port {0};
	uint32_t pid {0};
	/// The source checkout the game was built from (the nearest folder above the executable holding .git), or empty
	std::string worktree;
	std::string executable;
	/// Debug, RelWithDebInfo and the like
	std::string buildType;
	/// The land loaded now, empty while none is
	std::string land;
	/// When the game started, in seconds since 1970
	int64_t startTime {0};
};

[[nodiscard]] Json ToJson(const GameRecord& record);
/// The record a file holds, none if it isn't one (no port or pid)
[[nodiscard]] std::optional<GameRecord> FromJson(const Json& value);

/// The shared folder: openblack-inspector in the system's temporary directory
[[nodiscard]] std::filesystem::path DefaultFolder();
/// A game's file in a folder: <pid>.json
[[nodiscard]] std::filesystem::path FileFor(const std::filesystem::path& folder, uint32_t pid);
/// A game's own folder for pictures taken without a path: shots/<pid> in the folder, apart from the files and from
/// every other game's
[[nodiscard]] std::filesystem::path ShotsFolder(const std::filesystem::path& folder, uint32_t pid);

[[nodiscard]] uint32_t CurrentProcessId();
/// The running program's file, empty if the system won't say
[[nodiscard]] std::filesystem::path ExecutablePath();
/// Whether a process of that id runs now
[[nodiscard]] bool ProcessAlive(uint32_t pid);
/// The nearest folder at or above a path that holds a .git entry (a checkout or a worktree), none if there is none
[[nodiscard]] std::optional<std::filesystem::path> FindWorktree(const std::filesystem::path& from);

using AliveCheck = std::function<bool(uint32_t pid)>;

/// The games whose files are in a folder and whose processes run, by pid. Files and picture folders of processes that
/// have gone are removed; files that can't be read are skipped.
[[nodiscard]] std::vector<GameRecord> ReadGames(const std::filesystem::path& folder, const AliveCheck& alive);

/// A game's file, written as it is made, rewritten as the record changes and removed as it goes, with the game's picture
/// folder
class DiscoveryFile
{
public:
	DiscoveryFile(std::filesystem::path folder, GameRecord record);
	~DiscoveryFile();
	DiscoveryFile(const DiscoveryFile&) = delete;
	DiscoveryFile& operator=(const DiscoveryFile&) = delete;
	DiscoveryFile(DiscoveryFile&&) = delete;
	DiscoveryFile& operator=(DiscoveryFile&&) = delete;

	/// Rewrites the file when the land differs from the last written
	void SetLand(const std::string& land);

	[[nodiscard]] const GameRecord& Record() const { return _record; }
	[[nodiscard]] const std::filesystem::path& Path() const { return _path; }
	/// Whether the file could be written last time it was
	[[nodiscard]] bool Written() const { return _written; }

private:
	void Write();

	std::filesystem::path _path;
	GameRecord _record;
	bool _written {false};
};

} // namespace openblack::inspector::discovery
