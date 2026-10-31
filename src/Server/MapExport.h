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
#include <optional>
#include <string>

namespace openblack::server
{

/// Writes the file of the network map for 32 gods, as the server makes it from the game folder, to a file. The tools and
/// the operator of a server use it. Gives the error, or none.
[[nodiscard]] std::optional<std::string> WriteGodsRingMap(const std::filesystem::path& gamePath,
                                                          const std::filesystem::path& out);

} // namespace openblack::server
