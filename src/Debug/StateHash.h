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

#include <memory>
#include <optional>
#include <string>

#include <entt/meta/context.hpp>

#include "StateHashWatch.h"
#include "Window.h"

namespace openblack::debug::gui
{

/// The hash of the land's synced state, to check that two runs stay the same. The window takes the hash at the turn it
/// is asked or at a set number of turns, pins one, and tells whether the current hash is the pinned one and which parts
/// of the world differ when it is not. A hash taken here is the one that the run records and the inspector's ecs.hash
/// keep for the same turn.
class StateHash final: public Window
{
public:
	StateHash() noexcept;
	~StateHash() noexcept override;

protected:
	void Draw() noexcept override;
	void Update() noexcept override {}
	void UpdateAlways() noexcept override;
	void ProcessEventOpen(const SDL_Event&) noexcept override {}
	void ProcessEventAlways(const SDL_Event&) noexcept override {}

private:
	void Take() noexcept;

	std::unique_ptr<entt::meta_ctx> _context;
	std::optional<state_hash::Snapshot> _current;
	std::optional<state_hash::Snapshot> _pinned;
	/// Take the hash each time this many turns go by; 0 for only when asked
	int _every {0};
	std::string _pasteLine;
};

} // namespace openblack::debug::gui
