/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/ProfileFolderSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::filesystem
{
class FileSystemInterface;
}

namespace openblack::ecs::systems
{

class ProfileFolderSystem final: public ProfileFolderSystemInterface
{
public:
	/// The system reads and writes through the file system, which must stay until the system goes
	ProfileFolderSystem(filesystem::FileSystemInterface& fileSystem, std::optional<std::filesystem::path> folder);

	[[nodiscard]] std::optional<std::filesystem::path> GetFolder() const override { return _folder; }
	[[nodiscard]] std::optional<std::filesystem::path> GetPlayerFolder(std::u16string_view player) const override;
	std::optional<std::filesystem::path> MakeFolder(const std::filesystem::path& relative) override;

	[[nodiscard]] std::optional<std::vector<uint8_t>> ReadPlayerFile(std::u16string_view player,
	                                                                 std::string_view file) const override;
	bool WritePlayerFile(std::u16string_view player, std::string_view file, std::span<const uint8_t> bytes) override;
	bool AppendPlayerFile(std::u16string_view player, std::string_view file, std::span<const uint8_t> bytes) override;
	bool EmptyPlayerFolder(std::u16string_view player) override;

private:
	filesystem::FileSystemInterface& _fileSystem;
	std::optional<std::filesystem::path> _folder;
};

} // namespace openblack::ecs::systems
