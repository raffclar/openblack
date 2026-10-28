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
 * machine at the same time. ob-link limits the large links of all builds on this machine together:
 *
 * 1. The memory that a link needs is about the size of its object files and libraries. ob-link adds the sizes of the
 *    .obj and .lib files in the command and in its response files (@file). <link-mb> is the largest need.
 * 2. A link that needs less than 2 GB starts at once.
 * 3. A larger link takes one of the machine's link slots. The number of slots is the physical memory minus
 *    <reserve-mb>, divided by <link-mb> (minimum 1, maximum 64).
 * 4. With a slot, the link waits until the free memory is more than its need plus <reserve-mb>.
 *
 * The waits stop after <wait-seconds>; then the link starts anyway, so a build never stops. ob-link writes a note when
 * it waited more than 5 seconds. Each argument before the link command must not contain double quotes.
 */
#define WIN32_LEAN_AND_MEAN
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>

static const unsigned long long k_SmallLinkMb = 2048;

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
	return GlobalMemoryStatusEx(&status) ? status.ullAvailPhys / (1024ull * 1024ull) : 0;
}

static unsigned long long TotalMemoryMb(void)
{
	MEMORYSTATUSEX status;
	status.dwLength = sizeof(status);
	return GlobalMemoryStatusEx(&status) ? status.ullTotalPhys / (1024ull * 1024ull) : 0;
}

static int EndsWithNoCase(const char* text, size_t length, const char* suffix)
{
	const size_t suffixLength = strlen(suffix);
	return length >= suffixLength && _strnicmp(text + length - suffixLength, suffix, suffixLength) == 0;
}

static unsigned long long FileSize(const char* path)
{
	WIN32_FILE_ATTRIBUTE_DATA data;
	if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data) || (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
	{
		return 0;
	}
	return ((unsigned long long)data.nFileSizeHigh << 32) | data.nFileSizeLow;
}

static unsigned long long InputBytes(const char* text, size_t length, int depth);

/* Gives the size of one token: an object file, a library, or a response file with more tokens */
static unsigned long long TokenBytes(const char* token, size_t length, int depth)
{
	char path[MAX_PATH];
	if (length == 0 || length >= sizeof(path))
	{
		return 0;
	}
	if (token[0] == '@' && depth < 2)
	{
		memcpy(path, token + 1, length - 1);
		path[length - 1] = '\0';
		FILE* file = fopen(path, "rb");
		if (file == NULL)
		{
			return 0;
		}
		size_t capacity = 1 << 16;
		size_t used = 0;
		char* content = (char*)malloc(capacity);
		size_t got = 0;
		while (content != NULL && (got = fread(content + used, 1, capacity - used, file)) > 0)
		{
			used += got;
			if (used == capacity)
			{
				capacity *= 2;
				char* grown = (char*)realloc(content, capacity);
				if (grown == NULL)
				{
					break;
				}
				content = grown;
			}
		}
		fclose(file);
		const unsigned long long bytes = content != NULL ? InputBytes(content, used, depth + 1) : 0;
		free(content);
		return bytes;
	}
	if (!EndsWithNoCase(token, length, ".obj") && !EndsWithNoCase(token, length, ".lib"))
	{
		return 0;
	}
	memcpy(path, token, length);
	path[length] = '\0';
	return FileSize(path);
}

/* Adds the sizes of the object files and libraries that a command line or a response file names */
static unsigned long long InputBytes(const char* text, size_t length, int depth)
{
	unsigned long long bytes = 0;
	size_t i = 0;
	while (i < length)
	{
		while (i < length && (text[i] == ' ' || text[i] == '\t' || text[i] == '\r' || text[i] == '\n'))
		{
			++i;
		}
		if (i >= length)
		{
			break;
		}
		size_t start = i;
		size_t end = i;
		if (text[i] == '"')
		{
			start = ++i;
			while (i < length && text[i] != '"')
			{
				++i;
			}
			end = i;
			if (i < length)
			{
				++i;
			}
		}
		else
		{
			while (i < length && text[i] != ' ' && text[i] != '\t' && text[i] != '\r' && text[i] != '\n')
			{
				++i;
			}
			end = i;
		}
		bytes += TokenBytes(text + start, end - start, depth);
	}
	return bytes;
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

	unsigned long long needMb = 0;
	for (int i = 5; i < argc; ++i)
	{
		needMb += TokenBytes(argv[i], strlen(argv[i]), 0) / (1024ull * 1024ull);
	}
	if (needMb > linkMb)
	{
		needMb = linkMb;
	}

	HANDLE mutexes[MAXIMUM_WAIT_OBJECTS];
	DWORD opened = 0;
	HANDLE held = NULL;
	const ULONGLONG start = GetTickCount64();
	if (needMb >= k_SmallLinkMb)
	{
		// Each slot is a named mutex: when a link stops without a release (for example, a build that was stopped),
		// the next link gets the abandoned mutex, so no slot is lost
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
		while (FreeMemoryMb() < needMb + reserveMb && GetTickCount64() - start < waitSeconds * 1000ull)
		{
			Sleep(2000);
		}
		const ULONGLONG waited = (GetTickCount64() - start) / 1000;
		if (waited > 5)
		{
			fprintf(stderr, "ob-link: note: a link that needs about %llu MiB waited %llu s (%llu MiB free)\n", needMb,
			        waited, FreeMemoryMb());
		}
	}

	STARTUPINFOW startup;
	ZeroMemory(&startup, sizeof(startup));
	startup.cb = sizeof(startup);
	PROCESS_INFORMATION process;
	DWORD exitCode = 2;
	if (CreateProcessW(NULL, commandCopy, NULL, NULL, TRUE, 0, NULL, NULL, &startup, &process))
	{
		WaitForSingleObject(process.hProcess, INFINITE);
		GetExitCodeProcess(process.hProcess, &exitCode);
		CloseHandle(process.hProcess);
		CloseHandle(process.hThread);
	}
	else
	{
		fprintf(stderr, "ob-link: could not start: %ls (%lu)\n", commandCopy, GetLastError());
	}
	if (held != NULL)
	{
		ReleaseMutex(held);
	}
	for (DWORD i = 0; i < opened; ++i)
	{
		CloseHandle(mutexes[i]);
	}
	return (int)exitCode;
}
