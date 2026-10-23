/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CommandLog.h"

#include <variant>

#include "Commands/TurnCodec.h"

namespace openblack::command_log
{

namespace
{
constexpr std::string_view k_Digits = "0123456789abcdef";
constexpr std::string_view k_TurnKey = "\"turn\":";
constexpr std::string_view k_FrameKey = "\"frame\":\"";

std::optional<uint8_t> DigitOf(char c)
{
	const auto at = k_Digits.find(c);
	if (at == std::string_view::npos)
	{
		return std::nullopt;
	}
	return static_cast<uint8_t>(at);
}
} // namespace

std::string LineOf(uint32_t turn, std::span<const player_commands::Command> commands)
{
	turn_codec::TurnBundle bundle {.turn = turn, .blocks = {}};
	for (const auto& command : commands)
	{
		if (bundle.blocks.empty() || bundle.blocks.back().sender != command.player ||
		    bundle.blocks.back().actions.size() >= turn_codec::k_MostBlockCommands)
		{
			bundle.blocks.push_back(
			    {.sender = command.player, .sequence = static_cast<uint16_t>(bundle.blocks.size()), .actions = {}});
		}
		bundle.blocks.back().actions.push_back(command.action);
	}
	const auto bytes = turn_codec::Encode(bundle);
	std::string line = "{" + std::string(k_TurnKey) + std::to_string(turn) + "," + std::string(k_FrameKey);
	line.reserve(line.size() + bytes.size() * 2 + 2);
	for (const auto byte : bytes)
	{
		line.push_back(k_Digits[byte >> 4]);
		line.push_back(k_Digits[byte & 0xF]);
	}
	line += "\"}";
	return line;
}

std::optional<Turn> ReadLine(std::string_view line)
{
	const auto frameAt = line.find(k_FrameKey);
	if (frameAt == std::string_view::npos)
	{
		return std::nullopt;
	}
	const auto hexStart = frameAt + k_FrameKey.size();
	const auto hexEnd = line.find('"', hexStart);
	if (hexEnd == std::string_view::npos || (hexEnd - hexStart) % 2 != 0)
	{
		return std::nullopt;
	}
	std::vector<uint8_t> bytes;
	bytes.reserve((hexEnd - hexStart) / 2);
	for (auto at = hexStart; at < hexEnd; at += 2)
	{
		const auto high = DigitOf(line[at]);
		const auto low = DigitOf(line[at + 1]);
		if (!high.has_value() || !low.has_value())
		{
			return std::nullopt;
		}
		bytes.push_back(static_cast<uint8_t>((*high << 4) | *low));
	}
	const auto decoded = turn_codec::Decode(bytes);
	if (decoded.error != turn_codec::DecodeError::None || !decoded.frame.has_value())
	{
		return std::nullopt;
	}
	const auto* bundle = std::get_if<turn_codec::TurnBundle>(&*decoded.frame);
	if (bundle == nullptr)
	{
		return std::nullopt;
	}
	Turn turn {.turn = bundle->turn, .commands = {}};
	for (const auto& block : bundle->blocks)
	{
		for (const auto& action : block.actions)
		{
			turn.commands.push_back({.player = block.sender, .action = action});
		}
	}
	return turn;
}

Writer::Writer(const std::filesystem::path& path)
    : _file(path, std::ios::out | std::ios::trunc)
{
}

void Writer::Write(uint32_t turn, std::span<const player_commands::Command> commands)
{
	if (commands.empty() || !_file.is_open())
	{
		return;
	}
	_file << LineOf(turn, commands) << '\n';
	_file.flush();
}

std::optional<std::map<uint32_t, std::vector<player_commands::Command>>> ReadFile(const std::filesystem::path& path)
{
	std::ifstream file(path);
	if (!file.is_open())
	{
		return std::nullopt;
	}
	std::map<uint32_t, std::vector<player_commands::Command>> turns;
	std::string line;
	while (std::getline(file, line))
	{
		if (line.find_first_not_of(" \t\r") == std::string::npos)
		{
			continue;
		}
		auto turn = ReadLine(line);
		if (!turn.has_value())
		{
			return std::nullopt;
		}
		auto& commands = turns[turn->turn];
		commands.insert(commands.end(), turn->commands.begin(), turn->commands.end());
	}
	return turns;
}

} // namespace openblack::command_log
