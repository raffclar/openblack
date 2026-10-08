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
/// The script side's state (the script highlights, the countdown, the land script save, the vortexes, the script
/// camera, the help and the mods' runtime), one struct per module, made on first use. Each module keeps its own struct
/// type; this owns them (Locator::scriptState)
class ScriptStateInterface
{
public:
	virtual ~ScriptStateInterface() = default;

	/// The module state of type T, value-initialised on first use; the reference stays valid until the service goes
	/// (the context keeps each struct on the heap). Only the main thread uses it
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
