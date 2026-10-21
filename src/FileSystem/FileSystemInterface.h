/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cassert>
#include <cstdint>

#include <filesystem>
#include <functional>
#include <iosfwd>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "Stream.h"

namespace openblack::filesystem
{
enum class Path
{
	Audio,
	Scripts,
	Data,
	Playgrounds,
	Quests,
	CreatureMind,
	Misc,
	Symbols,
	Landscape,
	Textures,
	WeatherSystem,
	CreatureMesh,
	Citadel,
	CitadelEngine,
};

class FileSystemInterface
{
public:
	static std::filesystem::path FixPath(const std::filesystem::path& path);
	/// Finds a file or folder under a root whatever the letter case of each part of the relative path, as the game's own
	/// file system does. A "." part is passed over. Nothing is returned when a part has no match, or the path climbs up
	/// out of the root.
	[[nodiscard]] static std::optional<std::filesystem::path> FindIgnoringCase(const std::filesystem::path& root,
	                                                                           const std::filesystem::path& relative);

	virtual ~FileSystemInterface();

	template <Path pathType>
	[[nodiscard]] std::filesystem::path GetPath(bool withGamePath = false) const
	{
		std::filesystem::path result;
		if constexpr (pathType == Path::Audio)
		{
			result = "Audio";
		}
		else if constexpr (pathType == Path::Scripts)
		{
			result = "Scripts";
		}
		else if constexpr (pathType == Path::Data)
		{
			result = "Data";
		}
		else if constexpr (pathType == Path::Playgrounds)
		{
			result = GetPath<Path::Scripts>() / "Playgrounds";
		}
		else if constexpr (pathType == Path::Quests)
		{
			result = GetPath<Path::Scripts>() / "Quests";
		}
		else if constexpr (pathType == Path::CreatureMind)
		{
			result = GetPath<Path::Scripts>() / "CreatureMind";
		}
		else if constexpr (pathType == Path::Misc)
		{
			result = GetPath<Path::Data>() / "Misc";
		}
		else if constexpr (pathType == Path::Symbols)
		{
			result = GetPath<Path::Data>() / "Symbols";
		}
		else if constexpr (pathType == Path::Landscape)
		{
			result = GetPath<Path::Data>() / "Landscape";
		}
		else if constexpr (pathType == Path::Textures)
		{
			result = GetPath<Path::Data>() / "Textures";
		}
		else if constexpr (pathType == Path::WeatherSystem)
		{
			result = GetPath<Path::Data>() / "WeatherSystem";
		}
		else if constexpr (pathType == Path::CreatureMesh)
		{
			result = GetPath<Path::Data>() / "CreatureMesh";
		}
		else if constexpr (pathType == Path::Citadel)
		{
			result = GetPath<Path::Data>() / "Citadel";
		}
		else if constexpr (pathType == Path::CitadelEngine)
		{
			result = GetPath<Path::Citadel>() / "engine";
		}
		else
		{
			assert(false);
		}
		return withGamePath ? (GetGamePath() / result) : result;
	}

	[[nodiscard]] virtual std::filesystem::path FindPath(const std::filesystem::path& path) const = 0;
	virtual std::unique_ptr<std::istream> GetData(const std::filesystem::path& path) = 0;
	[[nodiscard]] virtual bool IsPathValid(const std::filesystem::path& path) = 0;
	virtual std::unique_ptr<Stream> Open(const std::filesystem::path& path, Stream::Mode mode) = 0;
	[[nodiscard]] virtual bool Exists(const std::filesystem::path& path) const = 0;
	virtual void SetGamePath(const std::filesystem::path& path) = 0;
	[[nodiscard]] virtual const std::filesystem::path& GetGamePath() const = 0;
	virtual void AddAdditionalPath(const std::filesystem::path& path) = 0;
	virtual std::vector<uint8_t> ReadAll(const std::filesystem::path& path) = 0;
	virtual void Iterate(const std::filesystem::path& path, bool recursive,
	                     const std::function<void(const std::filesystem::path&)>& function) const = 0;

	/// Makes the folder at the exact path, and each folder above it that is missing. The game only reads its own
	/// folder, thus this makes nothing there. Returns true when the folder is there after the call.
	virtual bool MakeFolder(const std::filesystem::path& path);
	/// Writes the bytes to the file at the exact path, in place of its contents. The game only reads its own folder,
	/// thus this writes nothing there. Returns true when it wrote all the bytes.
	virtual bool WriteAll(const std::filesystem::path& path, std::span<const uint8_t> bytes);
	/// How far a write of a file got
	enum class WriteResult : uint8_t
	{
		/// Every byte is in the file
		Written,
		/// The file did not open, so nothing changed
		NotOpened,
		/// The file opened, but not every byte went in
		Incomplete,
	};
	/// Writes the bytes to the file at the exact path, in place of its contents, as WriteAll does, and tells how far it
	/// got
	virtual WriteResult WriteFile(const std::filesystem::path& path, std::span<const uint8_t> bytes);
	/// Writes the bytes over those of the file at the exact path, from a place counted from its start. The file must be
	/// there already; its other bytes stay. Returns true when it wrote all the bytes.
	virtual bool WriteAt(const std::filesystem::path& path, uint64_t offset, std::span<const uint8_t> bytes);
	/// Gives a file a new path in the same folder or another. Returns true when the file has the new path.
	virtual bool Rename(const std::filesystem::path& from, const std::filesystem::path& to);
	/// Removes a file. Returns true when the file was there and is gone.
	virtual bool Remove(const std::filesystem::path& path);
	/// Removes every file and folder in the folder at the exact path. The folder itself stays. The game only reads its
	/// own folder, thus this removes nothing there. Returns true when the folder is there and empty after the call.
	virtual bool EmptyFolder(const std::filesystem::path& path);
	/// True when the path is in the game's folder
	[[nodiscard]] bool InGameFolder(const std::filesystem::path& path) const;
};

} // namespace openblack::filesystem
