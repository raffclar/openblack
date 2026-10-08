/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string>

namespace openblack
{

/// What the mod loader library (src/ModLoader) reported at start-up. The service (Locator::modLoader) exists only when
/// the library was found and loaded; the debug menu shows it then
struct ModLoaderStatus
{
	int version {0};    ///< the library's version
	std::string result; ///< its one line about the mods folder
};

} // namespace openblack
