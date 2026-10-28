/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Commands/PlayerCommands.h"

/// Tools: the file of the commands that each turn did. A game writes it, and a dedicated server reads it and does the
/// same commands at the same turns, as if the game sent them over the network. Each line is one turn that did
/// commands, as JSON: {"turn": N, "frame": "..."}. The frame is the bytes of the turn as the server sends it to its
/// clients (turn_codec), in hexadecimal.
namespace openblack::command_log
{

/// The line of a turn and its commands. The commands of a player that come one after the other go in one block.
[[nodiscard]] std::string LineOf(uint32_t turn, std::span<const player_commands::Command> commands);

/// The turn and the commands of a line. None when the line does not read.
struct Turn
{
	uint32_t turn {0};
	std::vector<player_commands::Command> commands;
};
[[nodiscard]] std::optional<Turn> ReadLine(std::string_view line);

/// Writes the commands of each turn to a file, one line a turn that did commands
class Writer
{
public:
	explicit Writer(const std::filesystem::path& path);
	[[nodiscard]] bool IsOpen() const { return _file.is_open(); }
	void Write(uint32_t turn, std::span<const player_commands::Command> commands);

private:
	std::ofstream _file;
};

/// The commands of each turn in a file, by the turn. None when the file does not open or a line does not read.
[[nodiscard]] std::optional<std::map<uint32_t, std::vector<player_commands::Command>>>
ReadFile(const std::filesystem::path& path);

/// Tools: the commands of a testbed scenario that the game gave before each turn. Some commands of a scenario wait for
/// what only the computer of the game knows, such as the end of a gesture that its frames draw. A server that plays the
/// commands file of the game gives the commands of its own scenario before the same turns as the game did. The file is
/// beside the commands file, and each line is one command given, as JSON: {"turn": N, "give": I}. I is the place of
/// the command in the scenario.
[[nodiscard]] std::filesystem::path GivesPathOf(const std::filesystem::path& commands);

/// The line of a command of a scenario that the game gave before a turn
[[nodiscard]] std::string GiveLineOf(uint32_t turn, size_t command);

/// A command of a scenario that the game gave before a turn
struct Give
{
	uint32_t turn {0};
	size_t command {0};

	bool operator==(const Give&) const = default;
};
/// The turn and the command of a line of the file of the given commands. None when the line does not read.
[[nodiscard]] std::optional<Give> ReadGiveLine(std::string_view line);

/// Writes the commands of a scenario that the game gives, one line a command
class GivesWriter
{
public:
	explicit GivesWriter(const std::filesystem::path& path);
	void Write(uint32_t turn, size_t command);

private:
	std::ofstream _file;
};

/// The commands of a scenario that the game gave before each turn, in order, by the turn. Empty when the file does not
/// open. A line that does not read is left out.
[[nodiscard]] std::map<uint32_t, std::vector<size_t>> ReadGivesFile(const std::filesystem::path& path);

} // namespace openblack::command_log
