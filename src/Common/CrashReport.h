/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>
#include <ctime>

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <typeinfo>

/// The pure part of crash handling: what a crash report says, what its files are called and which exit code the game
/// leaves with. The platform hooks that catch crashes live in CrashHandler.
namespace openblack::crash_report
{

enum class CrashKind : uint8_t
{
	Assertion,           ///< A failed runtime assertion
	RuntimeError,        ///< The C runtime reported an error it can't recover from
	InvalidParameter,    ///< A C runtime function was called with an invalid argument
	PureVirtualCall,     ///< A pure virtual function was called, usually on a half destroyed object
	Abort,               ///< abort() was called
	Terminate,           ///< std::terminate was called, usually by an exception nothing caught
	UncaughtException,   ///< An exception reached the top of main
	Signal,              ///< A fatal signal (segmentation fault, illegal instruction, floating point error)
	StructuredException, ///< A hardware or system exception on Windows, such as an access violation
	GraphicsFatal,       ///< The renderer reported an error it can't continue from, such as a lost device
};

/// The exit code for an abort-like crash: the code abort() itself leaves with on Windows, which scripts watch for
constexpr int k_AbortExitCode = 3;

/// One frame of a stack trace; anything that couldn't be resolved is left empty
struct StackFrame
{
	uint64_t address = 0;
	std::string_view module;
	std::string_view symbol;
	uint64_t symbolOffset = 0;
	std::string_view file;
	uint32_t line = 0;
};

struct CrashInfo
{
	CrashKind kind = CrashKind::Abort;
	/// What happened, in a line: the assertion's message, the exception's what() or the renderer's error
	std::string_view message;
	/// The exception's type name for exceptions, the signal's name for signals
	std::string_view detail;
	/// The source location the crash was reported from, when it is known
	std::string_view file;
	uint32_t line = 0;
	/// The signal number or the system exception code
	uint32_t code = 0;
	/// The instruction that faulted and, for access violations, the address it tried to touch
	std::optional<uint64_t> faultAddress;
	std::optional<uint64_t> accessAddress;
	/// For access violations, whether the faulting instruction was reading, writing or executing
	std::string_view accessKind;
	uint64_t threadId = 0;
	std::tm time {};
	/// Where the minidump went, empty when none was written
	std::string_view dumpPath;
	/// The crash came through the signal handler of a POSIX system
	bool fromSignal = false;
};

[[nodiscard]] std::string_view KindName(CrashKind kind);

/// The well-known name of a Windows exception code, such as "EXCEPTION_ACCESS_VIOLATION", or empty when unknown
[[nodiscard]] std::string_view StructuredExceptionName(uint32_t code);

/// The type name of an exception in the form that people read, for example "std::runtime_error". Compilers other
/// than MSVC give a mangled name such as "St13runtime_error", and this function decodes it. A name that it cannot
/// decode comes back as it is.
[[nodiscard]] std::string ReadableTypeName(const char* name);
[[nodiscard]] std::string ReadableTypeName(const std::type_info& type);

/// The name of a POSIX signal number such as "SIGSEGV", or empty when unknown
[[nodiscard]] std::string_view SignalName(int signal);

/// The code the process exits with after reporting: 3 for abort-like crashes, the exception code itself for system
/// exceptions (as Windows would report them), 128 plus the signal for signals elsewhere, and 1 for an exception caught at
/// the top of main, as the game always returned for those
[[nodiscard]] int ExitCodeFor(CrashKind kind, uint32_t code, bool windows);

/// The base name the report's files share, for example "openblack-crash-20261006-142233-4120" for process 4120 (so two
/// games crashing in the same second don't overwrite each other); the report adds ".txt" and the minidump ".dmp"
[[nodiscard]] std::string ReportBaseName(const std::tm& time, uint32_t processId);

/// Reads one frame of WebAssembly code from a line of the stack text that a web browser gives. Chrome writes
/// "at name (url:wasm-function[12]:0x3f4)", and Firefox writes "name@url:wasm-function[12]:0x3f4". The address is the
/// byte offset in the WebAssembly file. The debug file of the build changes this offset to a function and a source
/// line. The module is the last part of the url. The symbol is the name of the function, or "wasm-function[12]" when
/// the file has no names. The frame points into the line. A line of JavaScript code gives no frame.
[[nodiscard]] std::optional<StackFrame> ParseWebStackLine(std::string_view line);

/// Reads the frames of WebAssembly code from the whole stack text of a web browser into out, from the top of the
/// stack. It gives the number of frames that it wrote. The frames point into the text.
[[nodiscard]] size_t ParseWebStack(std::string_view text, std::span<StackFrame> out);

/// The frames left after dropping the crash handler's own frames from the top of a stack it captured itself
[[nodiscard]] std::span<const StackFrame> DropHandlerFrames(std::span<const StackFrame> frames);

/// Many failures end in a bare abort(): assert() prints its expression and aborts, and std::terminate aborts on threads
/// that have no terminate handler of their own. When the stack shows where an abort came from, this relabels the crash
/// as that assertion, at the asserting caller's source line, or as that std::terminate
void AttributeAbort(CrashInfo& info, std::span<const StackFrame> frames);

/// Writes the whole report into out, which is cleared first and keeps its capacity, so a buffer reserved at start-up
/// spares the allocator during a crash
void FormatReport(const CrashInfo& info, std::span<const StackFrame> frames, std::string& out);

} // namespace openblack::crash_report
