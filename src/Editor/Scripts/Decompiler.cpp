/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Decompiler.h"

#include <algorithm>
#include <format>

#include <LHVMDecompiler.h>

namespace openblack::editor::scripts
{

// Source comes from the Challenge Language decompiler. No compiler is linked in yet: filling in Compile is what's left,
// and the panel's editor already hands its text to it.

bool HasDecompiler()
{
	return true;
}

std::optional<DecompiledSource> Decompile(const Program& program, const lhvm::VMScript& script)
{
	const auto found = std::ranges::find(program.scripts, script.scriptId, &lhvm::VMScript::scriptId);
	if (found == program.scripts.end())
	{
		return std::nullopt;
	}

	// The machine's variable 0 is its null variable; the decompiler numbers globals from 1
	std::vector<std::string> globals;
	globals.reserve(program.globals.size());
	for (size_t i = 1; i < program.globals.size(); ++i)
	{
		globals.push_back(program.globals[i].name);
	}
	const lhvm::ProgramView view {
	    .instructions = program.code,
	    .scripts = program.scripts,
	    .globalNames = globals,
	    .data = program.data,
	    .autostart = {},
	};
	// ImGui draws no tabs, so indent with spaces
	const auto decompiled =
	    lhvm::DecompileScript(view, static_cast<size_t>(std::distance(program.scripts.begin(), found)), {.indent = "    "});

	DecompiledSource source;
	size_t start = 0;
	while (start < decompiled.text.size())
	{
		const auto end = decompiled.text.find('\n', start);
		source.lines.push_back(decompiled.text.substr(start, end - start));
		start = end == std::string::npos ? decompiled.text.size() : end + 1;
	}
	// A line leads to the first of the instructions it stands for
	for (size_t line = 0; line < source.lines.size(); ++line)
	{
		const auto& addresses = line < decompiled.lines.size() ? decompiled.lines[line] : std::vector<uint32_t> {};
		source.lineAddresses.push_back(addresses.empty() ? std::nullopt : std::optional(addresses.front()));
	}
	for (const auto& diagnostic : decompiled.diagnostics)
	{
		if (diagnostic.severity != lhvm::DiagnosticSeverity::Info)
		{
			source.diagnostics.push_back(std::format("{}: {}", diagnostic.ip, diagnostic.message));
		}
	}
	return source;
}

bool HasCompiler()
{
	return false;
}

CompileResult Compile([[maybe_unused]] const lhvm::VMScript& script, [[maybe_unused]] std::string_view source)
{
	return {.compiled = false, .diagnostics = {"No script compiler is linked in yet"}};
}

} // namespace openblack::editor::scripts
