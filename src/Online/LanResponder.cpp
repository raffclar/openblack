/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LanResponder.h"

#include <algorithm>
#include <initializer_list>
#include <utility>

namespace openblack::online
{

LanResponder::LanResponder(LanResponderParts parts)
    : _parts(std::move(parts))
{
	_server = _parts.random();
}

void LanResponder::Poll(const Room& room)
{
	const auto now = _parts.clock();
	if (now >= _nextInterfaceRead)
	{
		_interfaces = _parts.interfaces();
		_nextInterfaceRead = now + lan_discovery::k_InterfaceRefreshMicroseconds;
	}
	for (auto* socket : {_parts.v4.get(), _parts.v6.get()})
	{
		if (socket != nullptr && socket->IsOpen())
		{
			ReadSearches(*socket, now);
		}
	}
	SendDue(room, now);
}

void LanResponder::ReadSearches(DatagramSocketInterface& socket, int64_t now)
{
	// Forget the senders that can get an answer again
	std::erase_if(_sent, [now](const Sent& sent) { return now - sent.when >= lan_discovery::k_LeastAnswerGapMicroseconds; });
	for (size_t i = 0; i < lan_discovery::k_MostDatagramsInAFrame; ++i)
	{
		auto datagram = socket.Receive();
		if (!datagram.has_value())
		{
			return;
		}
		const auto session = lan_discovery::ParseSearch(datagram->bytes);
		if (!session.has_value() || !lan_discovery::IsOnLocalNetwork(datagram->from, _interfaces))
		{
			continue;
		}
		const auto same = [&datagram](const auto& item) { return item.to == datagram->from && item.port == datagram->port; };
		if (std::ranges::any_of(_waiting, same) || std::ranges::any_of(_sent, same) ||
		    _waiting.size() >= lan_discovery::k_MostWaitingAnswers)
		{
			continue;
		}
		_waiting.push_back({.to = datagram->from,
		                    .port = datagram->port,
		                    .session = *session,
		                    .due = now + lan_discovery::AnswerDelay(_parts.random())});
	}
}

void LanResponder::SendDue(const Room& room, int64_t now)
{
	if (std::ranges::none_of(_waiting, [now](const Waiting& item) { return item.due <= now; }))
	{
		return;
	}
	const auto answer = room();
	for (const auto& item : _waiting)
	{
		if (item.due > now)
		{
			continue;
		}
		auto& socket = item.to.family == lan_discovery::Family::V4 ? _parts.v4 : _parts.v6;
		if (socket != nullptr && socket->IsOpen())
		{
			socket->SendTo(item.to, item.port, lan_discovery::AnswerBytes(item.session, _server, answer));
		}
		_sent.push_back({.to = item.to, .port = item.port, .when = now});
	}
	std::erase_if(_waiting, [now](const Waiting& item) { return item.due <= now; });
}

} // namespace openblack::online
