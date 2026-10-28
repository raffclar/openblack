/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

/*
 * ob-emcc: the compiler wrapper that ob-web gives Emscripten (EM_COMPILER_WRAPPER), so the web build's compiles go
 * through sccache.
 *
 * Usage (by emcc): ob-emcc.exe <clang command...>, with the environment variable OB_SCCACHE_EXE set to sccache.exe.
 *
 * emcc gives clang the include folders of its system root as "-Xclang -iwithsysroot<folder>". sccache does not
 * cache a compile with an argument after -Xclang that it does not know. The clang driver itself takes
 * "-iwithsysroot<folder>" and gives it to the compiler in the same position, so ob-emcc removes the "-Xclang" before
 * each such argument. All other text of the command stays as emcc wrote it. Then ob-emcc runs sccache with the
 * command and gives its exit code. Without OB_SCCACHE_EXE, ob-emcc runs the command without sccache.
 */
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>

/* Gives the end of the command line token at cursor (the C runtime's splitting, without embedded quotes) */
static const wchar_t* TokenEnd(const wchar_t* cursor)
{
	int quoted = 0;
	while (*cursor != L'\0' && (quoted || (*cursor != L' ' && *cursor != L'\t')))
	{
		if (*cursor == L'"')
		{
			quoted = !quoted;
		}
		++cursor;
	}
	return cursor;
}

static const wchar_t* SkipSpaces(const wchar_t* cursor)
{
	while (*cursor == L' ' || *cursor == L'\t')
	{
		++cursor;
	}
	return cursor;
}

static int TokenIs(const wchar_t* start, const wchar_t* end, const wchar_t* text)
{
	const size_t length = wcslen(text);
	return (size_t)(end - start) == length && wcsncmp(start, text, length) == 0;
}

static int TokenStartsWith(const wchar_t* start, const wchar_t* end, const wchar_t* prefix)
{
	const size_t length = wcslen(prefix);
	return (size_t)(end - start) >= length && wcsncmp(start, prefix, length) == 0;
}

int main(void)
{
	const wchar_t* line = GetCommandLineW();
	const wchar_t* rest = SkipSpaces(TokenEnd(SkipSpaces(line)));

	const wchar_t* sccache = _wgetenv(L"OB_SCCACHE_EXE");
	const size_t restLength = wcslen(rest);
	const size_t capacity = restLength + (sccache != NULL ? wcslen(sccache) : 0) + 8;
	wchar_t* command = (wchar_t*)malloc(capacity * sizeof(wchar_t));
	if (command == NULL)
	{
		return 2;
	}
	command[0] = L'\0';
	if (sccache != NULL && sccache[0] != L'\0')
	{
		wcscat(command, L"\"");
		wcscat(command, sccache);
		wcscat(command, L"\" ");
	}
	size_t used = wcslen(command);

	// Copies the tokens one by one, with the spaces between them, and leaves out each -Xclang before -iwithsysroot
	const wchar_t* cursor = rest;
	while (*cursor != L'\0')
	{
		const wchar_t* start = cursor;
		const wchar_t* end = TokenEnd(start);
		const wchar_t* next = SkipSpaces(end);
		if (sccache != NULL && TokenIs(start, end, L"-Xclang") && *next != L'\0' &&
		    TokenStartsWith(next, TokenEnd(next), L"-iwithsysroot"))
		{
			cursor = next;
			continue;
		}
		memcpy(command + used, start, (size_t)(next - start) * sizeof(wchar_t));
		used += (size_t)(next - start);
		command[used] = L'\0';
		cursor = next;
	}

	STARTUPINFOW startup;
	ZeroMemory(&startup, sizeof(startup));
	startup.cb = sizeof(startup);
	PROCESS_INFORMATION process;
	DWORD exitCode = 2;
	if (!CreateProcessW(NULL, command, NULL, NULL, TRUE, 0, NULL, NULL, &startup, &process))
	{
		fwprintf(stderr, L"ob-emcc: could not start: %ls (%lu)\n", command, GetLastError());
		return 2;
	}
	WaitForSingleObject(process.hProcess, INFINITE);
	GetExitCodeProcess(process.hProcess, &exitCode);
	CloseHandle(process.hProcess);
	CloseHandle(process.hThread);
	free(command);
	return (int)exitCode;
}
