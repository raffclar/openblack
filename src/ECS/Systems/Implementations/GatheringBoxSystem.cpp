/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "GatheringBoxSystem.h"

#include <type_traits>
#include <utility>
#include <variant>

using namespace openblack::ecs::systems;
using namespace openblack::gathering_box;

GatheringBoxSystem::GatheringBoxSystem(Services services)
    : _services(std::move(services))
{
}

void GatheringBoxSystem::SetPingServer(std::unique_ptr<online::PingServerInterface> server)
{
	_server = std::move(server);
}

void GatheringBoxSystem::Update(uint32_t now)
{
	if (_server == nullptr)
	{
		return;
	}
	// The online clock starts with the gathering box and runs with the time of the computer
	if (!_onlineStart.has_value())
	{
		_onlineStart = now;
	}
	_onlineTime = chat::people::k_OnlineClockStart + (now - *_onlineStart);

	auto* people = People();
	if (_pings.Due(now))
	{
		if (_server->IsConnected())
		{
			_server->Ping();
			SendHostReport();
			if (people != nullptr)
			{
				if (const auto friends = chat::people::FriendNumbers(*people); !friends.empty())
				{
					_server->AskOnline(friends);
				}
			}
		}
		if (people != nullptr)
		{
			chat::people::UpdateOnline(*people, {}, _onlineTime);
			Changed();
		}
	}
	for (const auto& message : _server->TakeMessages())
	{
		Receive(message);
	}
}

void GatheringBoxSystem::Receive(const online::PingServerInterface::Message& message)
{
	auto* people = People();
	const auto own = _services.ownUser ? _services.ownUser() : std::nullopt;
	std::visit(
	    [&](const auto& value) {
		    using T = std::decay_t<decltype(value)>;
		    if constexpr (std::is_same_v<T, online::PingServerInterface::Online>)
		    {
			    if (people != nullptr)
			    {
				    chat::people::UpdateOnline(*people, value.users, _onlineTime);
				    Changed();
			    }
		    }
		    else if constexpr (std::is_same_v<T, online::PingServerInterface::Here>)
		    {
			    if (people != nullptr && SeePerson(*people, value.name, value.user, own, _onlineTime) && _box.open)
			    {
				    _box.open();
			    }
			    Changed();
		    }
		    else
		    {
			    const auto text = ClippedLine(value.text);
			    const bool plain = _services.plainChat && _services.plainChat();
			    if (auto question = question::Parse(value.name, text, value.user, plain); question.has_value())
			    {
				    _question = std::move(question);
				    return;
			    }
			    if (people == nullptr)
			    {
				    return;
			    }
			    if (SeePerson(*people, value.name, value.user, own, _onlineTime) && _box.open)
			    {
				    _box.open();
			    }
			    Changed();
			    if (ShowsLine(*people, value.user) && _services.showLine)
			    {
				    _services.showLine(value.name, text);
			    }
		    }
	    },
	    message);
}

void GatheringBoxSystem::SendToPicked(std::u16string_view text)
{
	if (_server == nullptr || !_server->IsConnected())
	{
		return;
	}
	const auto* people = People();
	if (people == nullptr)
	{
		return;
	}
	for (const auto user : PickedFriendsAndOthers(*people))
	{
		_server->SendLine(user, text);
	}
}

void GatheringBoxSystem::SendHostReport()
{
	if (_server == nullptr || !_server->IsConnected() || !_services.hostFacts)
	{
		return;
	}
	if (const auto facts = _services.hostFacts(); facts.has_value())
	{
		_server->SendHostReport(host_report::Encode(*facts));
	}
}

std::optional<question::Question> GatheringBoxSystem::TakeQuestion()
{
	return std::exchange(_question, std::nullopt);
}

void GatheringBoxSystem::Answer(const question::Question& question, std::u16string_view answer)
{
	if (_server != nullptr && _server->IsConnected())
	{
		_server->SendLine(question.sender, answer);
	}
}

openblack::chat::people::List* GatheringBoxSystem::People() const
{
	return _box.people ? _box.people() : nullptr;
}

void GatheringBoxSystem::Changed()
{
	if (_box.peopleChanged)
	{
		_box.peopleChanged();
	}
}
