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

#include <algorithm>
#include <deque>
#include <span>
#include <utility>
#include <variant>
#include <vector>

#include "Commands/TurnCodec.h"

namespace openblack::online
{

/// The blocks of commands that a session of a client sent to the game server. Each block gets the next number of the
/// session. The client keeps a block until a turn of the server holds it, and sends the kept blocks again when it comes
/// back. Thus a restarted server gets the commands that came after its last sent turn. The server leaves out a block
/// with a number that it had (deviation 100).
class SentBlocks
{
public:
	/// The number that the next block gets
	[[nodiscard]] uint16_t NextSequence() const { return _next; }

	/// The block of the commands with the next number. The number moves on only when the block went (Sent).
	[[nodiscard]] turn_codec::CommandBlock Make(uint32_t turn, std::span<const player_commands::Action> actions) const
	{
		return turn_codec::CommandBlock {.sequence = _next, .turn = turn, .actions = {actions.begin(), actions.end()}};
	}

	/// The block went to the server. The client keeps it until a turn holds it, when keep is true. Before the start, the
	/// server refuses the blocks, so the client does not keep them.
	void Sent(turn_codec::CommandBlock block, bool keep)
	{
		_next = static_cast<uint16_t>(block.sequence + 1);
		if (keep)
		{
			_kept.push_back(std::move(block));
		}
	}

	/// A turn of the server holds the block with this number. The server reads the blocks of a client in their order, so
	/// the blocks before it are in a turn too, or the server left them out.
	void Held(uint16_t sequence)
	{
		std::erase_if(_kept, [sequence](const turn_codec::CommandBlock& block) {
			return !turn_codec::SequenceAfter(block.sequence, sequence);
		});
	}

	/// The blocks of the god of this client that a turn holds. The server makes the blocks of a join or a leave itself, so
	/// they do not count.
	void HeldIn(const turn_codec::TurnBundle& turn, PlayerNames god)
	{
		for (const auto& block : turn.blocks)
		{
			if (block.sender == god && !MadeByServer(block))
			{
				Held(block.sequence);
			}
		}
	}

	/// The blocks that went to the server and that no turn holds yet, oldest first
	[[nodiscard]] const std::deque<turn_codec::CommandBlock>& Kept() const { return _kept; }

	/// A new session counts its blocks from 0
	void Reset()
	{
		_next = 0;
		_kept.clear();
	}

	/// True when the server made the block: the join or the leave of a god
	[[nodiscard]] static bool MadeByServer(const turn_codec::StampedBlock& block)
	{
		return std::ranges::any_of(block.actions, [](const player_commands::Action& action) {
			return std::holds_alternative<player_commands::PlayerJoined>(action) ||
			       std::holds_alternative<player_commands::PlayerLeft>(action);
		});
	}

private:
	uint16_t _next {0};
	std::deque<turn_codec::CommandBlock> _kept;
};

} // namespace openblack::online
