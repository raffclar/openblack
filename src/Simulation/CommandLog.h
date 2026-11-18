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
/// clients (turn_codec), in hexadecimal. After the game loads a land, a line also has "land": L, the count of the lands
/// that the game loaded before this land.
namespace openblack::command_log
{

/// The count of the lands that a game loaded before its present land. Each load of a land starts the turn count again,
/// so a turn that is less than the last turn starts the next land.
class LandCounter
{
public:
	/// Counts a turn of the game and gives the land of the turn
	uint32_t Note(uint32_t turn) noexcept
	{
		if (_lastTurn.has_value() && turn < *_lastTurn)
		{
			++_land;
		}
		_lastTurn = turn;
		return _land;
	}
	[[nodiscard]] uint32_t GetLand() const noexcept { return _land; }

private:
	std::optional<uint32_t> _lastTurn;
	uint32_t _land {0};
};

/// The line of a turn and its commands. The commands of a player that come one after the other go in one block.
[[nodiscard]] std::string LineOf(uint32_t turn, std::span<const player_commands::Command> commands, uint32_t land = 0);

/// The turn and the commands of a line. None when the line does not read.
struct Turn
{
	uint32_t turn {0};
	std::vector<player_commands::Command> commands;
	/// The count of the lands that the game loaded before the land of the turn
	uint32_t land {0};
};
[[nodiscard]] std::optional<Turn> ReadLine(std::string_view line);

/// Writes the commands of each turn to a file, one line a turn that did commands
class Writer
{
public:
	explicit Writer(const std::filesystem::path& path);
	[[nodiscard]] bool IsOpen() const { return _file.is_open(); }
	void Write(uint32_t turn, std::span<const player_commands::Command> commands, uint32_t land = 0);

private:
	std::ofstream _file;
};

/// The commands of each turn in a file, by the turn. None when the file does not open or a line does not read.
[[nodiscard]] std::optional<std::map<uint32_t, std::vector<player_commands::Command>>>
ReadFile(const std::filesystem::path& path);

/// The commands of each turn in a file, by the land and then by the turn. The first land of the game is the first item.
/// None when the file does not open or a line does not read.
[[nodiscard]] std::optional<std::vector<std::map<uint32_t, std::vector<player_commands::Command>>>>
ReadLandsFile(const std::filesystem::path& path);

/// Tools: the commands of a testbed scenario that the game gave before each turn. Some commands of a scenario wait for
/// what only the computer of the game knows, such as the end of a gesture that its frames draw. A server that plays the
/// commands file of the game gives the commands of its own scenario before the same turns as the game did. The file is
/// beside the commands file, and each line is one command given, as JSON: {"turn": N, "give": I}. I is the place of
/// the command in the scenario. After the game loads a land, a line also has "land": L, as in the commands file.
[[nodiscard]] std::filesystem::path GivesPathOf(const std::filesystem::path& commands);

/// The line of a command of a scenario that the game gave before a turn
[[nodiscard]] std::string GiveLineOf(uint32_t turn, size_t command, uint32_t land = 0);

/// A command of a scenario that the game gave before a turn
struct Give
{
	uint32_t turn {0};
	size_t command {0};
	/// The count of the lands that the game loaded before the land of the turn
	uint32_t land {0};

	bool operator==(const Give&) const = default;
};
/// The turn and the command of a line of the file of the given commands. None when the line does not read.
[[nodiscard]] std::optional<Give> ReadGiveLine(std::string_view line);

/// Writes the commands of a scenario that the game gives, one line a command
class GivesWriter
{
public:
	explicit GivesWriter(const std::filesystem::path& path);
	void Write(uint32_t turn, size_t command, uint32_t land = 0);

private:
	std::ofstream _file;
};

/// The commands of a scenario that the game gave, in the order of the file. A load of a land sets the turn back, so a
/// turn can occur more than one time. Empty when the file does not open. A line that does not read is left out.
[[nodiscard]] std::vector<Give> ReadGivesFile(const std::filesystem::path& path);

} // namespace openblack::command_log
