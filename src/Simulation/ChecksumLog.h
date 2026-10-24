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
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/// Tools: the file of the checksum of each turn of a run. Two builds of the game (for example the desktop build and the
/// web build) that play the same land with the same seed and the same commands must write the same lines. Each line is
/// one turn, as JSON: {"turn": N, "checksum": "...", "parts": {"name": "...", ...}}. The parts are there only when the
/// run keeps them.
namespace openblack::checksum_log
{

/// The path that writes the lines to the standard output, each after k_OutputTag. The web build uses it, because the
/// page gets the standard output of the game.
constexpr std::string_view k_StandardOutput = "-";
/// The text before each line on the standard output, by which a tool finds the lines among the other output
constexpr std::string_view k_OutputTag = "openblack-checksum ";

/// The line of a turn
[[nodiscard]] std::string LineOf(uint32_t turn, uint64_t checksum, const std::vector<std::pair<std::string, uint64_t>>& parts);

/// The turn and the checksum of a line, none when the line is not one
struct Turn
{
	uint32_t turn {0};
	uint64_t checksum {0};
};
[[nodiscard]] std::optional<Turn> ReadLine(std::string_view line);

/// Writes the line of each turn to a file, or to the standard output for k_StandardOutput
class Writer
{
public:
	explicit Writer(const std::filesystem::path& path);
	[[nodiscard]] bool IsOpen() const { return _toOutput || _file.is_open(); }
	void Write(uint32_t turn, uint64_t checksum, const std::vector<std::pair<std::string, uint64_t>>& parts);
	/// Writes each line of a dump of a turn, as {"dump_turn": N, "row": "..."}
	void WriteDump(uint32_t turn, const std::vector<std::string>& rows);

private:
	void WriteLine(const std::string& line);

	bool _toOutput {false};
	std::ofstream _file;
};

} // namespace openblack::checksum_log
