/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LanSearcher.h"

#include <initializer_list>
#include <utility>

namespace openblack::online
{

LanSearcher::LanSearcher(LanSearchParts parts)
    : _parts(std::move(parts))
{
}

bool LanSearcher::IsOpen(const std::unique_ptr<DatagramSocketInterface>& socket)
{
	return socket != nullptr && socket->IsOpen();
}

bool LanSearcher::IsAvailable() const
{
	return IsOpen(_parts.v4) || IsOpen(_parts.v6);
}

void LanSearcher::Start()
{
	_servers.Clear();
	_searching = true;
	// A new session for each start, so that late answers to an old search do not show
	_session = _parts.random();
	_searchIndex = 0;
	const auto now = _parts.clock();
	ReadInterfaces(now);
	_nextSearch = now + lan_discovery::SearchDelay(_searchIndex, _parts.random());
}

void LanSearcher::Stop()
{
	_searching = false;
}

void LanSearcher::Poll()
{
	if (!_searching)
	{
		return;
	}
	const auto now = _parts.clock();
	if (now >= _nextInterfaceRead)
	{
		ReadInterfaces(now);
	}
	if (now >= _nextSearch)
	{
		SendSearch(now);
	}
	for (auto* socket : {_parts.v4.get(), _parts.v6.get()})
	{
		if (socket != nullptr && socket->IsOpen())
		{
			ReadAnswers(*socket, now);
		}
	}
}

void LanSearcher::ReadInterfaces(int64_t now)
{
	_interfaces = _parts.interfaces();
	_nextInterfaceRead = now + lan_discovery::k_InterfaceRefreshMicroseconds;
}

void LanSearcher::SendSearch(int64_t now)
{
	_servers.Searched(now);
	const auto search = lan_discovery::SearchBytes(_session);
	const auto send = [this, &search](const std::unique_ptr<DatagramSocketInterface>& socket, lan_discovery::Family family) {
		if (!IsOpen(socket))
		{
			return;
		}
		for (const auto& target : lan_discovery::SearchTargets(family, _interfaces))
		{
			socket->SendTo(target, lan_discovery::k_Port, search);
		}
	};
	send(_parts.v4, lan_discovery::Family::V4);
	send(_parts.v6, lan_discovery::Family::V6);
	++_searchIndex;
	_nextSearch = now + lan_discovery::SearchDelay(_searchIndex, _parts.random());
}

void LanSearcher::ReadAnswers(DatagramSocketInterface& socket, int64_t now)
{
	for (size_t i = 0; i < lan_discovery::k_MostDatagramsInAFrame; ++i)
	{
		auto datagram = socket.Receive();
		if (!datagram.has_value())
		{
			return;
		}
		// Only answers to this session, from the local network, go in the list
		const auto answer = lan_discovery::ParseAnswer(datagram->bytes);
		if (!answer.has_value() || answer->session != _session || !lan_discovery::IsOnLocalNetwork(datagram->from, _interfaces))
		{
			continue;
		}
		_servers.Heard(*answer, datagram->from, now);
	}
}

} // namespace openblack::online
