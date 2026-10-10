/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

/*
 * ob-cc: the compiler launcher that ob-build puts in front of sccache for MSVC builds.
 *
 * Usage: ob-cc.exe <roots-file> <source-root> <build-root> <sccache.exe> <cl.exe> [compiler arguments...]
 *
 * An object compiled in one folder is a cache hit in every other build folder and worktree (sccache runs with every
 * worktree as a base directory and doesn't hash include paths). On a hit sccache replays the original compile's
 * output, and with MSVC that output holds the /showIncludes lines Ninja reads its header dependencies from: they name
 * the headers of the folder that first compiled the object. ob-cc runs the compile, then rewrites every path under
 * another known worktree to this worktree and every path under another build folder to this build folder, so Ninja
 * always records this folder's own headers.
 *
 * The roots file lists one root per line: "S <path>" for a worktree, "B <path>" for a build folder.
 * The first four arguments must not contain double quotes.
 */
#define WIN32_LEAN_AND_MEAN
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

typedef struct
{
	char kind;
	char* path;
	size_t length;
} Root;

static Root g_roots[4096];
static size_t g_rootCount;
static const char* g_sourceRoot;
static const char* g_buildRoot;

static int SameChar(char a, char b)
{
	if (a == '\\')
	{
		a = '/';
	}
	if (b == '\\')
	{
		b = '/';
	}
	if (a >= 'A' && a <= 'Z')
	{
		a = (char)(a - 'A' + 'a');
	}
	if (b >= 'A' && b <= 'Z')
	{
		b = (char)(b - 'A' + 'a');
	}
	return a == b;
}

static int StartsWithPath(const char* text, size_t textLength, const char* root, size_t rootLength)
{
	if (textLength < rootLength)
	{
		return 0;
	}
	for (size_t i = 0; i < rootLength; ++i)
	{
		if (!SameChar(text[i], root[i]))
		{
			return 0;
		}
	}
	// The root must end at a path separator, so C:/a does not match C:/ab
	return textLength == rootLength || text[rootLength] == '/' || text[rootLength] == '\\';
}

static int CompareRootLength(const void* a, const void* b)
{
	const Root* left = (const Root*)a;
	const Root* right = (const Root*)b;
	return left->length < right->length ? 1 : (left->length > right->length ? -1 : 0);
}

static void TrimSeparators(char* path)
{
	size_t length = strlen(path);
	while (length > 0 && (path[length - 1] == '/' || path[length - 1] == '\\' || path[length - 1] == '\r' ||
	                      path[length - 1] == '\n' || path[length - 1] == ' '))
	{
		path[--length] = '\0';
	}
}

static void LoadRoots(const char* file)
{
	FILE* handle = fopen(file, "rb");
	if (handle == NULL)
	{
		return;
	}
	char line[2048];
	while (g_rootCount < sizeof(g_roots) / sizeof(g_roots[0]) && fgets(line, sizeof(line), handle) != NULL)
	{
		TrimSeparators(line);
		if ((line[0] != 'S' && line[0] != 'B') || line[1] != ' ' || line[2] == '\0')
		{
			continue;
		}
		Root* root = &g_roots[g_rootCount++];
		root->kind = line[0];
		root->path = _strdup(line + 2);
		root->length = strlen(root->path);
	}
	fclose(handle);
	qsort(g_roots, g_rootCount, sizeof(g_roots[0]), CompareRootLength);
}

static void WriteAll(HANDLE handle, const char* data, size_t length)
{
	while (length > 0)
	{
		DWORD written = 0;
		if (!WriteFile(handle, data, (DWORD)length, &written, NULL) || written == 0)
		{
			return;
		}
		data += written;
		length -= written;
	}
}

/* Writes the compiler's output with every path under another folder's root moved to this folder's root */
static void WriteRewritten(HANDLE out, const char* text, size_t length)
{
	size_t copied = 0;
	for (size_t i = 0; i < length; ++i)
	{
		const char previous = i == 0 ? '\n' : text[i - 1];
		if (!(previous == '\n' || previous == ' ' || previous == '\t' || previous == '"' || previous == '\'' ||
		      previous == '(' || previous == '<' || previous == '='))
		{
			continue;
		}
		for (size_t r = 0; r < g_rootCount; ++r)
		{
			const Root* root = &g_roots[r];
			if (!StartsWithPath(text + i, length - i, root->path, root->length))
			{
				continue;
			}
			const char* own = root->kind == 'S' ? g_sourceRoot : g_buildRoot;
			const size_t ownLength = strlen(own);
			const int isOwn = root->length == ownLength && StartsWithPath(root->path, root->length, own, ownLength);
			if (!isOwn)
			{
				WriteAll(out, text + copied, i - copied);
				WriteAll(out, own, strlen(own));
				copied = i + root->length;
			}
			i += root->length - 1;
			break;
		}
	}
	WriteAll(out, text + copied, length - copied);
}

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

int main(int argc, char** argv)
{
	if (argc < 6)
	{
		fprintf(stderr, "usage: ob-cc.exe <roots-file> <source-root> <build-root> <sccache.exe> <compiler> [args...]\n");
		return 2;
	}
	LoadRoots(argv[1]);
	g_sourceRoot = argv[2];
	g_buildRoot = argv[3];

	// Everything from the launcher on is passed to CreateProcess exactly as Ninja wrote it
	const wchar_t* command = GetCommandLineW();
	for (int i = 0; i < 4; ++i)
	{
		command = SkipToken(command);
	}
	wchar_t* commandCopy = _wcsdup(command);

	SECURITY_ATTRIBUTES security = {sizeof(security), NULL, TRUE};
	HANDLE readEnd = NULL;
	HANDLE writeEnd = NULL;
	if (!CreatePipe(&readEnd, &writeEnd, &security, 0))
	{
		fprintf(stderr, "ob-cc: CreatePipe failed (%lu)\n", GetLastError());
		return 2;
	}
	SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);

	STARTUPINFOW startup;
	ZeroMemory(&startup, sizeof(startup));
	startup.cb = sizeof(startup);
	startup.dwFlags = STARTF_USESTDHANDLES;
	startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	startup.hStdOutput = writeEnd;
	startup.hStdError = writeEnd;
	PROCESS_INFORMATION process;
	if (!CreateProcessW(NULL, commandCopy, NULL, NULL, TRUE, 0, NULL, NULL, &startup, &process))
	{
		fprintf(stderr, "ob-cc: could not start: %ls (%lu)\n", commandCopy, GetLastError());
		return 2;
	}
	CloseHandle(writeEnd);

	size_t capacity = 1 << 16;
	size_t length = 0;
	char* output = (char*)malloc(capacity);
	for (;;)
	{
		if (length + 4096 > capacity)
		{
			capacity *= 2;
			output = (char*)realloc(output, capacity);
		}
		DWORD got = 0;
		if (!ReadFile(readEnd, output + length, (DWORD)(capacity - length), &got, NULL) || got == 0)
		{
			break;
		}
		length += got;
	}
	CloseHandle(readEnd);

	WaitForSingleObject(process.hProcess, INFINITE);
	DWORD exitCode = 1;
	GetExitCodeProcess(process.hProcess, &exitCode);
	CloseHandle(process.hProcess);
	CloseHandle(process.hThread);

	WriteRewritten(GetStdHandle(STD_OUTPUT_HANDLE), output, length);
	return (int)exitCode;
}
