/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>
#include <vector>

#include <entt/meta/context.hpp>

#include "MemoryUseRules.h"
#include "Window.h"

namespace openblack::debug::gui
{

/// How much memory each kind of component of the world uses: for each storage of the registry, how many entities have
/// the component and the bytes that the storage holds, the largest first, and the total
class MemoryUse final: public Window
{
public:
	MemoryUse() noexcept;
	MemoryUse(const MemoryUse&) = delete;
	MemoryUse& operator=(const MemoryUse&) = delete;
	MemoryUse(MemoryUse&&) = delete;
	MemoryUse& operator=(MemoryUse&&) = delete;
	~MemoryUse() noexcept override;

protected:
	void Draw() noexcept override;
	void Update() noexcept override {}
	void ProcessEventOpen(const SDL_Event&) noexcept override {}
	void ProcessEventAlways(const SDL_Event&) noexcept override {}

private:
	/// Reads the storages again
	void Take() noexcept;

	std::unique_ptr<entt::meta_ctx> _context;
	std::vector<memory_use::Row> _rows;
	int _frames {0};
};

} // namespace openblack::debug::gui
