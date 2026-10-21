/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SystemLibraries.h"

#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace openblack::system_libraries
{

void LoadBeforeDrivers()
{
#ifdef _WIN32
	for (const auto name : k_LoadFirst)
	{
		// The game does not free the library. The system closes it when the process ends.
		LoadLibraryExW(std::wstring(name).c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
	}
#endif
}

} // namespace openblack::system_libraries
