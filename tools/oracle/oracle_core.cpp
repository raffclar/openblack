/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The oracle host. It is a 32-bit x86 Windows program that maps the original game's executable into its own memory at
// the base the executable needs, and calls one function of it at a time on inputs that the caller gives. It never runs
// the game: it does not run the start-up code, the static constructors or the entry point. Each import of the
// executable stops the call with an error, unless the caller replaces it with a stub. Thus the host cannot open a
// window, play sound or use the network.
//
// The host reads one command on each line of the standard input and writes one answer line to the standard output.
// The answer starts with "ok" or "err". All numbers are hexadecimal without a prefix. tools/oracle/oracle.py drives
// the host; see tools/oracle/README.md for the commands.
//
// Build it with "python tools/oracle/oracle.py build" (MSVC, x86). It is not a part of the game build.

#if !defined(_M_IX86)
#error The oracle host must be built for 32-bit x86
#endif

#define WIN32_LEAN_AND_MEAN
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <array>
#include <string>
#include <vector>

#include <windows.h>

namespace
{

constexpr uint32_t k_ImportTrapCode = 0xE0B10001;
constexpr size_t k_ArenaSize = 64 * 1024 * 1024;
constexpr size_t k_MaxArgs = 64;

struct Image
{
	std::vector<uint8_t> file;
	uint32_t base = 0;
	uint32_t size = 0;
	uint8_t* memory = nullptr;
};

struct Import
{
	std::string dll;
	std::string name;
	uint32_t slot = 0; // The address of its entry in the import address table
};

Image g_image;
std::vector<Import> g_imports;
uint8_t* g_trapCode = nullptr;
uint8_t* g_arena = nullptr;
size_t g_arenaUsed = 0;
uint32_t g_lastImport = 0xFFFFFFFF;

// The state for one call. The call code reads and writes these with inline assembly, so they are global.
uint32_t g_target = 0;
uint32_t g_inEax = 0;
uint32_t g_inEcx = 0;
uint32_t g_inEdx = 0;
uint32_t g_argCount = 0;
uint32_t g_args[k_MaxArgs] = {};
uint16_t g_controlWord = 0x007F;
uint32_t g_savedEsp = 0;
uint32_t g_outEax = 0;
uint32_t g_outEdx = 0;
uint32_t g_outEspDelta = 0;
uint16_t g_outStatus = 0;
uint32_t g_outDepth = 0;
uint8_t g_outSt0[10] = {};
uint32_t g_outSt0Float = 0;
uint32_t g_outSt0Empty = 1;

void Reply(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	std::vfprintf(stdout, format, args);
	va_end(args);
	std::fputc('\n', stdout);
	std::fflush(stdout);
}

bool InImage(uint32_t address, uint32_t length)
{
	return address >= g_image.base && length <= g_image.size && address - g_image.base <= g_image.size - length;
}

bool InArena(uint32_t address, uint32_t length)
{
	const auto start = reinterpret_cast<uint32_t>(g_arena);
	return address >= start && length <= k_ArenaSize && address - start <= k_ArenaSize - length;
}

bool Writable(uint32_t address, uint32_t length)
{
	return InImage(address, length) || InArena(address, length);
}

// Called by the import traps, with the index of the import that the game code called
extern "C" void __stdcall ImportTrapped(uint32_t index)
{
	g_lastImport = index;
	RaiseException(k_ImportTrapCode, 0, 0, nullptr);
}

bool ReadFileBytes(const char* path, std::vector<uint8_t>& out)
{
	FILE* file = std::fopen(path, "rb");
	if (file == nullptr)
	{
		return false;
	}
	std::fseek(file, 0, SEEK_END);
	const long length = std::ftell(file);
	std::fseek(file, 0, SEEK_SET);
	out.resize(static_cast<size_t>(length));
	const size_t read = std::fread(out.data(), 1, out.size(), file);
	std::fclose(file);
	return read == out.size();
}

// Copies the headers and the sections into the reserved memory, and fills the rest with zeros
bool CopySections()
{
	const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(g_image.file.data());
	const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(g_image.file.data() + dos->e_lfanew);
	std::memset(g_image.memory, 0, g_image.size);
	std::memcpy(g_image.memory, g_image.file.data(), nt->OptionalHeader.SizeOfHeaders);
	const auto* section = IMAGE_FIRST_SECTION(nt);
	for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section)
	{
		const uint32_t length = section->SizeOfRawData < section->Misc.VirtualSize || section->Misc.VirtualSize == 0
		                            ? section->SizeOfRawData
		                            : section->Misc.VirtualSize;
		if (section->PointerToRawData + length > g_image.file.size() || section->VirtualAddress + length > g_image.size)
		{
			return false;
		}
		std::memcpy(g_image.memory + section->VirtualAddress, g_image.file.data() + section->PointerToRawData, length);
	}
	return true;
}

