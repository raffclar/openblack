/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string_view>

/// The lists of script and file names the stopping natives take, such as "ScriptA, ScriptB"
namespace openblack::script::name_lists
{

/// Whether a list holds a name. Its names are split at spaces, commas and tabs; script names match as they are
/// written, file names whatever their case.
[[nodiscard]] bool HoldsScript(std::string_view list, std::string_view name);
[[nodiscard]] bool HoldsFile(std::string_view list, std::string_view name);

} // namespace openblack::script::name_lists
