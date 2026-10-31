/******************************************************************************
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
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace openblack::filesystem
{
class FileSystemInterface;
}

/// The store of the lands of network games that the player got from the lobby (deviation 170). The game keeps them
/// apart from the game folder, which stays read only:
/// - The desktop: the "Online Maps" folder in the profile folder.
/// - The browser: the IndexedDB storage of the site. The store keeps a copy in memory of each map that it found or
///   wrote in this run, because the start of a game reads the map at once.
namespace openblack::online
{

class OnlineMapStoreInterface
{
public:
	/// Gets true when the store has the map
	using FoundCallback = std::function<void(bool found)>;
	/// Gets true when the store kept all the bytes of the map
	using WrittenCallback = std::function<void(bool written)>;

	virtual ~OnlineMapStoreInterface() = default;

	/// Looks for a map file (for example "obm_32p_1.map") with this start checksum (start_rules::LandChecksum). A
	/// file with another checksum is not the map. The callback runs before the call returns, or later from Poll.
	virtual void Find(std::string file, uint64_t checksum, FoundCallback done) = 0;
	/// The bytes of a map file that the store has now. The browser store gives only the maps that it found or wrote in
	/// this run.
	[[nodiscard]] virtual std::optional<std::vector<uint8_t>> Read(std::string_view file) const = 0;
	/// Keeps the bytes of a map file in place of the file before it. The callback runs before the call returns, or
	/// later from Poll.
	virtual void Write(std::string file, std::vector<uint8_t> bytes, WrittenCallback done) = 0;
	/// Once a frame: runs the callbacks of the work that is done
	virtual void Poll() {}
};

/// The name of the folder of the store in the profile folder
constexpr std::string_view k_OnlineMapFolder = "Online Maps";

/// Whether a name is a map file that the store can keep: a plain file name that ends with ".map", with no folder, no
/// ".." and no character that a file system does not take
[[nodiscard]] bool IsStoreFileName(std::string_view file) noexcept;

/// The desktop store: the "Online Maps" folder in the profile folder. With no profile folder the store keeps nothing.
class FolderMapStore final: public OnlineMapStoreInterface
{
public:
	FolderMapStore(filesystem::FileSystemInterface& files, std::optional<std::filesystem::path> profileFolder);

	void Find(std::string file, uint64_t checksum, FoundCallback done) override;
	[[nodiscard]] std::optional<std::vector<uint8_t>> Read(std::string_view file) const override;
	void Write(std::string file, std::vector<uint8_t> bytes, WrittenCallback done) override;

private:
	[[nodiscard]] std::optional<std::filesystem::path> PathOf(std::string_view file) const;

	filesystem::FileSystemInterface& _files;
	std::optional<std::filesystem::path> _folder;
};

/// The bytes of the land that a start of the game socket names (network_land::WireName). A map of the "Online Maps"
/// folder comes from the store when the store has it, else from the game folder. Any other land comes from the Scripts
/// folder of the game. Nothing when the name is not a land of the game, or the file is not there.
[[nodiscard]] std::optional<std::vector<uint8_t>> ReadStartLand(std::string_view wireName, const OnlineMapStoreInterface* store,
                                                                filesystem::FileSystemInterface& files);

} // namespace openblack::online
