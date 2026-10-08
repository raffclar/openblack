/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GamePackets.h"

#include <cstdio>
#include <cstdlib>

#include <unordered_map>
#include <utility>
#include <vector>

#include "ECS/Registry.h"
#include "ECS/Systems/InputStateInterface.h"
#include "Locator.h"

namespace openblack::game_packets
{
namespace
{
struct GamePacketsState
{
	std::vector<Packet> buffer;  ///< The game's packet buffer
	std::vector<Packet> session; ///< The session's list
	std::unordered_map<uint8_t, Handler> handlers;
};

/// This module's state (Locator::inputState)
GamePacketsState& Get()
{
	if (!Locator::inputState::has_value())
	{
		std::fputs("game_packets: no input state in the locator (Locator::inputState)\n", stderr);
		std::abort();
	}
	return Locator::inputState::value().Get<GamePacketsState>();
}

constexpr size_t k_AutoFlush = 6; ///< Push flushes once the buffer holds 6 packets
} // namespace

void Push(const Packet& packet)
{
	auto& state = Get();
	state.buffer.push_back(packet);
	if (state.buffer.size() >= k_AutoFlush)
	{
		Flush();
	}
}

void Flush()
{
	auto& state = Get();
	state.session.insert(state.session.end(), state.buffer.begin(), state.buffer.end());
	state.buffer.clear();
}

void DispatchQueuedPackets()
{
	auto& state = Get();
	// the whole list, in the order it was sent; a handler may send new packets: they wait in the buffer for the next flush
	auto packets = std::move(state.session);
	state.session.clear();
	for (auto packet : packets)
	{
		// The index and its unique id must still match (an entity's version); otherwise the handler gets null
		if (packet.object != entt::null &&
		    (!Locator::entitiesRegistry::has_value() || !Locator::entitiesRegistry::value().Valid(packet.object)))
		{
			packet.object = entt::null;
		}
		if (const auto handler = state.handlers.find(static_cast<uint8_t>(packet.type)); handler != state.handlers.end())
		{
			handler->second(packet);
		}
	}
}

void SetHandler(Type type, Handler handler)
{
	Get().handlers[static_cast<uint8_t>(type)] = std::move(handler);
}

size_t Pending()
{
	const auto& state = Get();
	return state.buffer.size() + state.session.size();
}

void Reset()
{
	auto& state = Get();
	state.buffer.clear();
	state.session.clear();
}

void ClearHandlers()
{
	Get().handlers.clear();
}

} // namespace openblack::game_packets
