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

#include <string_view>

namespace openblack::ecs::systems
{

/// The debug inspector: a local server that agents and tools query for the game's state (entities, components, the
/// moon, splashes and the like) and control the run through (pause, resume, step, scenarios). Built only with the
/// inspector option and started only by the --inspect-port flag; without them it is not there at all.
class InspectorSystemInterface
{
public:
	virtual ~InspectorSystemInterface() = default;

	/// Once a frame, before the game's turn: answers the requests that came in since the last frame, then holds or
	/// releases the game for any stepping asked for
	virtual void Service() = 0;
	/// The port it listens on, on 127.0.0.1
	[[nodiscard]] virtual uint16_t GetPort() const = 0;
	/// The game is about to load something that takes long (a land, the testbed), during which it serves no frames:
	/// meanwhile the inspector answers on its own, saying it is loading (game.state is not ready, other queries are
	/// refused as loading), so tools wait rather than time out. Nested loads count as one.
	virtual void BeginLoading(std::string_view what) = 0;
	/// The load has finished: requests are answered by the game's frames again
	virtual void EndLoading() = 0;
};

} // namespace openblack::ecs::systems
