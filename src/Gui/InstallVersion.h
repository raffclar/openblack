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

#include <optional>

namespace openblack::gui
{

/// What the game's install records about its version, for the version number of the tips screen
struct InstallVersion
{
	/// The game's version, 100 for 1.00. Empty when the install has no record of it
	std::optional<uint32_t> version;
	/// The number of a developer's patch, 0 for none. Empty when the install has no record of it
	std::optional<uint32_t> developerPatch;
};

/// Reads the records from the current user's settings of the game in the Windows registry. On other systems, or when
/// a record is missing, its value is empty
[[nodiscard]] InstallVersion ReadInstallVersion();

} // namespace openblack::gui
