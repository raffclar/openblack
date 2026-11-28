/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DrawReadWatch.h"

#include <cstddef>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <utility>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#if defined(_WIN32)
// clang-format off
#include <windows.h>
#include <dbghelp.h>
// clang-format on
#endif

namespace openblack::ecs
{

namespace
{
/// The return addresses of the calling thread, from the innermost call
std::vector<uintptr_t> CaptureStack()
{
	std::vector<uintptr_t> stack;
#if defined(_WIN32)
	// The most return addresses that a read keeps
	constexpr size_t k_StackDepth = 32;
	std::array<void*, k_StackDepth> frames {};
	const auto count = CaptureStackBackTrace(0, static_cast<DWORD>(frames.size()), frames.data(), nullptr);
	stack.reserve(count);
	for (USHORT i = 0; i < count; ++i)
	{
		stack.push_back(reinterpret_cast<uintptr_t>(frames.at(i)));
	}
#endif
	return stack;
}
} // namespace

DrawReadWatch::DrawReadWatch(DrawReadCheck check, std::function<void(const Read&)> onFail)
    : _check(check)
    , _onFail(std::move(onFail))
{
}

void DrawReadWatch::Start() noexcept
{
	_thread = std::this_thread::get_id();
	_watching = _check != DrawReadCheck::Off;
}

void DrawReadWatch::Stop() noexcept
{
	_watching = false;
}

void DrawReadWatch::Note(std::string_view registry, std::string_view call,
                         std::initializer_list<std::string_view> components) noexcept
{
	if (!_watching || std::this_thread::get_id() != _thread)
	{
		return;
	}
	std::string what(call);
	what += '<';
	bool first = true;
	for (const auto component : components)
	{
		if (!first)
		{
			what += ", ";
		}
		what += component;
		first = false;
	}
	what += '>';
	Read found;
	{
		const std::scoped_lock lock(_readsMutex);
		const auto known =
		    std::ranges::find_if(_reads, [&](const Read& read) { return read.registry == registry && read.what == what; });
		if (known != _reads.end())
		{
			++known->count;
			return;
		}
		found =
		    _reads.emplace_back(Read {.registry = std::string(registry), .what = what, .count = 1, .stack = CaptureStack()});
	}
	if (auto logger = spdlog::get("graphics"))
	{
		SPDLOG_LOGGER_WARN(logger, "The draw reads the {} directly: {}", found.registry, found.what);
	}
	if (_check != DrawReadCheck::Fail)
	{
		return;
	}
	if (_onFail)
	{
		_onFail(found);
		return;
	}
	if (auto logger = spdlog::get("graphics"))
	{
		SPDLOG_LOGGER_CRITICAL(logger, "The check of the draw's reads stops the game");
		logger->flush();
	}
	std::abort();
}

std::vector<DrawReadWatch::Read> DrawReadWatch::GetReads() const
{
	const std::scoped_lock lock(_readsMutex);
	return _reads;
}

std::vector<std::string> DrawReadWatch::DescribeStack([[maybe_unused]] std::span<const uintptr_t> stack)
{
	std::vector<std::string> lines;
#if defined(_WIN32)
	// The symbols load only for this call. The crash handler loads them again when it needs them. When another part of
	// the game loaded them already, this call uses them and does not unload them.
	auto* process = GetCurrentProcess();
	if (stack.empty())
	{
		return lines;
	}
	const bool loaded = SymInitialize(process, nullptr, TRUE) != FALSE;
	if (!loaded && GetLastError() != ERROR_INVALID_PARAMETER)
	{
		return lines;
	}
	SymSetOptions(SymGetOptions() | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
	constexpr size_t k_NameSize = 512;
	std::array<std::byte, sizeof(SYMBOL_INFO) + k_NameSize> buffer {};
	auto* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer.data());
	for (const auto address : stack)
	{
		symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
		symbol->MaxNameLen = k_NameSize;
		DWORD64 displacement = 0;
		std::string line =
		    SymFromAddr(process, address, &displacement, symbol) != FALSE ? std::string(symbol->Name) : std::string("?");
		IMAGEHLP_LINE64 source {};
		source.SizeOfStruct = sizeof(source);
		DWORD column = 0;
		if (SymGetLineFromAddr64(process, address, &column, &source) != FALSE)
		{
			line += fmt::format(" ({}:{})", source.FileName, source.LineNumber);
		}
		lines.push_back(std::move(line));
	}
	if (loaded)
	{
		SymCleanup(process);
	}
#endif
	return lines;
}

} // namespace openblack::ecs
