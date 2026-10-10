/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "InspectorDiscovery.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>

#if defined(_WIN32)
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <csignal>

#include <sys/types.h>
#include <unistd.h>
#endif
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

using namespace openblack::inspector;
using namespace openblack::inspector::discovery;

namespace
{

constexpr std::string_view k_FolderName = "openblack-inspector";
constexpr std::string_view k_Extension = ".json";
constexpr std::string_view k_ShotsFolderName = "shots";

/// The whole text of a file, none if it can't be read
std::optional<std::string> ReadText(const std::filesystem::path& path)
{
	std::ifstream stream(path, std::ios::binary);
	if (!stream)
	{
		return std::nullopt;
	}
	return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

bool WriteText(const std::filesystem::path& path, std::string_view text)
{
	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	if (!stream)
	{
		return false;
	}
	stream.write(text.data(), static_cast<std::streamsize>(text.size()));
	return static_cast<bool>(stream);
}

/// The pid a name of digits gives, none for any other name
std::optional<uint32_t> PidOfName(const std::string& stem)
{
	if (stem.empty() || stem.size() > 10 || !std::ranges::all_of(stem, [](char c) { return c >= '0' && c <= '9'; }))
	{
		return std::nullopt;
	}
	const auto value = std::stoull(stem);
	if (value == 0 || value > UINT32_MAX)
	{
		return std::nullopt;
	}
	return static_cast<uint32_t>(value);
}

/// The pid a file's name gives, none for files that aren't a game's
std::optional<uint32_t> PidOfFile(const std::filesystem::path& path)
{
	if (path.extension() != k_Extension)
	{
		return std::nullopt;
	}
	return PidOfName(path.stem().string());
}

/// Removes the picture folders of games whose processes have gone; anything else in shots is left
void RemoveStaleShots(const std::filesystem::path& folder, const AliveCheck& alive)
{
	std::vector<std::filesystem::path> stale;
	std::error_code error;
	std::filesystem::directory_iterator entries(folder / k_ShotsFolderName, error);
	if (error)
	{
		return;
	}
	for (const std::filesystem::directory_iterator end; entries != end; entries.increment(error))
	{
		if (error)
		{
			break;
		}
		const auto pid = PidOfName(entries->path().filename().string());
		if (pid.has_value() && entries->is_directory(error) && !alive(*pid))
		{
			stale.push_back(entries->path());
		}
	}
	for (const auto& path : stale)
	{
		std::filesystem::remove_all(path, error);
	}
}

} // namespace

Json discovery::ToJson(const GameRecord& record)
{
	return {
	    {"port", record.port},
	    {"pid", record.pid},
	    {"worktree", record.worktree},
	    {"executable", record.executable},
	    {"build_type", record.buildType},
	    {"land", record.land},
	    {"started", record.startTime},
	};
}

std::optional<GameRecord> discovery::FromJson(const Json& value)
{
	if (!value.is_object())
	{
		return std::nullopt;
	}
	const auto port = NumberMember(value, "port");
	const auto pid = NumberMember(value, "pid");
	if (!port.has_value() || !pid.has_value() || *port <= 0 || *port > UINT16_MAX || *pid <= 0 || *pid > UINT32_MAX)
	{
		return std::nullopt;
	}
	return GameRecord {
	    .port = static_cast<uint16_t>(*port),
	    .pid = static_cast<uint32_t>(*pid),
	    .worktree = StringMember(value, "worktree").value_or(""),
	    .executable = StringMember(value, "executable").value_or(""),
	    .buildType = StringMember(value, "build_type").value_or(""),
	    .land = StringMember(value, "land").value_or(""),
	    .startTime = static_cast<int64_t>(NumberMember(value, "started").value_or(0.0)),
	};
}

std::filesystem::path discovery::DefaultFolder()
{
	std::error_code error;
	auto temp = std::filesystem::temp_directory_path(error);
	if (error)
	{
		temp = std::filesystem::path(".");
	}
	return temp / k_FolderName;
}

std::filesystem::path discovery::FileFor(const std::filesystem::path& folder, uint32_t pid)
{
	return folder / (std::to_string(pid) + std::string(k_Extension));
}

std::filesystem::path discovery::ShotsFolder(const std::filesystem::path& folder, uint32_t pid)
{
	return folder / k_ShotsFolderName / std::to_string(pid);
}

uint32_t discovery::CurrentProcessId()
{
#if defined(_WIN32)
	return static_cast<uint32_t>(GetCurrentProcessId());
#else
	return static_cast<uint32_t>(::getpid());
#endif
}

std::filesystem::path discovery::ExecutablePath()
{
#if defined(_WIN32)
	std::wstring buffer(MAX_PATH, L'\0');
	while (true)
	{
		const auto length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
		if (length == 0)
		{
			return {};
		}
		if (length < buffer.size())
		{
			buffer.resize(length);
			return std::filesystem::path(buffer);
		}
		buffer.resize(buffer.size() * 2);
	}
#elif defined(__APPLE__)
	uint32_t size = 0;
	_NSGetExecutablePath(nullptr, &size);
	std::string buffer(size, '\0');
	if (_NSGetExecutablePath(buffer.data(), &size) != 0)
	{
		return {};
	}
	buffer.resize(std::char_traits<char>::length(buffer.c_str()));
	return std::filesystem::path(buffer);
#else
	std::error_code error;
	auto path = std::filesystem::read_symlink("/proc/self/exe", error);
	return error ? std::filesystem::path() : path;
#endif
}

bool discovery::ProcessAlive(uint32_t pid)
{
	if (pid == 0)
	{
		return false;
	}
#if defined(_WIN32)
	HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
	if (process == nullptr)
	{
		// No such process is the one answer meaning it has gone (a game's file is removed on it); another user's can't
		// be opened but is there
		return GetLastError() != ERROR_INVALID_PARAMETER;
	}
	DWORD code = 0;
	const bool asked = GetExitCodeProcess(process, &code) != 0;
	CloseHandle(process);
	return !asked || code == STILL_ACTIVE;
#else
	return ::kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM;
#endif
}

std::optional<std::filesystem::path> discovery::FindWorktree(const std::filesystem::path& from)
{
	std::error_code error;
	auto folder = std::filesystem::weakly_canonical(from, error);
	if (error)
	{
		folder = from;
	}
	while (!folder.empty())
	{
		if (std::filesystem::exists(folder / ".git", error))
		{
			return folder;
		}
		auto parent = folder.parent_path();
		if (parent == folder)
		{
			break;
		}
		folder = std::move(parent);
	}
	return std::nullopt;
}

namespace
{

/// A small text file's first line, without its end; empty when it can't be read
std::string FirstLine(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	std::string line;
	if (!file || !std::getline(file, line))
	{
		return {};
	}
	while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
	{
		line.pop_back();
	}
	return line;
}

/// A folder named in a git file, relative to the folder holding the file unless it is absolute
std::filesystem::path FolderFrom(const std::filesystem::path& base, std::string_view named)
{
	std::filesystem::path folder {std::string(named)};
	return folder.is_absolute() ? folder : base / folder;
}

} // namespace

discovery::Revision discovery::ReadRevision(const std::filesystem::path& worktree)
{
	Revision revision;
	std::error_code error;
	const auto dotGit = worktree / ".git";
	std::filesystem::path gitDir;
	if (std::filesystem::is_directory(dotGit, error))
	{
		gitDir = dotGit;
	}
	else
	{
		// A worktree's .git is a file naming its folder in the main checkout's
		constexpr std::string_view k_GitDir = "gitdir: ";
		const auto line = FirstLine(dotGit);
		if (!line.starts_with(k_GitDir))
		{
			return revision;
		}
		gitDir = FolderFrom(worktree, std::string_view(line).substr(k_GitDir.size()));
	}
	// Refs live in the folder shared by every worktree of the checkout
	const auto common = FirstLine(gitDir / "commondir");
	const auto commonDir = common.empty() ? gitDir : FolderFrom(gitDir, common);
	const auto head = FirstLine(gitDir / "HEAD");
	constexpr std::string_view k_Ref = "ref: ";
	if (!head.starts_with(k_Ref))
	{
		revision.commit = head;
		return revision;
	}
	const auto ref = head.substr(k_Ref.size());
	constexpr std::string_view k_Heads = "refs/heads/";
	revision.branch = ref.starts_with(k_Heads) ? ref.substr(k_Heads.size()) : ref;
	for (const auto& folder : {gitDir, commonDir})
	{
		if (auto commit = FirstLine(folder / ref); !commit.empty())
		{
			revision.commit = std::move(commit);
			return revision;
		}
	}
	// Or packed: "<commit> <ref>" lines
	std::ifstream packed(commonDir / "packed-refs", std::ios::binary);
	std::string line;
	while (packed && std::getline(packed, line))
	{
		if (!line.empty() && line.back() == '\r')
		{
			line.pop_back();
		}
		const auto space = line.find(' ');
		if (space != std::string::npos && std::string_view(line).substr(space + 1) == ref)
		{
			revision.commit = line.substr(0, space);
			break;
		}
	}
	return revision;
}

std::optional<std::filesystem::path> discovery::GameWorktree(const std::filesystem::path& builtFrom,
                                                             const std::filesystem::path& executable)
{
	if (!builtFrom.empty())
	{
		if (auto worktree = FindWorktree(builtFrom); worktree.has_value())
		{
			return worktree;
		}
	}
	return FindWorktree(executable.parent_path());
}

std::vector<GameRecord> discovery::ReadGames(const std::filesystem::path& folder, const AliveCheck& alive)
{
	std::vector<GameRecord> games;
	std::vector<std::filesystem::path> stale;
	std::error_code error;
	std::filesystem::directory_iterator entries(folder, error);
	if (error)
	{
		return games;
	}
	for (const std::filesystem::directory_iterator end; entries != end; entries.increment(error))
	{
		if (error)
		{
			break;
		}
		const auto& path = entries->path();
		const auto pid = PidOfFile(path);
		if (!pid.has_value())
		{
			continue;
		}
		if (!alive(*pid))
		{
			stale.push_back(path);
			continue;
		}
		const auto text = ReadText(path);
		const auto parsed = text.has_value() ? Parse(*text) : std::nullopt;
		auto record = parsed.has_value() ? FromJson(*parsed) : std::nullopt;
		if (record.has_value() && record->pid == *pid)
		{
			games.push_back(std::move(*record));
		}
	}
	for (const auto& path : stale)
	{
		std::filesystem::remove(path, error);
	}
	RemoveStaleShots(folder, alive);
	std::ranges::sort(games, {}, &GameRecord::pid);
	return games;
}

DiscoveryFile::DiscoveryFile(std::filesystem::path folder, GameRecord record)
    : _path(FileFor(folder, record.pid))
    , _record(std::move(record))
{
	std::error_code error;
	std::filesystem::create_directories(folder, error);
	Write();
}

DiscoveryFile::~DiscoveryFile()
{
	std::error_code error;
	std::filesystem::remove(_path, error);
	std::filesystem::remove_all(ShotsFolder(_path.parent_path(), _record.pid), error);
}

void DiscoveryFile::SetLand(const std::string& land)
{
	if (land == _record.land)
	{
		return;
	}
	_record.land = land;
	Write();
}

void DiscoveryFile::Write()
{
	// Written aside and moved into place, so that a reader never sees half a file
	const auto text = Dump(ToJson(_record));
	auto writing = _path;
	writing += ".writing";
	std::error_code error;
	if (WriteText(writing, text))
	{
		// A reader holding the file stops the move for a moment on Windows: it is tried again before writing in place
		constexpr int k_Attempts = 20;
		for (int attempt = 0; attempt < k_Attempts; ++attempt)
		{
			std::filesystem::rename(writing, _path, error);
			if (!error)
			{
				_written = true;
				return;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
		std::filesystem::remove(writing, error);
	}
	// A reader holding the file open can stop the move on some systems: write it in place instead
	_written = WriteText(_path, text);
}
