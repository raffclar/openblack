/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/RealWeatherSystemInterface.h"

namespace openblack::ecs
{

/// The weather of the land in the game: the weather system of the locator, and the land that it is over. openblack has
/// no network game yet, so a game is never a multiplayer game.
class RealWeatherLand final: public systems::RealWeatherLandInterface
{
public:
	[[nodiscard]] components::WeatherInfo GetBackground() const override;
	void SetBackground(const components::WeatherInfo& weather) override;
	entt::entity AddStorm(const script_weather::LandStorm& storm) override;
	[[nodiscard]] bool StormLives(entt::entity storm) const override;
	void EndStorm(entt::entity storm) override;
	[[nodiscard]] glm::vec3 IslandCentre() const override;
	[[nodiscard]] bool IsMultiplayer() const override { return false; }
};

} // namespace openblack::ecs
