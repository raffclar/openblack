/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

/*
 * ob-link: the link launcher that ob-build puts in front of each link.
 *
 * Usage: ob-link.exe <link-mb> <reserve-mb> <wait-seconds> <link command...>
 *
 * A Debug link of the game or the test program can use more than 10 GB of memory, and many builds can run on one
 * machine at the same time. ob-link limits the links of all builds on this machine together:
 *
 * 1. A link takes one of the machine's link slots. The number of slots is the physical memory minus <reserve-mb>,
 *    divided by <link-mb> (minimum 1, maximum 64).
 * 2. With a slot, the link waits until the free memory is more than <link-mb> plus <reserve-mb>.
 *
 * Each wait stops after <wait-seconds>; then the link starts anyway, so a build never stops. ob-link writes a note
 * when it waited more than 5 seconds. Each argument before the link command must not contain double quotes.
 */
#define WIN32_LEAN_AND_MEAN
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>

/* Skips one command line token the way the C runtime splits arguments without embedded quotes */
static const wchar_t* SkipToken(const wchar_t* cursor)
{
	while (*cursor == L' ' || *cursor == L'\t')
	{
		++cursor;
	}
	int quoted = 0;
	while (*cursor != L'\0' && (quoted || (*cursor != L' ' && *cursor != L'\t')))
	{
		if (*cursor == L'"')
		{
			quoted = !quoted;
		}
		++cursor;
	}
	while (*cursor == L' ' || *cursor == L'\t')
	{
		++cursor;
	}
	return cursor;
}

static unsigned long long FreeMemoryMb(void)
{
	MEMORYSTATUSEX status;
	status.dwLength = sizeof(status);
	if (!GlobalMemoryStatusEx(&status))
	{
		return 0;
	}
	return status.ullAvailPhys / (1024ull * 1024ull);
}

static unsigned long long TotalMemoryMb(void)
{
	MEMORYSTATUSEX status;
	status.dwLength = sizeof(status);
	if (!GlobalMemoryStatusEx(&status))
	{
		return 0;
	}
	return status.ullTotalPhys / (1024ull * 1024ull);
}

int main(int argc, char** argv)
{
	if (argc < 5)
	{
		fprintf(stderr, "usage: ob-link.exe <link-mb> <reserve-mb> <wait-seconds> <link command...>\n");
		return 2;
	}
	const unsigned long long linkMb = strtoull(argv[1], NULL, 10);
	const unsigned long long reserveMb = strtoull(argv[2], NULL, 10);
	const DWORD waitSeconds = (DWORD)strtoul(argv[3], NULL, 10);

	// Everything after the first four tokens is the link command, exactly as Ninja wrote it
	const wchar_t* command = GetCommandLineW();
	for (int i = 0; i < 4; ++i)
	{
		command = SkipToken(command);
	}
	wchar_t* commandCopy = _wcsdup(command);

	// Each slot is a named mutex: when a link stops without a release (for example, a build that was stopped), the
	// next link gets the abandoned mutex, so no slot is lost
	const unsigned long long total = TotalMemoryMb();
	DWORD slots = 1;
	if (linkMb > 0 && total > reserveMb + linkMb)
	{
		slots = (DWORD)((total - reserveMb) / linkMb);
	}
	if (slots > MAXIMUM_WAIT_OBJECTS)
	{
		slots = MAXIMUM_WAIT_OBJECTS;
	}
	HANDLE mutexes[MAXIMUM_WAIT_OBJECTS];
	DWORD opened = 0;
	for (DWORD i = 0; i < slots; ++i)
	{
		wchar_t name[64];
		swprintf(name, 64, L"Local\\openblack-ob-link-slot-%lu", i);
		HANDLE mutex = CreateMutexW(NULL, FALSE, name);
		if (mutex != NULL)
		{
			mutexes[opened++] = mutex;
		}
	}
	const ULONGLONG start = GetTickCount64();
	HANDLE held = NULL;
	if (opened > 0)
	{
		const DWORD result = WaitForMultipleObjects(opened, mutexes, FALSE, waitSeconds * 1000);
		if (result < WAIT_OBJECT_0 + opened)
		{
			held = mutexes[result - WAIT_OBJECT_0];
		}
		else if (result >= WAIT_ABANDONED_0 && result < WAIT_ABANDONED_0 + opened)
		{
			held = mutexes[result - WAIT_ABANDONED_0];
		}
		else
		{
			fprintf(stderr, "ob-link: note: no link slot came free in %lu s; the link starts anyway\n", waitSeconds);
		}
	}
	while (FreeMemoryMb() < linkMb + reserveMb && GetTickCount64() - start < waitSeconds * 1000ull)
	{
		Sleep(2000);
	}
	const ULONGLONG waited = (GetTickCount64() - start) / 1000;
	if (waited > 5)
	{
		fprintf(stderr, "ob-link: note: waited %llu s for memory (%llu MiB free)\n", waited, FreeMemoryMb());
	}

	STARTUPINFOW startup;
	ZeroMemory(&startup, sizeof(startup));
	startup.cb = sizeof(startup);
	PROCESS_INFORMATION process;
	if (!CreateProcessW(NULL, commandCopy, NULL, NULL, TRUE, 0, NULL, NULL, &startup, &process))
	{
		fprintf(stderr, "ob-link: could not start: %ls (%lu)\n", commandCopy, GetLastError());
		if (held != NULL)
		{
			ReleaseMutex(held);
		}
		return 2;
	}
	WaitForSingleObject(process.hProcess, INFINITE);
	DWORD exitCode = 1;
	GetExitCodeProcess(process.hProcess, &exitCode);
	CloseHandle(process.hProcess);
	CloseHandle(process.hThread);
	if (held != NULL)
	{
		ReleaseMutex(held);
	}
	return (int)exitCode;
}
