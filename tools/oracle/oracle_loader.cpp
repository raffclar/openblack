/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The oracle loader (oracle_host.exe). Windows maps a program before it makes heaps or maps other files, so this
// program is fixed at the game executable's base and holds a large empty section. Thus the game executable's address
// range is free for the oracle core (oracle_core.dll), which this program loads and runs. It uses no C runtime: the
// core overwrites this program's memory, so this program never runs again after it calls the core.
//
// Usage: oracle_host.exe <path to the game executable>

#if !defined(_M_IX86)
#error The oracle loader must be built for 32-bit x86
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// The address range that the core fills with the game executable: 16 MB from the base
#pragma bss_seg(".range")
char g_range[0x1000000];
#pragma bss_seg()

using OracleRun = int(__cdecl*)(const char* exePath);

extern "C" void __stdcall LoaderEntry()
{
	*static_cast<volatile char*>(g_range) = 0;
	// The core is next to this program
	wchar_t path[MAX_PATH + 32];
	DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
	while (length > 0 && path[length - 1] != L'\\' && path[length - 1] != L'/')
	{
		--length;
	}
	const wchar_t core[] = L"oracle_core.dll";
	for (DWORD i = 0; i < sizeof(core) / sizeof(core[0]); ++i)
	{
		path[length + i] = core[i];
	}
	HMODULE module = LoadLibraryW(path);
	if (module == nullptr)
	{
		ExitProcess(3);
	}
	auto run = reinterpret_cast<OracleRun>(GetProcAddress(module, "OracleRun"));
	if (run == nullptr)
	{
		ExitProcess(3);
	}
	// The only argument is the game executable's path: skip this program's name (quoted or not), then spaces
	const char* line = GetCommandLineA();
	if (*line == '"')
	{
		++line;
		while (*line != '\0' && *line != '"')
		{
			++line;
		}
		if (*line == '"')
		{
			++line;
		}
	}
	else
	{
		while (*line != '\0' && *line != ' ' && *line != '\t')
		{
			++line;
		}
	}
	while (*line == ' ' || *line == '\t')
	{
		++line;
	}
	static char exePath[MAX_PATH * 2] = {};
	DWORD at = 0;
	const bool quoted = *line == '"';
	if (quoted)
	{
		++line;
	}
	while (*line != '\0' && at + 1 < sizeof(exePath) && (quoted ? *line != '"' : (*line != ' ' && *line != '\t')))
	{
		exePath[at++] = *line++;
	}
	ExitProcess(static_cast<UINT>(run(exePath)));
}
