/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "Online/PingServerInterface.h"

namespace openblack::testbed_scenarios
{

/// A ping server for the testbed. It is always connected. The testbed gives it the messages that it sends, and it
/// keeps what the game sends to it for the log of the scenario.
class TestbedPingServer final: public online::PingServerInterface
{
public:
	/// What the testbed and the game share
	struct Mailbox
	{
		std::vector<Message> toGame;
		std::vector<std::pair<gathering_box::UserId, std::u16string>> lines;
		int pings {0};
	};

	explicit TestbedPingServer(std::shared_ptr<Mailbox> mailbox)
	    : _mailbox(std::move(mailbox))
	{
	}
	[[nodiscard]] bool IsConnected() const override { return true; }
	void Ping() override { ++_mailbox->pings; }
	void AskOnline(std::span<const gathering_box::UserId> /*friends*/) override {}
	void SendLine(gathering_box::UserId user, std::u16string_view text) override
	{
		_mailbox->lines.emplace_back(user, std::u16string(text));
	}
	void SendHostReport(std::span<const uint8_t> /*report*/) override {}
	[[nodiscard]] std::vector<Message> TakeMessages() override { return std::exchange(_mailbox->toGame, {}); }

private:
	std::shared_ptr<Mailbox> _mailbox;
};

} // namespace openblack::testbed_scenarios
