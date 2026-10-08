/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/ParticleSystemInterface.h"
#include "Particles/PSysManagerState.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// Holds the particle system's state; it starts empty, with the first effect id 1
class ParticleSystem final: public ParticleSystemInterface
{
public:
	[[nodiscard]] psys::manager::State& GetState() noexcept override { return _state; }

protected:
	[[nodiscard]] entt::registry& ModuleStore() noexcept override { return _modules; }

private:
	/// The files' own state, declared first so that it goes after the effects that point into it
	entt::registry _modules;
	psys::manager::State _state;
};
} // namespace openblack::ecs::systems
