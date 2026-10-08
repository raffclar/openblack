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

namespace openblack::psys::manager
{
struct State;
} // namespace openblack::psys::manager

namespace openblack::ecs::systems
{
/// The particle system's state: the running effects, the spot visual containers, the next effect id, the drawable
/// sources and the shields' defensive spheres. psys::manager's functions and the shield rules work on it
/// (Locator::particleSystem)
class ParticleSystemInterface
{
public:
	virtual ~ParticleSystemInterface() = default;

	[[nodiscard]] virtual psys::manager::State& GetState() noexcept = 0;

	/// A rule or creator file's own state of type T (each file keeps its struct type), value-initialised on first
	/// use; it lives as long as the service and goes after the effects. Only the main thread runs the effects
	template <typename T>
	[[nodiscard]] T& Module()
	{
		auto& context = ModuleStore().ctx();
		if (!context.contains<T>())
		{
			context.emplace<T>();
		}
		return context.get<T>();
	}

protected:
	[[nodiscard]] virtual entt::registry& ModuleStore() noexcept = 0;
};
} // namespace openblack::ecs::systems
