/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LobbyPingServer.h"

#include <limits>
#include <string>
#include <utility>

#include <LobbyApiJson.h>

#include "ECS/Systems/LobbyClientSystemInterface.h"
#include "Gui/TextDatabase.h"

namespace openblack::online
{

using ecs::systems::LobbyResult;
using ecs::systems::LobbySessionState;

std::optional<gathering_box::UserId> lobby_ping::UserOf(int64_t account) noexcept
{
	if (account <= 0 || account > static_cast<int64_t>(std::numeric_limits<gathering_box::UserId>::max()))
	{
		return std::nullopt;
	}
	return static_cast<gathering_box::UserId>(account);
}

std::vector<PingServerInterface::Message> lobby_ping::MessagesOf(const lobby_api::PingReply& reply)
{
	std::vector<PingServerInterface::Message> messages;
	PingServerInterface::Online online;
	for (const auto account : reply.online)
	{
		if (const auto user = UserOf(account); user.has_value())
		{
			online.users.push_back(*user);
		}
	}
	messages.emplace_back(std::move(online));
	for (const auto& person : reply.here)
	{
		if (const auto user = UserOf(person.id); user.has_value())
		{
			messages.emplace_back(PingServerInterface::Here {.user = *user, .name = gui::ToUtf16(person.name)});
		}
	}
	return messages;
}

std::vector<PingServerInterface::Message> lobby_ping::MessagesOf(const lobby_api::PushEvent& event)
{
	std::vector<PingServerInterface::Message> messages;
	if (event.type == lobby_api::events::k_PingLine)
	{
		const auto line = lobby_api::ParsePingLine(event.data);
		if (!line.has_value())
		{
			return messages;
		}
		if (const auto user = UserOf(line->from.id); user.has_value())
		{
			messages.emplace_back(PingServerInterface::Line {
			    .user = *user, .name = gui::ToUtf16(line->from.name), .text = gui::ToUtf16(line->text)});
		}
	}
	else if (event.type == lobby_api::events::k_PingHere)
	{
		const auto here = lobby_api::ParsePingHere(event.data);
		if (!here.has_value())
		{
			return messages;
		}
		for (const auto& person : here->people)
		{
			if (const auto user = UserOf(person.id); user.has_value())
			{
				messages.emplace_back(PingServerInterface::Here {.user = *user, .name = gui::ToUtf16(person.name)});
			}
		}
	}
	return messages;
}

LobbyPingServer::LobbyPingServer(LobbyGetter lobby)
    : _lobby(std::move(lobby))
    , _inbox(std::make_shared<std::vector<Message>>())
{
	Listen();
}

LobbyPingServer::~LobbyPingServer()
{
	auto* lobby = _lobby ? _lobby() : nullptr;
	if (lobby != nullptr && _listener.has_value())
	{
		lobby->RemoveEventListener(*_listener);
	}
}

void LobbyPingServer::Listen()
{
	auto* lobby = _lobby ? _lobby() : nullptr;
	if (lobby == nullptr || _listener.has_value())
	{
		return;
	}
	_listener = lobby->AddEventListener([inbox = std::weak_ptr(_inbox)](const lobby_api::PushEvent& event) {
		if (auto messages = inbox.lock(); messages != nullptr)
		{
			for (auto& message : lobby_ping::MessagesOf(event))
			{
				messages->push_back(std::move(message));
			}
		}
	});
}

bool LobbyPingServer::IsConnected() const
{
	const auto* lobby = _lobby ? _lobby() : nullptr;
	return lobby != nullptr && lobby->GetState() == LobbySessionState::LoggedIn && lobby->IsPushOpen();
}

void LobbyPingServer::Ping()
{
	_pingDue = true;
	_friends.clear();
}

void LobbyPingServer::AskOnline(std::span<const gathering_box::UserId> friends)
{
	_friends.assign(friends.begin(), friends.end());
}

void LobbyPingServer::SendLine(gathering_box::UserId user, std::u16string_view text)
{
	auto* lobby = _lobby ? _lobby() : nullptr;
	if (lobby == nullptr)
	{
		return;
	}
	// As a packet of the original game, the line gets no answer: a failure is dropped
	lobby->SendPersonLine(user, gui::ToUtf8(text), [](LobbyResult<ecs::systems::LobbyDone>&&) {});
}

void LobbyPingServer::SendHostReport(std::span<const uint8_t> /*report*/)
{
	// The game server is the host of a network game of openblack, and it sends the report
}

void LobbyPingServer::SendPing()
{
	auto* lobby = _lobby ? _lobby() : nullptr;
	if (!_pingDue || lobby == nullptr)
	{
		return;
	}
	_pingDue = false;
	lobby->SendPing(std::exchange(_friends, {}), [inbox = std::weak_ptr(_inbox)](LobbyResult<lobby_api::PingReply>&& result) {
		auto messages = inbox.lock();
		if (messages == nullptr || !result.Ok())
		{
			return;
		}
		for (auto& message : lobby_ping::MessagesOf(*result.value))
		{
			messages->push_back(std::move(message));
		}
	});
}

std::vector<PingServerInterface::Message> LobbyPingServer::TakeMessages()
{
	Listen();
	SendPing();
	return std::exchange(*_inbox, {});
}

} // namespace openblack::online
