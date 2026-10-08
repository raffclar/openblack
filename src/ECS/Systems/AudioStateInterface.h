/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <mutex>

#include <entt/entity/registry.hpp>

namespace openblack::ecs::systems
{
/// The audio modules' state (the device, the banks, the sample channels, the music, the sound services), one struct
/// per module, made on first use. Each module keeps its own struct type; this owns them (Locator::audioState)
class AudioStateInterface
{
public:
	virtual ~AudioStateInterface() = default;

	/// The module state of type T, value-initialised on first use. The music thread reaches some of it, so the lookup
	/// is locked; the reference stays valid until the service goes (the context keeps each struct on the heap)
	template <typename T>
	[[nodiscard]] T& Get()
	{
		const std::lock_guard lock(Mutex());
		auto& context = Store().ctx();
		if (!context.contains<T>())
		{
			context.emplace<T>();
		}
		return context.get<T>();
	}

protected:
	[[nodiscard]] virtual entt::registry& Store() noexcept = 0;
	[[nodiscard]] virtual std::mutex& Mutex() noexcept = 0;
};
} // namespace openblack::ecs::systems
