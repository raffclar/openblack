/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

/*
 * ob-job: the CPU meter that ob-build runs each build command in.
 *
 * Usage: ob-job.exe <log-file> <note-file> <label> <command...>
 *
 * ob-job runs the command in a Windows job object. The job holds every process that the command starts, also the
 * build's own sccache server and its compiler processes. When the command stops, ob-job adds one line to <log-file>:
 * the time, the label, the exit code, the wall time, the CPU time of all the processes of the job, the number of
 * processes and the peak memory of the job. The text in <note-file> (written by ob-build, for example the cache hits
 * and misses) goes at the end of the line, then ob-job deletes <note-file>. ob-job gives the exit code of the command.
 * The first three arguments must not contain spaces or double quotes.
 */
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

/* Skips one command line token the way the C runtime splits arguments without embedded quotes */
static const char* SkipToken(const char* cursor)
{
	while (*cursor == ' ' || *cursor == '\t')
	{
		++cursor;
	}
	int quoted = 0;
	while (*cursor != '\0' && (quoted || (*cursor != ' ' && *cursor != '\t')))
	{
		if (*cursor == '"')
		{
			quoted = !quoted;
		}
		++cursor;
	}
	while (*cursor == ' ' || *cursor == '\t')
	{
		++cursor;
	}
	return cursor;
}

/* Reads the first line of a small text file into buffer (empty when the file does not exist), then deletes the file */
static void TakeNote(const char* path, char* buffer, size_t size)
{
	buffer[0] = '\0';
	FILE* file = fopen(path, "rb");
	if (file == NULL)
	{
		return;
	}
	const size_t got = fread(buffer, 1, size - 1, file);
	fclose(file);
	buffer[got] = '\0';
	for (size_t i = 0; i < got; ++i)
	{
		if (buffer[i] == '\r' || buffer[i] == '\n')
		{
			buffer[i] = ' ';
		}
	}
	DeleteFileA(path);
}

int main(int argc, char** argv)
{
	if (argc < 5)
	{
		fprintf(stderr, "usage: ob-job.exe <log-file> <note-file> <label> <command...>\n");
		return 2;
	}
	const char* command = GetCommandLineA();
	for (int i = 0; i < 4; ++i)
	{
		command = SkipToken(command);
	}
	char* commandCopy = _strdup(command);

	// Without a job (for example, when the system refuses one), the command still runs, and the line has no CPU time
	HANDLE job = CreateJobObjectA(NULL, NULL);
	STARTUPINFOA startup;
	ZeroMemory(&startup, sizeof(startup));
	startup.cb = sizeof(startup);
	PROCESS_INFORMATION process;
	const ULONGLONG start = GetTickCount64();
	if (!CreateProcessA(NULL, commandCopy, NULL, NULL, TRUE, CREATE_SUSPENDED, NULL, NULL, &startup, &process))
	{
		fprintf(stderr, "ob-job: could not start: %s (%lu)\n", commandCopy, GetLastError());
		return 2;
	}
	const int inJob = job != NULL && AssignProcessToJobObject(job, process.hProcess);
	ResumeThread(process.hThread);
	WaitForSingleObject(process.hProcess, INFINITE);
	DWORD exitCode = 2;
	GetExitCodeProcess(process.hProcess, &exitCode);
	const double wall = (GetTickCount64() - start) / 1000.0;

	double user = -1.0;
	double kernel = -1.0;
	unsigned long processes = 0;
	unsigned long long peakMb = 0;
	if (inJob)
	{
		JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting;
		JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits;
		if (QueryInformationJobObject(job, JobObjectBasicAccountingInformation, &accounting, sizeof(accounting), NULL))
		{
			user = accounting.TotalUserTime.QuadPart / 1e7;
			kernel = accounting.TotalKernelTime.QuadPart / 1e7;
			processes = accounting.TotalProcesses;
		}
		if (QueryInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits), NULL))
		{
			peakMb = (unsigned long long)(limits.PeakJobMemoryUsed / (1024 * 1024));
		}
	}

	char note[1024];
	TakeNote(argv[2], note, sizeof(note));
	SYSTEMTIME now;
	GetLocalTime(&now);
	char line[2048];
	const int length = snprintf(line, sizeof(line),
	                            "%04u-%02u-%02u %02u:%02u:%02u %s rc=%lu wall=%.1f cpu=%.1f user=%.1f kernel=%.1f "
	                            "processes=%lu peak_mb=%llu %s\r\n",
	                            now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, argv[3], exitCode, wall,
	                            user + kernel, user, kernel, processes, peakMb, note);
	// One write with append access, so the lines of builds that end at the same time do not mix
	HANDLE log = CreateFileA(argv[1], FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
	                         OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (log != INVALID_HANDLE_VALUE && length > 0)
	{
		DWORD written = 0;
		WriteFile(log, line, (DWORD)strlen(line), &written, NULL);
		CloseHandle(log);
	}
	CloseHandle(process.hProcess);
	CloseHandle(process.hThread);
	return (int)exitCode;
}
