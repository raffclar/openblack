/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ChecksumLog.h"

#include <charconv>
#include <cstdio>

#include <fmt/format.h>

namespace openblack::checksum_log
{

namespace
{
constexpr std::string_view k_TurnKey = "\"turn\":";
constexpr std::string_view k_ChecksumKey = "\"checksum\":\"";
constexpr size_t k_ChecksumDigits = 16;
} // namespace

std::string LineOf(uint32_t turn, uint64_t checksum, const std::vector<std::pair<std::string, uint64_t>>& parts)
{
	auto line = fmt::format("{{{}{},{}{:016x}\"", k_TurnKey, turn, k_ChecksumKey, checksum);
	if (!parts.empty())
	{
		line += ",\"parts\":{";
		for (size_t i = 0; i < parts.size(); ++i)
		{
			line += fmt::format("{}\"{}\":\"{:016x}\"", i == 0 ? "" : ",", parts[i].first, parts[i].second);
		}
		line += "}";
	}
	line += "}";
	return line;
}

std::optional<Turn> ReadLine(std::string_view line)
{
	const auto turnAt = line.find(k_TurnKey);
	const auto checksumAt = line.find(k_ChecksumKey);
	if (turnAt == std::string_view::npos || checksumAt == std::string_view::npos ||
	    checksumAt + k_ChecksumKey.size() + k_ChecksumDigits > line.size())
	{
		return std::nullopt;
	}
	Turn result {};
	const auto* turnStart = line.data() + turnAt + k_TurnKey.size();
	if (std::from_chars(turnStart, line.data() + line.size(), result.turn).ec != std::errc {})
	{
		return std::nullopt;
	}
	const auto* checksumStart = line.data() + checksumAt + k_ChecksumKey.size();
	const auto [end, error] = std::from_chars(checksumStart, checksumStart + k_ChecksumDigits, result.checksum, 16);
	if (error != std::errc {} || end != checksumStart + k_ChecksumDigits)
	{
		return std::nullopt;
	}
	return result;
}

Writer::Writer(const std::filesystem::path& path)
    : _toOutput(path == std::filesystem::path(k_StandardOutput))
{
	if (!_toOutput)
	{
		_file.open(path, std::ios::trunc);
	}
}

void Writer::Write(uint32_t turn, uint64_t checksum, const std::vector<std::pair<std::string, uint64_t>>& parts)
{
	WriteLine(LineOf(turn, checksum, parts));
}

void Writer::WriteDump(uint32_t turn, const std::vector<std::string>& rows)
{
	for (const auto& row : rows)
	{
		WriteLine(fmt::format("{{\"dump_turn\":{},\"row\":\"{}\"}}", turn, row));
	}
}

void Writer::WriteLine(const std::string& line)
{
	if (_toOutput)
	{
		std::fputs(fmt::format("{}{}\n", k_OutputTag, line).c_str(), stdout);
		std::fflush(stdout);
		return;
	}
	_file << line << '\n';
	_file.flush();
}

} // namespace openblack::checksum_log
