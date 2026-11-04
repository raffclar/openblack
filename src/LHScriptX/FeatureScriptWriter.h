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

#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <glm/vec2.hpp>

/// Writes the lines of a land's script as the game writes them when it saves a map: each command with its arguments in
/// the form that the command's list of argument kinds gives
namespace openblack::lhscriptx::writer
{

/// A place on the map as a line gives it: the text of its x and z in metres, in quotes
struct Place
{
	std::string text;
};
/// A name, which the line puts in quotes
struct Name
{
	std::string text;
};
/// One argument of a command, in the order of the command's list of kinds: a place ('A'), a number with a fraction
/// ('F', a float or a double), a name ('L') or a whole number ('N')
using Argument = std::variant<Place, float, Name, int32_t, double>;

/// The text of a place on the map, given in map units: x and z each times 10, rounded to a float, divided by 65536 and
/// written with two decimals, separated by a comma and in quotes
[[nodiscard]] std::string PlaceText(glm::ivec2 place);

/// One line of the script: the command's name, then its arguments in brackets, separated by a comma and a space, and a
/// line end. A kind that is not one of the four letters writes nothing for its argument. Each argument must be of its
/// kind, and the count of arguments must be the count of kinds; else the line is empty.
[[nodiscard]] std::string CommandLine(std::string_view name, std::string_view kinds, std::span<const Argument> arguments);

/// The first line of a map that the game saves: its version
[[nodiscard]] std::string VersionLine(float version);

/// The version that the game writes at the start of a map
constexpr float k_Version = 2.3f;

} // namespace openblack::lhscriptx::writer
