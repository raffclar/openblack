/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <functional>
#include <optional>
#include <vector>

#include "ECS/RealWeatherRules.h"
#include "ECS/Systems/LobbyClientSystemInterface.h"
#include "ECS/Systems/RealWeatherSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class RealWeatherSystem final: public RealWeatherSystemInterface
{
public:
	/// The presets come from Scripts/weatherinfo.lhw, none when the file is not there
	using ReadPresets = std::function<std::optional<std::vector<uint8_t>>()>;
	/// The date of today on the computer, as the game asks the lobby for it
	using Today = std::function<lobby_api::Date()>;

	RealWeatherSystem(bool turnedOn, LobbyClientSystemInterface& lobby, ReadPresets readPresets, Today today);

	void StartGame(RealWeatherLandInterface& land) override;
	void ProcessTurn(RealWeatherLandInterface& land) override;
	void Request(bool on) override;

	[[nodiscard]] std::optional<lobby_api::RealWeather> GetWeather() const override { return _weather; }
	[[nodiscard]] bool IsApplied() const override { return !_landWeatherInPlace; }
	[[nodiscard]] entt::entity GetStorm() const override { return _storm; }

private:
	enum class Wanted : uint8_t
	{
		Nothing,
		/// The land's own background weather goes back
		Off,
		/// The real weather goes on the land
		On,
	};

	void Got(lobby_api::RealWeather weather);

	bool _turnedOn;
	LobbyClientSystemInterface& _lobby;
	ReadPresets _readPresets;
	Today _today;

	std::vector<real_weather::Preset> _presets;
	std::optional<lobby_api::RealWeather> _weather;
	/// The number of the game. An answer for an earlier game is ignored.
	uint32_t _game {0};
	/// The system works in the turns only after the lobby gave the weather
	bool _active {false};
	Wanted _wanted {Wanted::Nothing};
	/// The land's own weather is in place, not the real weather
	bool _landWeatherInPlace {true};
	/// The first turn after the weather came puts it on the land
	bool _firstTurnDone {false};
	/// The land's own background weather, kept at the first turn so that it can go back. The game keeps it one time
	/// and never again, also for a later game.
	std::optional<components::WeatherInfo> _landWeather;
	/// The background weather of the real weather when it went off
	std::optional<components::WeatherInfo> _realWeather;
	entt::entity _storm {entt::null};
};

} // namespace openblack::ecs::systems
