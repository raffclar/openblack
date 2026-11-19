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

WaitingCommands CommandQueue::Waiting() const
{
	WaitingCommands waiting {.sent = {}, .open = _open};
	waiting.sent.reserve(SentCount());
	for (const auto& block : _sent)
	{
		waiting.sent.insert(waiting.sent.end(), block.begin(), block.end());
	}
	return waiting;
}

WaitingCommands openblack::player_commands::KeptByLoad(WaitingCommands before)
{
	return WaitingCommands {.sent = std::move(before.open), .open = {}};
}

void CommandQueue::PutBack(WaitingCommands waiting)
{
	Clear();
	if (!waiting.sent.empty())
	{
		_sent.push_back(std::move(waiting.sent));
	}
	_open = std::move(waiting.open);
}

size_t CommandQueue::SentCount() const
{
	return std::accumulate(_sent.begin(), _sent.end(), size_t {0},
	                       [](size_t total, const std::vector<Command>& block) { return total + block.size(); });
}
