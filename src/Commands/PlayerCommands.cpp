/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PlayerCommands.h"

#include <numeric>
#include <utility>

using namespace openblack::player_commands;

void CommandQueue::Submit(PlayerNames sender, Action action)
{
	_open.push_back(Command {.player = sender, .action = std::move(action)});
	if (_open.size() >= k_BlockSize)
	{
		Send();
	}
}

void CommandQueue::Send()
{
	if (_open.empty())
	{
		return;
	}
	_sent.push_back(std::exchange(_open, {}));
}

std::vector<Command> CommandQueue::TakeSent()
{
	std::vector<Command> commands;
	commands.reserve(SentCount());
	for (auto& block : _sent)
	{
		commands.insert(commands.end(), block.begin(), block.end());
	}
	_sent.clear();
	return commands;
}

void CommandQueue::Clear()
{
	_open.clear();
	_sent.clear();
}

size_t CommandQueue::SentCount() const
{
	return std::accumulate(_sent.begin(), _sent.end(), size_t {0},
	                       [](size_t total, const std::vector<Command>& block) { return total + block.size(); });
}
