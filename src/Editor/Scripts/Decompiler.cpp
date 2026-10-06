/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Decompiler.h"

namespace openblack::editor::scripts
{

// No decompiler or compiler is linked in yet. Linking them means filling these in: the panel's source view is already
// laid out round the source's lines and their instructions, and its editor hands its text to Compile.

bool HasDecompiler()
{
	return false;
}

std::optional<DecompiledSource> Decompile([[maybe_unused]] const Program& program,
                                          [[maybe_unused]] const lhvm::VMScript& script)
{
	return std::nullopt;
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
