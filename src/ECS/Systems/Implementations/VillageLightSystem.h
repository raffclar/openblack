/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "3D/NightLightsState.h"
#include "ECS/Systems/VillageLightSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// Holds the NightLights state, as the game starts it
class VillageLightSystem final: public VillageLightSystemInterface
{
public:
	[[nodiscard]] openblack::night_lights::State& GetState() noexcept override { return _state; }

private:
	openblack::night_lights::State _state;
};
} // namespace openblack::ecs::systems
