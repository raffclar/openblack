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

#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "ECS/GatheringBoxRules.h"

namespace openblack::online
{

/// The connection of the game to the ping server (see gathering_box). The messages go in single packets and the game
/// waits for no answer. The address of the server and the form of the messages wait on a user decision (question
/// GBX-1): no implementation is in the game yet, and the gathering box does nothing without one.
class PingServerInterface
{
public:
	/// The server tells which friends are online
	struct Online
	{
		std::vector<gathering_box::UserId> users;
	};
	/// A person sent a line
	struct Line
	{
		gathering_box::UserId user {0};
		std::u16string name;
		std::u16string text;
	};
	/// A person tells that it is here, with no line
	struct Here
	{
		gathering_box::UserId user {0};
		std::u16string name;
	};
	using Message = std::variant<Online, Line, Here>;

	virtual ~PingServerInterface() = default;

	/// The game can send now
	[[nodiscard]] virtual bool IsConnected() const = 0;
	/// Tells the server that the player plays
	virtual void Ping() = 0;
	/// Asks which of these friends are online
	virtual void AskOnline(std::span<const gathering_box::UserId> friends) = 0;
	/// Sends a line to a person
	virtual void SendLine(gathering_box::UserId user, std::u16string_view text) = 0;
	/// Sends a line of the staff to the server itself, with the name that the line shows (see staff_code)
	virtual void SendStaffLine(std::u16string_view name, std::u16string_view text) = 0;
	/// Tells all the people that a person with the staff number and this name is here (see staff_code)
	virtual void SendAnnouncement(gathering_box::UserId user, std::u16string_view name) = 0;
	/// Sends the report of the host (see host_report::Encode)
	virtual void SendHostReport(std::span<const uint8_t> report) = 0;
	/// The messages that came since the last call, in the order that they came
	[[nodiscard]] virtual std::vector<Message> TakeMessages() = 0;
};

} // namespace openblack::online
