/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ScriptModel.h"

namespace openblack::editor::scripts
{

/// Whether a decompiler is linked in to turn scripts into source
[[nodiscard]] bool HasDecompiler();
/// A script's code as source, with each line's instruction, from the decompiler; none without one
[[nodiscard]] std::optional<DecompiledSource> Decompile(const Program& program, const lhvm::VMScript& script);

/// What became of compiling edited source
struct CompileResult
{
	bool compiled {false};
	std::vector<std::string> diagnostics;
};
/// Whether a compiler is linked in to turn edited source back into a program
[[nodiscard]] bool HasCompiler();
/// Compiles a script's edited source; says why not without a compiler
[[nodiscard]] CompileResult Compile(const lhvm::VMScript& script, std::string_view source);

} // namespace openblack::editor::scripts
