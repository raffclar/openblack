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
#include <span>
#include <string_view>
#include <vector>

#include <LobbyApi.h>

#include "Online/PingServerInterface.h"

namespace openblack::ecs::systems
{
class LobbyClientSystemInterface;
} // namespace openblack::ecs::systems

namespace openblack::online
{

/// The ping server of the gathering box through the lobby (deviation 220). The original game sent packets to its own
/// ping server. openblack sends the same messages to the HTTPS API of the lobby, and gets the messages of the server
/// as events of the push channel:
/// - The ping and the question about the online friends go in one call, at the end of the frame of the ping.
/// - A line to a person goes to the lobby account with that number.
/// - "ping.line" events are the lines of people and of the staff. "ping.here" events and the answer of a ping give
///   the announcements, each as a person of the staff that is here.
/// - The host report comes from the game server, so a game sends none.
class LobbyPingServer final: public PingServerInterface
{
public:
	/// Gives the lobby client, or none when the game has no lobby client now
	using LobbyGetter = std::function<ecs::systems::LobbyClientSystemInterface*()>;

	explicit LobbyPingServer(LobbyGetter lobby);
	~LobbyPingServer() override;
	LobbyPingServer(const LobbyPingServer&) = delete;
	LobbyPingServer& operator=(const LobbyPingServer&) = delete;
	LobbyPingServer(LobbyPingServer&&) = delete;
	LobbyPingServer& operator=(LobbyPingServer&&) = delete;

	/// The player is logged in to the lobby and its push channel is open
	[[nodiscard]] bool IsConnected() const override;
	void Ping() override;
	void AskOnline(std::span<const gathering_box::UserId> friends) override;
	void SendLine(gathering_box::UserId user, std::u16string_view text) override;
	/// The lobby has no path for the lines and the announcements of the staff code yet (question GB2-1)
	void SendStaffLine(std::u16string_view name, std::u16string_view text) override;
	void SendAnnouncement(gathering_box::UserId user, std::u16string_view name) override;
	void SendHostReport(std::span<const uint8_t> report) override;
	[[nodiscard]] std::vector<Message> TakeMessages() override;

private:
	void SendPing();
	void Listen();

	LobbyGetter _lobby;
	/// The messages that came. The callbacks of the lobby keep a weak pointer, so an answer after the end of this object
	/// goes nowhere.
	std::shared_ptr<std::vector<Message>> _inbox;
	std::optional<uint32_t> _listener;
	bool _pingDue {false};
	std::vector<int64_t> _friends;
};

/// The rules of the messages, with no state
namespace lobby_ping
{

/// The number of a person in the gathering box for a lobby account, none when the id does not fit
[[nodiscard]] std::optional<gathering_box::UserId> UserOf(int64_t account) noexcept;
/// The messages of an answer of a ping: the online friends, then each announcement as a person that is here
[[nodiscard]] std::vector<PingServerInterface::Message> MessagesOf(const lobby_api::PingReply& reply);
/// The messages of an event of the push channel. An event that is not of the ping server gives none.
[[nodiscard]] std::vector<PingServerInterface::Message> MessagesOf(const lobby_api::PushEvent& event);

} // namespace lobby_ping

} // namespace openblack::online
