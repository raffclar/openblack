/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "InstallVersion.h"

#ifdef _WIN32
// clang-format off
// can't sort these includes
#include <wtypes.h>
#include <winreg.h>
// clang-format on
#endif

namespace openblack::gui
{

namespace
{
#ifdef _WIN32
/// The game keeps its settings for the current user in this key
constexpr const char* k_SettingsKey = "SOFTWARE\\Lionhead Studios Ltd\\Black & White";

[[nodiscard]] std::optional<uint32_t> ReadNumber(const char* name)
{
	DWORD value = 0;
	DWORD size = sizeof(value);
	if (RegGetValueA(HKEY_CURRENT_USER, k_SettingsKey, name, RRF_RT_REG_DWORD, nullptr, &value, &size) != ERROR_SUCCESS)
	{
		return std::nullopt;
	}
	return static_cast<uint32_t>(value);
}
#endif
} // namespace

InstallVersion ReadInstallVersion()
{
#ifdef _WIN32
	return {.version = ReadNumber("GameVersion"), .developerPatch = ReadNumber("IsDeveloperPatch")};
#else
	return {};
#endif
}

} // namespace openblack::gui
