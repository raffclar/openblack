/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "RowDump.h"

#include <cstdio>

#include <algorithm>
#include <array>
#include <span>
#include <string>

#include <entt/entity/entity.hpp>

namespace openblack::row_dump
{

TaggedLines::int_type TaggedLines::overflow(int_type character)
{
	if (traits_type::eq_int_type(character, traits_type::eof()))
	{
		return traits_type::not_eof(character);
	}
	const auto letter = traits_type::to_char_type(character);
	if (letter != '\n')
	{
		_line.push_back(letter);
		return character;
	}
	const auto line = _tag + _line + '\n';
	std::fputs(line.c_str(), stdout);
	_line.clear();
	return character;
}

int TaggedLines::sync()
{
	return std::fflush(stdout);
}

void AddValue(ecs::reflection::WorldRows& rows, std::string_view name, std::span<const uint8_t> bytes)
{
	rows.rows.push_back({.entity = entt::null,
	                     .field = static_cast<uint32_t>(rows.fields.size()),
	                     .offset = static_cast<uint32_t>(rows.bytes.size()),
	                     .size = static_cast<uint32_t>(bytes.size())});
	rows.fields.emplace_back(name);
	rows.bytes.insert(rows.bytes.end(), bytes.begin(), bytes.end());
}

RowDump::RowDump(std::ostream& out, Settings settings)
    : _out(out)
    , _settings(settings)
{
}

void RowDump::Keep(uint32_t turn, ecs::reflection::WorldRows rows)
{
	if (_settings.atTurn == turn)
	{
		Write(turn, rows, "turn");
	}
	if (_settings.keptTurns == 0)
	{
		return;
	}
	while (_kept.size() >= _settings.keptTurns)
	{
		_kept.pop_front();
	}
	_kept.emplace_back(turn, std::move(rows));
}

bool RowDump::Part(uint32_t turn, std::string_view reason, uint32_t source)
{
	// A world that stays parted differs at each turn after the first: only the first turn tells the cause
	const auto last = _lastPart.find(source);
	const bool continues = last != _lastPart.end() && last->second + 1 == turn;
	_lastPart[source] = turn;
	if (continues)
	{
		return false;
	}
	const auto kept = std::ranges::find(_kept, turn, &decltype(_kept)::value_type::first);
	if (kept == _kept.end())
	{
		_out << "turn " << turn << " " << reason << " (the rows of the turn are not kept)\n";
		_out.flush();
		return false;
	}
	return Write(turn, kept->second, reason);
}

bool RowDump::Write(uint32_t turn, const ecs::reflection::WorldRows& rows, std::string_view reason)
{
	if (_written.size() >= _settings.mostWrites || std::ranges::find(_written, turn) != _written.end())
	{
		return false;
	}
	_written.push_back(turn);
	constexpr std::array k_Digits {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
	_out << "turn " << turn << " " << reason << "\n";
	std::string line;
	for (const auto& row : rows.rows)
	{
		line.clear();
		line += rows.fields.at(row.field);
		line += ' ';
		line += std::to_string(entt::to_integral(row.entity));
		line += ' ';
		for (const auto byte : std::span(rows.bytes).subspan(row.offset, row.size))
		{
			line += k_Digits.at(byte >> 4U);
			line += k_Digits.at(byte & 0xfU);
		}
		if (row.presentation)
		{
			line += " shown";
		}
		line += '\n';
		_out << line;
	}
	_out.flush();
	return true;
}

} // namespace openblack::row_dump
