/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "Lexer.h"

namespace openblack::lhscriptx
{

/// One command of a map script, as the script gives it
struct Command
{
	std::string name;
	std::vector<Token> arguments;
	/// The line of the text that the command starts on. The first line is 1.
	int line {0};
};

/// The commands of a map script, read with none of them done
struct ReadCommands
{
	std::vector<Command> commands;
	/// Why the read stopped before the end of the text, if it stopped. The game stops the script at this point.
	std::optional<std::string> error;
};

class Script
{
public:
	Script();
	/// A script whose places are all moved by an offset across the land (x, z) and raised by its height (y), as the
	/// lines of the crossing file are read about the place the vortex throws things out. The game adds `originOnMap` to
	/// the map position of each place.
	Script(glm::vec3 origin, const map_coords::MapCoords& originOnMap);

	/// Carries out the commands of a map script, calling `afterCommand`, when given, after each one
	void Load(const std::string& source, const std::function<void()>& afterCommand = {});

	/// Reads the commands of a map script and does none of them. The read stops where `Load` stops.
	[[nodiscard]] static ReadCommands Read(const std::string& source);

	/// Does one command of a map script, as `Load` does it. A command that has the wrong arguments throws.
	void Run(const Command& command);

private:
	/// Reads the commands of the text in their order and gives each to `onCommand`. A line that the script cannot read
	/// throws.
	static void ForEachCommand(const std::string& source, const std::function<void(Command&&)>& onCommand);
	[[nodiscard]] static bool IsCommand(const std::string& identifier);
	void RunCommand(const std::string& identifier, const std::vector<Token>& args);

	/// What every place the script gives is moved by
	glm::vec3 _origin {0.0f};
	/// What the map position of every place the script gives is moved by
	map_coords::MapCoords _originOnMap {};
};

} // namespace openblack::lhscriptx
