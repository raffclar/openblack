/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CrashHandler.h"

#if defined(_WIN32)

#include <cstdio>
#include <cstdlib>

#include <array>
#include <exception>
#include <stdexcept>
#include <string>

// clang-format off
#include <windows.h>
#include <dbghelp.h>
// clang-format on

#pragma comment(lib, "dbghelp.lib")

namespace
{
void WriteStack(FILE* out, const char* header, CONTEXT context)
{
	const auto print = [out](const char* text) {
		std::fputs(text, stderr);
		if (out != nullptr)
		{
			std::fputs(text, out);
		}
	};
	std::array<char, 1024> line;
	print(header);

	const HANDLE process = GetCurrentProcess();
	const HANDLE thread = GetCurrentThread();
	SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
	SymInitialize(process, nullptr, TRUE);

	STACKFRAME64 frame {};
	frame.AddrPC.Offset = context.Rip;
	frame.AddrPC.Mode = AddrModeFlat;
	frame.AddrFrame.Offset = context.Rbp;
	frame.AddrFrame.Mode = AddrModeFlat;
	frame.AddrStack.Offset = context.Rsp;
	frame.AddrStack.Mode = AddrModeFlat;

	alignas(SYMBOL_INFO) char symbolBuffer[sizeof(SYMBOL_INFO) + 512] {};
	auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
	for (int depth = 0; depth < 48; ++depth)
	{
		if (StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &context, nullptr, SymFunctionTableAccess64,
		                SymGetModuleBase64, nullptr) == FALSE ||
		    frame.AddrPC.Offset == 0)
		{
			break;
		}
		const DWORD64 address = frame.AddrPC.Offset;
		symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
		symbol->MaxNameLen = 511;
		DWORD64 displacement = 0;
		const char* name = SymFromAddr(process, address, &displacement, symbol) != FALSE ? symbol->Name : "?";
		IMAGEHLP_LINE64 source {};
		source.SizeOfStruct = sizeof(source);
		DWORD lineDisplacement = 0;
		std::array<char, MAX_PATH> module {'?'};
		if (const auto base = SymGetModuleBase64(process, address); base != 0)
		{
			GetModuleFileNameA(reinterpret_cast<HMODULE>(base), module.data(), MAX_PATH);
		}
		if (SymGetLineFromAddr64(process, address, &lineDisplacement, &source) != FALSE)
		{
			std::snprintf(line.data(), line.size(), "  #%02d %s (%s:%lu)\n", depth, name, source.FileName, source.LineNumber);
		}
		else
		{
			std::snprintf(line.data(), line.size(), "  #%02d %s +0x%llx [%s]\n", depth, name,
			              static_cast<unsigned long long>(displacement), module.data());
		}
		print(line.data());
	}
	SymCleanup(process);
}

LONG WINAPI OnUnhandledException(EXCEPTION_POINTERS* info)
{
	FILE* out = std::fopen("openblack_crash.txt", "w");
	std::array<char, 256> header;
	std::snprintf(header.data(), header.size(), "openblack crashed: exception 0x%08lX at %p\n",
	              info->ExceptionRecord->ExceptionCode, info->ExceptionRecord->ExceptionAddress);
	WriteStack(out, header.data(), *info->ContextRecord);
	if (out != nullptr)
	{
		std::fclose(out);
	}
	std::fflush(stderr);
	return EXCEPTION_EXECUTE_HANDLER;
}

/// std::terminate (e.g. an exception leaving a noexcept function): the exception's message and the stack
void OnTerminate()
{
	std::string what = "unknown";
	if (const auto current = std::current_exception(); current)
	{
		try
		{
			std::rethrow_exception(current);
		}
		catch (const std::exception& e)
		{
			what = e.what();
		}
		catch (const char* text)
		{
			what = text;
		}
		catch (...)
		{
			what = "non-standard exception";
		}
	}
	FILE* out = std::fopen("openblack_crash.txt", "w");
	const std::string header = "openblack terminated: " + what + "\n";
	CONTEXT context {};
	RtlCaptureContext(&context);
	WriteStack(out, header.c_str(), context);
	if (out != nullptr)
	{
		std::fclose(out);
	}
	std::fflush(stderr);
	std::abort();
}
} // namespace

void openblack::InstallCrashHandler()
{
	SetUnhandledExceptionFilter(OnUnhandledException);
	std::set_terminate(OnTerminate);
}

#else

void openblack::InstallCrashHandler() {}

#endif
