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

#include "ECS/Systems/AudioStateInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// Owns the audio modules' state in the context of an entity registry of its own (no entities: only its context)
class AudioState final: public AudioStateInterface
{
protected:
	[[nodiscard]] entt::registry& Store() noexcept override { return _store; }
	[[nodiscard]] std::mutex& Mutex() noexcept override { return _mutex; }

private:
	entt::registry _store;
	std::mutex _mutex;
};
} // namespace openblack::ecs::systems
