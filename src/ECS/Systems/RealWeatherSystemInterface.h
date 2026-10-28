/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <chrono>
#include <optional>

#include <LobbyApi.h>
#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/Weather.h"
#include "ECS/ScriptWeatherRules.h"

namespace openblack::ecs::systems
{

/// The weather of the land that the real-world weather changes
class RealWeatherLandInterface
{
public:
	virtual ~RealWeatherLandInterface() = default;

	/// The weather of each place before the storms add to it
	[[nodiscard]] virtual components::WeatherInfo GetBackground() const = 0;
	virtual void SetBackground(const components::WeatherInfo& weather) = 0;
	/// Makes a storm that stands where it is made and belongs to no climate
	virtual entt::entity AddStorm(const script_weather::LandStorm& storm) = 0;
	[[nodiscard]] virtual bool StormLives(entt::entity storm) const = 0;
	virtual void EndStorm(entt::entity storm) = 0;
	/// The middle of the island: half way between the first and the last block that holds land
	[[nodiscard]] virtual glm::vec3 IslandCentre() const = 0;
	[[nodiscard]] virtual bool IsMultiplayer() const = 0;
};

/// The real-world weather, as the original game's internet weather. It is on unless the player turns it off. When it
/// is on, a single-player game asks the lobby for the weather of the place of the player's account as the game starts.
/// The game does not wait for the answer. At the next turn after the answer the weather changes the background weather
/// of the land, and its preset can make a storm. An answer that comes too late, or no answer, leaves the land's own
/// weather.
class RealWeatherSystemInterface
{
public:
	/// The longest time from the question to the answer. A later answer is ignored.
	static constexpr auto k_AnswerTimeout = std::chrono::seconds(10);

	virtual ~RealWeatherSystemInterface() = default;

	/// Once as a game starts: when the real weather is on and the player has a lobby account, the system asks the lobby
	/// for the weather of today. Nothing happens in a multiplayer game.
	virtual void StartGame(RealWeatherLandInterface& land) = 0;
	/// Once a turn, after the turn of the game: the weather that came changes the weather of the land
	virtual void ProcessTurn(RealWeatherLandInterface& land) = 0;
	/// Puts the real weather on the land, or puts the land's own background weather back and ends the storm of the real
	/// weather. The original game did this with its developer keys only.
	virtual void Request(bool on) = 0;

	/// The weather that the lobby gave for this game, if any
	[[nodiscard]] virtual std::optional<lobby_api::RealWeather> GetWeather() const = 0;
	/// Whether the real weather changes the land now
	[[nodiscard]] virtual bool IsApplied() const = 0;
	/// The storm of the real weather, null when there is none
	[[nodiscard]] virtual entt::entity GetStorm() const = 0;
};

} // namespace openblack::ecs::systems
