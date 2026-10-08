/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::weather
{
struct State;
} // namespace openblack::weather

namespace openblack::ecs::systems
{
/// The weather's state: the atmosphere grid, the climates, the rain, the storms and their clouds, and the order of the
/// weather things. The weather's functions (atmos, climate, rain, storms, storm_clouds, weather_thing) work on it
/// (Locator::weatherSystem)
class WeatherSystemInterface
{
public:
	virtual ~WeatherSystemInterface() = default;

	[[nodiscard]] virtual weather::State& GetState() noexcept = 0;
};
} // namespace openblack::ecs::systems
