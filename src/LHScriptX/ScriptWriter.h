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

#include <array>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "3D/MapCoords.h"

/// The land-script writer: the inverse of the reader (LHScriptX/Script.cpp), as the original's SaveObject writers make
/// their lines (docs/bw1-notes/land-script-save.md "The line format", docs/bw1-notes/vortex.md).
namespace openblack::lhscriptx
{
/// One entry of the command table (the name, then 12 type letters, spaces as padding):
/// N an int, A a text without quotes (a position or a name), L a quoted string, F a float; any other letter is padding
struct CommandFormat
{
	std::string_view name;
	std::string_view types;
};
/// The command table, in the original's order (SCRIPT_FEATURE_COMMANDS 0..104)
extern const std::array<CommandFormat, 105> k_CommandFormats;

/// The entry of a command by name, or nullptr
[[nodiscard]] const CommandFormat* FindCommandFormat(std::string_view name);

/// The printf format of a command: the name, "(", per type letter A "%s, ", L "\"%s\", ", N "%d, ", F "%f, ", the
/// last ", " cut when anything was added, then ")\n"
[[nodiscard]] std::string CommandAsText(const CommandFormat& command);

/// sprintf "\"%0.2f,%0.2f\"" (the quotes in the literal) of x then z, each (double)(fixed x 10.0f x 2^-16) (with the
/// game's float precision the first product is rounded to a float, the second is exact); the digits rounded half up
/// as the original's C runtime
[[nodiscard]] std::string PositionText(const map_coords::MapCoords& coords);

/// One value of a line: an N is an int32_t, an F a float, an L a std::string, an A a std::string (already the text,
/// e.g. a villager type's name) or a MapCoords (written with PositionText)
using CommandValue = std::variant<int32_t, float, std::string, map_coords::MapCoords>;

/// One land-script line, the writer's sprintf(CommandAsText(cmd), values...): nullopt for an unknown command, a wrong
/// number of values or a value of the wrong type for its letter. %d as sprintf; %f / %0.2f with the original's half-up
/// rounding of the digits, not the UCRT's half to even
[[nodiscard]] std::optional<std::string> WriteCommand(std::string_view name, std::initializer_list<CommandValue> values);
} // namespace openblack::lhscriptx
