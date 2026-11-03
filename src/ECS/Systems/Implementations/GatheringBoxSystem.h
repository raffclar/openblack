/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "ECS/Systems/GatheringBoxSystemInterface.h"
#include "Online/PingServerInterface.h"

namespace openblack::ecs::systems
{

class GatheringBoxSystem final: public GatheringBoxSystemInterface
{
public:
	/// What the system uses of the rest of the game. A service that is empty does nothing.
	struct Services
	{
		/// The lasting number of the player, none while the game does not know it
		std::function<std::optional<gathering_box::UserId>()> ownUser;
		/// The name of the player on the ping server
		std::function<std::u16string()> ownName;
		/// A line of a person shows in the incoming messages and goes to the chat log
		std::function<void(std::u16string_view name, std::u16string_view text)> showLine;
		/// The player made the questions of the staff plain lines of the chat
		std::function<bool()> plainChat;
		/// The facts of the report, none when this computer is not the host of an internet game
		std::function<std::optional<gathering_box::host_report::Facts>()> hostFacts;
		/// The friends that the profile of the player keeps
		std::function<std::vector<chat::people::FriendRecord>()> readFriends;
		/// The profile of the player keeps these friends
		std::function<void(std::span<const chat::people::FriendRecord> friends)> writeFriends;
	};

	explicit GatheringBoxSystem(Services services);

	void SetBox(Box box) override;
	void SetPingServer(std::unique_ptr<online::PingServerInterface> server) override;
	[[nodiscard]] bool HasPingServer() const override { return _server != nullptr; }
	void Update(uint32_t now) override;
	void SendToPicked(std::u16string_view text) override;
	void SendStaffCode(std::u16string_view text) override;
	void SendHostReport() override;
	[[nodiscard]] std::optional<gathering_box::question::Question> TakeQuestion() override;
	void Answer(const gathering_box::question::Question& question, std::u16string_view answer) override;
	[[nodiscard]] uint32_t GetOnlineTime() const override { return _onlineTime; }

private:
	void Receive(const online::PingServerInterface::Message& message);
	void Changed();
	void UpdateFriends();

	[[nodiscard]] chat::people::List* People() const;

	Services _services;
	Box _box;
	std::unique_ptr<online::PingServerInterface> _server;
	gathering_box::PingClock _pings;
	/// The time of the computer when the online clock started
	std::optional<uint32_t> _onlineStart;
	uint32_t _onlineTime {chat::people::k_OnlineClockStart};
	std::optional<gathering_box::question::Question> _question;
	/// The friend button is on because of the connection
	bool _friendsOn {false};
};

} // namespace openblack::ecs::systems
