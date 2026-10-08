/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/DebugHooksInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// Owns the debug hooks' state in the context of an entity registry of its own (no entities: only its context)
class DebugHooks final: public DebugHooksInterface
{
protected:
	[[nodiscard]] entt::registry& Store() noexcept override { return _store; }

private:
	entt::registry _store;
};
} // namespace openblack::ecs::systems
