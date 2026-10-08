/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/Weather/WeatherState.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// Holds the weather's state, as the game starts it
class WeatherSystem final: public WeatherSystemInterface
{
public:
	[[nodiscard]] weather::State& GetState() noexcept override { return _state; }

private:
	weather::State _state;
};
} // namespace openblack::ecs::systems
