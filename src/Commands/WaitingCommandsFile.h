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

#include <optional>
#include <span>
#include <variant>
#include <vector>

#include "Commands/PlayerCommands.h"
#include "Commands/TurnCodec.h"

/// The bytes of the commands of a single player game that wait for a turn, which a save of an older build keeps. The
/// load does not use these commands. The bytes are a turn of the network layout with one block for each command. The
/// number of each block tells the part of the queue of the command.
namespace openblack::waiting_commands_file
{

/// The number of a block of a command that went and waits for the start of the next turn
inline constexpr uint16_t k_SentPart = 0;
/// The number of a block of a command in the open block, which goes after the turns of the frame
inline constexpr uint16_t k_OpenPart = 1;

/// The bytes of the commands that wait. None when no command waits, or when the commands do not fit in a turn.
[[nodiscard]] inline std::vector<uint8_t> Write(const player_commands::WaitingCommands& waiting)
{
	if (waiting.Empty())
	{
		return {};
	}
	turn_codec::TurnBundle turn;
	const auto add = [&turn](const std::vector<player_commands::Command>& commands, uint16_t part) {
		for (const auto& command : commands)
		{
			turn.blocks.push_back(turn_codec::StampedBlock {
			    .sender = command.player,
			    .sequence = part,
			    .actions = {command.action},
			    .observer = command.observer,
			});
		}
	};
	add(waiting.sent, k_SentPart);
	add(waiting.open, k_OpenPart);
	return turn_codec::Encode(turn);
}

/// The commands that wait, from their bytes. None when the bytes are not the commands of a turn.
[[nodiscard]] inline std::optional<player_commands::WaitingCommands> Read(std::span<const uint8_t> bytes)
{
	const auto decoded = turn_codec::Decode(bytes);
	const auto* turn = decoded.frame.has_value() ? std::get_if<turn_codec::TurnBundle>(&*decoded.frame) : nullptr;
	if (decoded.error != turn_codec::DecodeError::None || turn == nullptr)
	{
		return std::nullopt;
	}
	player_commands::WaitingCommands waiting;
	for (const auto& block : turn->blocks)
	{
		if (block.sequence != k_SentPart && block.sequence != k_OpenPart)
		{
			return std::nullopt;
		}
		auto& commands = block.sequence == k_SentPart ? waiting.sent : waiting.open;
		for (const auto& action : block.actions)
		{
			commands.push_back({.player = block.sender, .action = action, .observer = block.observer});
		}
	}
	return waiting;
}

} // namespace openblack::waiting_commands_file
