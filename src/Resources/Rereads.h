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
#include <map>
#include <memory>
#include <vector>

namespace openblack::pack
{
class PackFile;
}

namespace openblack::resources
{

/// Reads the game files again after a restore of the graphics device, for the resources that give their data again.
/// It reads each pack file one time for all the resources of the restore.
class Rereads
{
public:
	/// Gives the bytes of a file, or no bytes when the file cannot be read
	using Reader = std::function<std::vector<uint8_t>(const std::filesystem::path&)>;

	explicit Rereads(Reader reader);
	~Rereads();
	Rereads(const Rereads&) = delete;
	Rereads& operator=(const Rereads&) = delete;
	Rereads(Rereads&&) = delete;
	Rereads& operator=(Rereads&&) = delete;

	/// The bytes of a file. It reads the file each time.
	[[nodiscard]] std::vector<uint8_t> File(const std::filesystem::path& path) const;
	/// The pack file, which it reads only the first time. Null when the pack cannot be read.
	[[nodiscard]] const pack::PackFile* Pack(const std::filesystem::path& path);
	/// The number of files that it read, for the tests
	[[nodiscard]] uint32_t ReadCount() const { return _reads; }

private:
	Reader _reader;
	std::map<std::filesystem::path, std::unique_ptr<pack::PackFile>> _packs;
	mutable uint32_t _reads {0};
};

} // namespace openblack::resources