// Points every import address table entry at a trap of its own
void TrapImports()
{
	g_imports.clear();
	const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(g_image.memory);
	const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(g_image.memory + dos->e_lfanew);
	const auto& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	if (directory.VirtualAddress == 0)
	{
		return;
	}
	const auto* descriptor = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(g_image.memory + directory.VirtualAddress);
	for (; descriptor->Name != 0; ++descriptor)
	{
		const char* dll = reinterpret_cast<const char*>(g_image.memory + descriptor->Name);
		const uint32_t lookupRva =
		    descriptor->OriginalFirstThunk != 0 ? descriptor->OriginalFirstThunk : descriptor->FirstThunk;
		const auto* lookup = reinterpret_cast<const uint32_t*>(g_image.memory + lookupRva);
		auto* slot = reinterpret_cast<uint32_t*>(g_image.memory + descriptor->FirstThunk);
		for (; *lookup != 0; ++lookup, ++slot)
		{
			Import entry;
			entry.dll = dll;
			if ((*lookup & IMAGE_ORDINAL_FLAG32) != 0)
			{
				entry.name = "#" + std::to_string(*lookup & 0xFFFF);
			}
			else
			{
				entry.name = reinterpret_cast<const char*>(g_image.memory + *lookup + 2);
			}
			entry.slot = reinterpret_cast<uint32_t>(slot);
			g_imports.push_back(entry);
		}
	}
	// Each trap is "push index; call ImportTrapped": 10 bytes
	if (g_trapCode != nullptr)
	{
		VirtualFree(g_trapCode, 0, MEM_RELEASE);
	}
	g_trapCode = static_cast<uint8_t*>(
	    VirtualAlloc(nullptr, g_imports.size() * 16 + 16, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
	for (size_t i = 0; i < g_imports.size(); ++i)
	{
		uint8_t* code = g_trapCode + i * 16;
		code[0] = 0x68; // push imm32
		const auto index = static_cast<uint32_t>(i);
		std::memcpy(code + 1, &index, 4);
		code[5] = 0xE8; // call rel32
		const auto rel = reinterpret_cast<uint32_t>(&ImportTrapped) - reinterpret_cast<uint32_t>(code + 10);
		std::memcpy(code + 6, &rel, 4);
		code[10] = 0xCC;
		const auto trap = reinterpret_cast<uint32_t>(code);
		std::memcpy(reinterpret_cast<void*>(g_imports[i].slot), &trap, 4);
	}
}

bool LoadImage(const char* path, std::string& error)
{
	if (!ReadFileBytes(path, g_image.file))
	{
		error = "cannot read the executable";
		return false;
	}
	if (g_image.file.size() < sizeof(IMAGE_DOS_HEADER))
	{
		error = "not an executable";
		return false;
	}
	const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(g_image.file.data());
	if (dos->e_magic != IMAGE_DOS_SIGNATURE ||
	    static_cast<size_t>(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS32) > g_image.file.size())
	{
		error = "not an executable";
		return false;
	}
	const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(g_image.file.data() + dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386)
	{
		error = "not a 32-bit x86 executable";
		return false;
	}
	g_image.base = nt->OptionalHeader.ImageBase;
	g_image.size = nt->OptionalHeader.SizeOfImage;
	// The loader program (oracle_loader.cpp) is fixed at the same base and holds a large empty section, so the range
	// is already the host's when this code runs. Other memory (heaps, mapped files) takes low addresses early in a
	// process, so a later allocation at the base does not work.
	const auto* loader = reinterpret_cast<const IMAGE_DOS_HEADER*>(GetModuleHandleW(nullptr));
	const auto* loaderNt =
	    reinterpret_cast<const IMAGE_NT_HEADERS32*>(reinterpret_cast<const uint8_t*>(loader) + loader->e_lfanew);
	if (reinterpret_cast<uint32_t>(loader) != g_image.base || loaderNt->OptionalHeader.SizeOfImage < g_image.size)
	{
		error = "the loader does not hold the executable's address range";
		return false;
	}
	g_image.memory = reinterpret_cast<uint8_t*>(g_image.base);
	DWORD previous = 0;
	if (VirtualProtect(g_image.memory, g_image.size, PAGE_EXECUTE_READWRITE, &previous) == 0)
	{
		error = "cannot make the executable's address range writable";
		return false;
	}
	if (!CopySections())
	{
		error = "a section is outside the file or the image";
		return false;
	}
	TrapImports();
	return true;
}

bool ParseHex(const char* text, uint32_t& value)
{
	if (text == nullptr || *text == '\0')
	{
		return false;
	}
	char* end = nullptr;
	value = std::strtoul(text, &end, 16);
	return *end == '\0';
}

bool ParseHexBytes(const char* text, std::vector<uint8_t>& out)
{
	const size_t length = std::strlen(text);
	if (length % 2 != 0)
	{
		return false;
	}
	out.resize(length / 2);
	for (size_t i = 0; i < out.size(); ++i)
	{
		char pair[3] = {text[i * 2], text[i * 2 + 1], '\0'};
		char* end = nullptr;
		out[i] = static_cast<uint8_t>(std::strtoul(pair, &end, 16));
		if (*end != '\0')
		{
			return false;
		}
	}
	return true;
}

// Writes code at a function's entry that returns at once: "mov eax, value; [fld float]; ret n"
bool WriteStub(uint32_t address, uint32_t eax, uint32_t pop, bool withFloat, uint32_t floatBits)
{
	if (!InImage(address, 16))
	{
		return false;
	}
	uint8_t* code = reinterpret_cast<uint8_t*>(address);
	size_t at = 0;
	code[at++] = 0xB8;
	std::memcpy(code + at, &eax, 4);
	at += 4;
	if (withFloat)
	{
		auto* constant = reinterpret_cast<uint32_t*>(g_arena + g_arenaUsed);
		g_arenaUsed += 16;
		*constant = floatBits;
		code[at++] = 0xD9; // fld dword ptr [abs]
		code[at++] = 0x05;
		const auto where = reinterpret_cast<uint32_t>(constant);
		std::memcpy(code + at, &where, 4);
		at += 4;
	}
	if (pop == 0)
	{
		code[at++] = 0xC3;
	}
	else
	{
		code[at++] = 0xC2;
		const auto bytes = static_cast<uint16_t>(pop);
		std::memcpy(code + at, &bytes, 2);
	}
	return true;
}

// A cdecl allocator that the game code can call in place of its own: it gives zeroed memory from the arena
extern "C" void* __cdecl ArenaAllocate(uint32_t size)
{
	const size_t aligned = (static_cast<size_t>(size) + 15) & ~static_cast<size_t>(15);
	if (g_arenaUsed + aligned > k_ArenaSize)
	{
		return nullptr;
	}
	void* result = g_arena + g_arenaUsed;
	g_arenaUsed += aligned;
	std::memset(result, 0, aligned);
	return result;
}

extern "C" void __cdecl ArenaFree(void*) {}

bool WriteJump(uint32_t address, const void* target)
{
	if (!InImage(address, 5))
	{
		return false;
	}
	uint8_t* code = reinterpret_cast<uint8_t*>(address);
	code[0] = 0xE9;
	const auto rel = reinterpret_cast<uint32_t>(target) - (address + 5);
	std::memcpy(code + 1, &rel, 4);
	return true;
}

// Calls the target with the registers and the stack arguments set, then keeps EAX, EDX and the FPU result. The stack
// pointer is put back from a saved copy, so the calling convention only changes who removes the arguments.
__declspec(noinline) void CallTarget()
{
	__asm {
		pushad
		mov g_savedEsp, esp
		fninit
		fldcw g_controlWord
		cld
		mov ecx, g_argCount
	push_loop:
		test ecx, ecx
		jz pushed
		dec ecx
		push dword ptr [g_args + ecx * 4]
		jmp push_loop
	pushed:
		mov eax, g_inEax
		mov ecx, g_inEcx
		mov edx, g_inEdx
		call dword ptr [g_target]
		mov g_outEax, eax
		mov g_outEdx, edx
		mov eax, esp
		sub eax, g_savedEsp
		mov g_outEspDelta, eax
		mov esp, g_savedEsp
		fnstsw ax
		mov g_outStatus, ax
		mov g_outDepth, 0
		mov g_outSt0Empty, 1
		fxam
		fnstsw ax
		and ax, 0x4500
		cmp ax, 0x4100
		je fpu_done
		mov g_outSt0Empty, 0
		fst dword ptr [g_outSt0Float]
		fld st(0)
		fstp tbyte ptr [g_outSt0]
	count_loop:
		fxam
		fnstsw ax
		and ax, 0x4500
		cmp ax, 0x4100
		je fpu_done
		inc g_outDepth
		fstp st(0)
		mov eax, g_outDepth
		cmp eax, 8
		jb count_loop
	fpu_done:
		fninit
		popad
	}
}

void ReportCall()
{
	char st0[21] = {};
	for (int i = 0; i < 10; ++i)
	{
		std::snprintf(st0 + i * 2, 3, "%02x", g_outSt0[9 - i]);
	}
	Reply("ok eax=%08x edx=%08x esp=%x fsw=%04x depth=%u st0=%s st0f=%08x", g_outEax, g_outEdx, g_outEspDelta, g_outStatus,
	      g_outDepth, g_outSt0Empty != 0 ? "-" : st0, g_outSt0Empty != 0 ? 0u : g_outSt0Float);
}

int CallFilter(unsigned code, EXCEPTION_POINTERS* info, uint32_t& where, uint32_t& accessAddress)
{
	where = reinterpret_cast<uint32_t>(info->ExceptionRecord->ExceptionAddress);
	accessAddress = info->ExceptionRecord->NumberParameters >= 2
	                    ? static_cast<uint32_t>(info->ExceptionRecord->ExceptionInformation[1])
	                    : 0;
	(void)code;
	return EXCEPTION_EXECUTE_HANDLER;
}

void RunCall()
{
	uint32_t code = 0;
	uint32_t where = 0;
	uint32_t accessAddress = 0;
	g_lastImport = 0xFFFFFFFF;
	bool failed = false;
	__try
	{
		CallTarget();
	}
	__except (CallFilter(code = GetExceptionCode(), GetExceptionInformation(), where, accessAddress))
	{
		failed = true;
	}
	if (!failed)
	{
		ReportCall();
		return;
	}
	unsigned short fpuReset = 0x037F;
	__asm {
		fninit
		fldcw fpuReset
	}
	if (code == k_ImportTrapCode && g_lastImport < g_imports.size())
	{
		Reply("err import %s!%s", g_imports[g_lastImport].dll.c_str(), g_imports[g_lastImport].name.c_str());
		return;
	}
	Reply("err exception code=%08x at=%08x address=%08x", code, where, accessAddress);
}

const Import* FindImport(const char* text)
{
	for (const auto& entry : g_imports)
	{
		const std::string full = entry.dll + "!" + entry.name;
		if (_stricmp(full.c_str(), text) == 0)
		{
			return &entry;
		}
	}
	return nullptr;
}

std::vector<char*> Split(char* line)
{
	std::vector<char*> words;
	for (char* word = std::strtok(line, " \t\r\n"); word != nullptr; word = std::strtok(nullptr, " \t\r\n"))
	{
		words.push_back(word);
	}
	return words;
}

// Reads "key=value" words after the command
bool KeyValue(const char* word, const char* key, uint32_t& value)
{
	const size_t length = std::strlen(key);
	return std::strncmp(word, key, length) == 0 && word[length] == '=' && ParseHex(word + length + 1, value);
}

void Command(std::vector<char*>& words)
{
	const std::string command = words[0];
	if (command == "reset")
	{
		g_arenaUsed = 0;
		std::memset(g_arena, 0, k_ArenaSize);
		if (!CopySections())
		{
			Reply("err cannot copy the sections");
			return;
		}
		TrapImports();
		Reply("ok");
	}
	else if (command == "info")
	{
		Reply("ok base=%08x size=%08x arena=%08x arenasize=%08x imports=%u", g_image.base, g_image.size,
		      reinterpret_cast<uint32_t>(g_arena), static_cast<uint32_t>(k_ArenaSize), static_cast<uint32_t>(g_imports.size()));
	}
	else if (command == "alloc" && words.size() == 2)
	{
		uint32_t size = 0;
		if (!ParseHex(words[1], size) || g_arenaUsed + size + 16 > k_ArenaSize)
		{
			Reply("err bad size");
			return;
		}
		Reply("ok %08x", reinterpret_cast<uint32_t>(ArenaAllocate(size)));
	}
	else if (command == "write" && words.size() == 3)
	{
		uint32_t address = 0;
		std::vector<uint8_t> bytes;
		if (!ParseHex(words[1], address) || !ParseHexBytes(words[2], bytes))
		{
			Reply("err bad arguments");
			return;
		}
		if (!Writable(address, static_cast<uint32_t>(bytes.size())))
		{
			Reply("err the range is outside the image and the arena");
			return;
		}
		std::memcpy(reinterpret_cast<void*>(address), bytes.data(), bytes.size());
		Reply("ok");
	}
	else if (command == "read" && words.size() == 3)
	{
		uint32_t address = 0;
		uint32_t length = 0;
		if (!ParseHex(words[1], address) || !ParseHex(words[2], length) || !Writable(address, length))
		{
			Reply("err bad range");
			return;
		}
		std::string hex(length * 2, '0');
		const auto* bytes = reinterpret_cast<const uint8_t*>(address);
		for (uint32_t i = 0; i < length; ++i)
		{
			std::snprintf(&hex[i * 2], 3, "%02x", bytes[i]);
		}
		Reply("ok %s", hex.c_str());
	}
	else if (command == "stub" && words.size() >= 2)
	{
		// stub <address> [eax=v] [pop=n] [st0=floatbits]
		uint32_t address = 0;
		uint32_t eax = 0;
		uint32_t pop = 0;
		uint32_t floatBits = 0;
		bool withFloat = false;
		if (!ParseHex(words[1], address))
		{
			Reply("err bad address");
			return;
		}
		for (size_t i = 2; i < words.size(); ++i)
		{
			if (KeyValue(words[i], "st0", floatBits))
			{
				withFloat = true;
			}
			else if (!KeyValue(words[i], "eax", eax) && !KeyValue(words[i], "pop", pop))
			{
				Reply("err bad argument %s", words[i]);
				return;
			}
		}
		Reply(WriteStub(address, eax, pop, withFloat, floatBits) ? "ok" : "err the address is outside the image");
	}
	else if (command == "hook" && words.size() == 3)
	{
		// hook <address> alloc|free: sends a cdecl allocator of the game to the host's arena
		uint32_t address = 0;
		const std::string kind = words[2];
		if (!ParseHex(words[1], address) || (kind != "alloc" && kind != "free"))
		{
			Reply("err bad arguments");
			return;
		}
		const void* target =
		    kind == "alloc" ? reinterpret_cast<const void*>(&ArenaAllocate) : reinterpret_cast<const void*>(&ArenaFree);
		Reply(WriteJump(address, target) ? "ok" : "err the address is outside the image");
	}
	else if (command == "import" && words.size() >= 3)
	{
		// import <DLL!name> stub [eax=v] [pop=n] [st0=floatbits]: a stub in place of the import's trap
		const Import* entry = FindImport(words[1]);
		if (entry == nullptr || std::strcmp(words[2], "stub") != 0)
		{
			Reply("err no such import, or not \"stub\"");
			return;
		}
		uint32_t eax = 0;
		uint32_t pop = 0;
		uint32_t floatBits = 0;
		bool withFloat = false;
		for (size_t i = 3; i < words.size(); ++i)
		{
			if (KeyValue(words[i], "st0", floatBits))
			{
				withFloat = true;
			}
			else if (!KeyValue(words[i], "eax", eax) && !KeyValue(words[i], "pop", pop))
			{
				Reply("err bad argument %s", words[i]);
				return;
			}
		}
		// The stub lives in the arena; the import's entry points at it
		auto* code = static_cast<uint8_t*>(ArenaAllocate(32));
		size_t at = 0;
		code[at++] = 0xB8;
		std::memcpy(code + at, &eax, 4);
		at += 4;
		if (withFloat)
		{
			auto* constant = reinterpret_cast<uint32_t*>(code + 24);
			*constant = floatBits;
			code[at++] = 0xD9;
			code[at++] = 0x05;
			const auto where = reinterpret_cast<uint32_t>(constant);
			std::memcpy(code + at, &where, 4);
			at += 4;
		}
		if (pop == 0)
		{
			code[at++] = 0xC3;
		}
		else
		{
			code[at++] = 0xC2;
			const auto bytes = static_cast<uint16_t>(pop);
			std::memcpy(code + at, &bytes, 2);
		}
		const auto target = reinterpret_cast<uint32_t>(code);
		std::memcpy(reinterpret_cast<void*>(entry->slot), &target, 4);
		Reply("ok");
	}
	else if (command == "call" && words.size() >= 2)
	{
		// call <address> [cw=v] [eax=v] [ecx=v] [edx=v] [args...]: every argument is one stack dword, first to last
		uint32_t address = 0;
		if (!ParseHex(words[1], address) || !InImage(address, 1))
		{
			Reply("err the address is outside the image");
			return;
		}
		g_target = address;
		g_inEax = 0;
		g_inEcx = 0;
		g_inEdx = 0;
		g_argCount = 0;
		g_controlWord = 0x007F;
		for (size_t i = 2; i < words.size(); ++i)
		{
			uint32_t value = 0;
			uint32_t controlWord = 0;
			if (KeyValue(words[i], "cw", controlWord))
			{
				g_controlWord = static_cast<uint16_t>(controlWord);
			}
			else if (KeyValue(words[i], "eax", g_inEax) || KeyValue(words[i], "ecx", g_inEcx) ||
			         KeyValue(words[i], "edx", g_inEdx))
			{
			}
			else if (ParseHex(words[i], value) && g_argCount < k_MaxArgs)
			{
				g_args[g_argCount++] = value;
			}
			else
			{
				Reply("err bad argument %s", words[i]);
				return;
			}
		}
		RunCall();
	}
	else if (command == "imports")
	{
		std::string all;
		for (const auto& entry : g_imports)
		{
			all += entry.dll + "!" + entry.name + " ";
		}
		Reply("ok %s", all.c_str());
	}
	else
	{
		Reply("err unknown command %s", words[0]);
	}
}

} // namespace

// The loader calls this with the path of the game executable, and ends the process with the result
extern "C" __declspec(dllexport) int __cdecl OracleRun(const char* exePath)
{
	std::string error;
	const std::string path = exePath;
	if (!LoadImage(path.c_str(), error))
	{
		Reply("err %s", error.c_str());
		return 1;
	}
	g_arena = static_cast<uint8_t*>(VirtualAlloc(nullptr, k_ArenaSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
	if (g_arena == nullptr)
	{
		Reply("err cannot allocate the arena");
		return 1;
	}
	Reply("ok ready base=%08x size=%08x", g_image.base, g_image.size);
	static char line[1 << 20];
	while (std::fgets(line, sizeof(line), stdin) != nullptr)
	{
		auto words = Split(line);
		if (words.empty())
		{
			continue;
		}
		if (std::strcmp(words[0], "quit") == 0)
		{
			break;
		}
		Command(words);
	}
	std::fflush(stdout);
	return 0;
}
