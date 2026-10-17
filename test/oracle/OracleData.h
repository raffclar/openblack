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

#ifndef OPENBLACK_ORACLE_DATA
#error Define OPENBLACK_ORACLE_DATA as the folder of the pinned data files
#endif

class Table
{
public:
	/// Reads a data file from the pinned data folder. It gives no value when the file is missing or not valid. The
	/// folder is a default argument, so that each test reads the folder it was compiled with: the body of this inline
	/// function is the same in every test, and one copy of it is kept for all of them.
	[[nodiscard]] static std::optional<Table> Load(std::string_view name, std::string_view folder = OPENBLACK_ORACLE_DATA)
	{
		std::ifstream stream(std::filesystem::path(folder) / name);
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
