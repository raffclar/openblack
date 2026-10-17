/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cctype>
#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <bit>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

// Pinned values from the oracle (tools/oracle): the original game's own functions, run on chosen inputs. A data file
// has comment lines that start with '#', one "columns" line with the names, then one row for each case. Each value in
// a row is a 32-bit word in hexadecimal (a float as its bits). The tests do not need the game: they read these files.
namespace openblack::oracle
{

// The data folder is found when the test runs, never when it is compiled: a compiled object can be reused from another
// checkout (the shared compiler cache), and a path in it would then point at the wrong folder.
#ifdef OPENBLACK_ORACLE_DATA
#error Do not define OPENBLACK_ORACLE_DATA: the data folder is found at run time (see DataFolder)
#endif

/// The folder of the pinned data files. It is the value of the environment variable OPENBLACK_ORACLE_DATA, which ctest
/// sets for each test. When the variable is not set, it is the first line of the file oracle_data_folder.txt in the
/// working directory (the build folder of the tests has one). It gives no value when neither exists.
[[nodiscard]] inline std::optional<std::filesystem::path> DataFolder()
{
	// NOLINTNEXTLINE(concurrency-mt-unsafe): read only, and no other thread changes the environment
	const char* const folder = std::getenv("OPENBLACK_ORACLE_DATA");
	if (folder != nullptr && std::string_view(folder).size() > 0)
	{
		return std::filesystem::path(folder);
	}
	std::ifstream stream("oracle_data_folder.txt");
	std::string line;
	if (stream && std::getline(stream, line))
	{
		while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back())) != 0)
		{
			line.pop_back();
		}
		if (!line.empty())
		{
			return std::filesystem::path(line);
		}
	}
	return std::nullopt;
}

class Table
{
public:
	/// Reads a data file from the pinned data folder. It gives no value when the file is missing or not valid.
	[[nodiscard]] static std::optional<Table> Load(std::string_view name)
	{
		const auto folder = DataFolder();
		if (!folder)
		{
			return std::nullopt;
		}
		std::ifstream stream(*folder / name);
		if (!stream)
		{
			return std::nullopt;
		}
		Table table;
		std::string line;
		while (std::getline(stream, line))
		{
			if (line.empty() || line.front() == '#')
			{
				continue;
			}
			std::istringstream words(line);
			if (table._columns.empty())
			{
				std::string word;
				words >> word;
				if (word != "columns")
				{
					return std::nullopt;
				}
				while (words >> word)
				{
					table._columns.push_back(word);
				}
				continue;
			}
			std::vector<uint32_t> row;
			std::string word;
			while (words >> word)
			{
				row.push_back(static_cast<uint32_t>(std::strtoul(word.c_str(), nullptr, 16)));
			}
			if (row.size() != table._columns.size())
			{
				return std::nullopt;
			}
			table._rows.push_back(std::move(row));
		}
		return table;
	}

	[[nodiscard]] size_t Rows() const { return _rows.size(); }

	/// The index of a column, or no value when the table does not have it
	[[nodiscard]] std::optional<size_t> Column(std::string_view name) const
	{
		const auto found = std::ranges::find(_columns, name);
		if (found == _columns.end())
		{
			return std::nullopt;
		}
		return static_cast<size_t>(found - _columns.begin());
	}

	[[nodiscard]] uint32_t Bits(size_t row, size_t column) const { return _rows.at(row).at(column); }
	[[nodiscard]] float Float(size_t row, size_t column) const { return std::bit_cast<float>(Bits(row, column)); }

private:
	std::vector<std::string> _columns;
	std::vector<std::vector<uint32_t>> _rows;
};

/// A float's bits as text, for test messages
[[nodiscard]] inline std::string Hex(float value)
{
	std::ostringstream text;
	text << std::hex << std::bit_cast<uint32_t>(value);
	return text.str();
}

} // namespace openblack::oracle
