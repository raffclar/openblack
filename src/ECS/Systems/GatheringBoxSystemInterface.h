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

#include <functional>
#include <memory>
#include <optional>
#include <string_view>

#include "ECS/GatheringBoxRules.h"

namespace openblack::online
{
class PingServerInterface;
} // namespace openblack::online

namespace openblack::ecs::systems
{

/// The side of the gathering box that talks to the ping server (see gathering_box). With no connection to a ping
/// server, it does nothing. Nothing here is a part of the synced world.
class GatheringBoxSystemInterface
{
public:
	/// The gathering box on the screen. A hook that is empty does nothing.
	struct Box
	{
		/// The people of the box
		std::function<chat::people::List*()> people;
		/// The people changed, and the box must show the change
		std::function<void()> peopleChanged;
		/// The box opens
		std::function<void()> open;
	};

	virtual ~GatheringBoxSystemInterface() = default;

	/// The box of the interface, an empty box while the game has no interface
	virtual void SetBox(Box box) = 0;

	/// The connection to the ping server, none to stop
	virtual void SetPingServer(std::unique_ptr<online::PingServerInterface> server) = 0;
	[[nodiscard]] virtual bool HasPingServer() const = 0;

	/// Each frame, with the time of the computer in milliseconds:
	/// 1. When the ping is due and the game has a connection: the ping, the report of the host, and the question
	///    about the friends that are online, when the player has friends.
	/// 2. When the ping is due: the friends that the server did not see for 450 seconds are no longer online.
	/// 3. The messages of the server: the online friends, the people that are here, the lines and the questions.
	virtual void Update(uint32_t now) = 0;

	/// The player sent a typed line or the words of a taunt: they go also to the picked friends and others
	virtual void SendToPicked(std::u16string_view text) = 0;
	/// The host tells the server the state of the game at once, as when the game is decided
	virtual void SendHostReport() = 0;

	/// The question of the staff that the box must ask, once
	[[nodiscard]] virtual std::optional<gathering_box::question::Question> TakeQuestion() = 0;
	/// The player answered a question: the answer goes back to its sender
	virtual void Answer(const gathering_box::question::Question& question, std::u16string_view answer) = 0;

	/// The time of the online clock, in milliseconds
	[[nodiscard]] virtual uint32_t GetOnlineTime() const = 0;
};

} // namespace openblack::ecs::systems
