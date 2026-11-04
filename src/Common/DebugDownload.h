/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <filesystem>

namespace openblack::debug_download
{

/// True when the file is not inside the folder. The paths are compared as text and the file system is not read.
[[nodiscard]] inline bool IsOutsideFolder(const std::filesystem::path& file, const std::filesystem::path& folder)
{
	const auto relative = file.lexically_normal().lexically_relative(folder.lexically_normal());
	return relative.empty() || *relative.begin() == "..";
}

/// Tell the player's browser that the game wrote a debug file, so that the page gives a link to download it. Call after
/// the file is complete. The web build with the developer tools does this for a file outside the Profiles folder. All
/// other builds do nothing, because they write to a folder that the player can open.
void Offer(const std::filesystem::path& file);

} // namespace openblack::debug_download
