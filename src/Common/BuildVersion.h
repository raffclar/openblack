/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string_view>

/// The version of this build of openblack. The build writes the definitions from the project's version and the git
/// commit of the sources.
namespace openblack::build_version
{

/// The semantic version of the project, such as "0.1.0"
[[nodiscard]] std::string_view Semantic() noexcept;
/// The first 12 characters of the git commit of the sources, or "unknown" when the build had no git
[[nodiscard]] std::string_view Commit() noexcept;
/// The semantic version and the commit, such as "0.1.0+0123456789ab". The --version option writes it, and the game
/// list compares it with the version of each game server.
[[nodiscard]] std::string_view Text() noexcept;

} // namespace openblack::build_version
