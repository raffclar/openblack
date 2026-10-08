/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/registry.hpp>

namespace openblack::ecs::systems
{
/// The state of the test and debug hooks (the OPENBLACK_TEST_* and trace variables), one struct per hook file, made on
/// first use. Each hook still reads its own environment variable; this only owns what it keeps between calls
/// (Locator::debugHooks)
class DebugHooksInterface
{
public:
	virtual ~DebugHooksInterface() = default;

	/// The hooks' state of type T, value-initialised on first use
	template <typename T>
	[[nodiscard]] T& Get()
	{
		auto& context = Store().ctx();
		if (!context.contains<T>())
		{
			context.emplace<T>();
		}
		return context.get<T>();
	}

protected:
	[[nodiscard]] virtual entt::registry& Store() noexcept = 0;
};
} // namespace openblack::ecs::systems
