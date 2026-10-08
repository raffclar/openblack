/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// The mod loader: a shared library of its own (ModLoader.dll), which the game loads at start-up when it is there and
// never links against. For now it only checks the mods folder's version; the mod SDK is built on it once the whole game
// is rebuilt. It depends on nothing but the standard library.

#if defined(_WIN32)
#if defined(MODLOADER_BUILD)
#define MODLOADER_API __declspec(dllexport)
#else
#define MODLOADER_API __declspec(dllimport)
#endif
#else
#define MODLOADER_API __attribute__((visibility("default")))
#endif

/// The library's version. A mods folder is read only when its version.txt holds this number, and the game calls the
/// library only when ModLoader_Version() returns the version the game was built with
constexpr int k_ModLoaderVersion = 1;

/// The values ModLoader_LoadMods returns, as an int:
/// 0 = version.txt matches (no mod is loaded yet);
/// 1 = there is no version.txt, or no folder was given: no mod is read;
/// 2 = version.txt cannot be read or does not hold an integer: no mod is read;
/// 3 = version.txt holds another version: no mod is read.
enum class ModLoaderResult : int
{
	Matched = 0,
	NoVersionFile = 1,
	UnreadableVersion = 2,
	VersionMismatch = 3,
};

/// Receives the one line ModLoader_LoadMods reports, with the context it was given
using ModLoaderLogFn = void (*)(const char* line, void* context);

/// The names the game looks the functions up by, and their types
constexpr const char* k_ModLoaderVersionSymbol = "ModLoader_Version";
constexpr const char* k_ModLoaderLoadModsSymbol = "ModLoader_LoadMods";
using ModLoaderVersionFn = int (*)();
using ModLoaderLoadModsFn = int (*)(const char* modsFolder, ModLoaderLogFn log, void* context);

extern "C" {
/// The library's version, k_ModLoaderVersion
MODLOADER_API int ModLoader_Version() noexcept;

/// Reads `<modsFolder>/version.txt`, which holds the mods folder's version as an integer (whitespace around it is
/// allowed). Reports exactly one line through `log` (when it is not null) and returns one of the values above
MODLOADER_API int ModLoader_LoadMods(const char* modsFolder, ModLoaderLogFn log, void* context) noexcept;
}
