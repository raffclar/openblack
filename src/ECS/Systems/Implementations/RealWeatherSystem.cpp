/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "RealWeatherSystem.h"

#include <array>
#include <string_view>
#include <utility>

#include <spdlog/spdlog.h>

namespace openblack::ecs::systems
{

RealWeatherSystem::RealWeatherSystem(bool turnedOn, LobbyClientSystemInterface& lobby, ReadPresets readPresets, Today today,
                                     Clock clock, const ReadCountries& readCountries, WriteDays writeDays, Tell tell)
    : _turnedOn(turnedOn)
    , _lobby(lobby)
    , _readPresets(std::move(readPresets))
    , _today(std::move(today))
    , _clock(std::move(clock))
    , _writeDays(std::move(writeDays))
    , _tell(std::move(tell))
{
	// The list of countries gives the full names of the countries in the message about the weather
	if (const auto bytes = readCountries ? readCountries() : std::nullopt; bytes.has_value())
	{
		_countries =
		    real_weather::ParseCountries(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
	}
}

std::optional<std::string> RealWeatherSystem::GetPlace() const
{
	if (!_weather.has_value())
	{
		return std::nullopt;
	}
	return real_weather::PlaceText(_weather->city, _weather->area, real_weather::CountryName(_countries, _weather->country));
}

void RealWeatherSystem::StartGame(RealWeatherLandInterface& land)
{
	const auto game = ++_game;
	if (land.IsMultiplayer())
	{
		return;
	}
	// Without the file of the presets the real weather does nothing
	const auto bytes = _readPresets ? _readPresets() : std::nullopt;
	if (!bytes.has_value())
	{
		return;
	}
	_presets = real_weather::ParsePresets(*bytes);
	// The game asks only when the player turned the real weather on and has an account
	if (!_turnedOn || !_lobby.IsConfigured())
	{
		return;
	}
	const auto date = _today();
	// The game tries one time and never waits: an answer after the timeout leaves the land's own weather
	_askedAt = _clock();
	auto ask = [this, game, date]() {
		_lobby.GetWeather(date, [this, game, date](LobbyResult<lobby_api::RealWeather>&& result) {
			// Only an answer for this game counts
			if (game != _game)
			{
				return;
			}
			if (_clock() - _askedAt > k_AnswerTimeout)
			{
				if (const auto logger = spdlog::get("game"))
				{
					SPDLOG_LOGGER_INFO(logger, "The real weather came too late: the land keeps its own weather");
				}
				return;
			}
			if (!result.Ok())
			{
				if (const auto logger = spdlog::get("game"))
				{
					SPDLOG_LOGGER_INFO(logger, "The lobby gave no real weather");
				}
				return;
			}
			Got(std::move(*result.value), date);
		});
	};
	if (_lobby.GetState() == LobbySessionState::LoggedIn)
	{
		ask();
		return;
	}
	_lobby.ResumeSession([this, game, ask = std::move(ask)](LobbyResult<lobby_api::AccountRef>&& result) {
		if (result.Ok() && game == _game)
		{
			ask();
		}
	});
}

void RealWeatherSystem::Got(lobby_api::RealWeather weather, lobby_api::Date date)
{
	if (const auto logger = spdlog::get("game"))
	{
		SPDLOG_LOGGER_INFO(logger, "Real weather in {}: type {}, {} degrees", weather.city, weather.type, weather.temperature);
	}
	// The game keeps the days that it got in the folder of the player. The lobby gives the day that the game asked for.
	if (_writeDays)
	{
		const std::array days {real_weather::KeptDay {.country = weather.country,
		                                              .area = weather.area,
		                                              .city = weather.city,
		                                              .day = date.day,
		                                              .month = date.month,
		                                              .year = date.year,
		                                              .temperature = weather.temperature,
		                                              .visibility = weather.visibility,
		                                              .type = weather.type}};
		_writeDays(real_weather::DaysFile(days));
	}
	_weather = std::move(weather);
	_wanted = Wanted::On;
	_landWeatherInPlace = true;
	_active = true;
}

void RealWeatherSystem::Request(bool on)
{
	_wanted = on ? Wanted::On : Wanted::Off;
}

void RealWeatherSystem::ProcessTurn(RealWeatherLandInterface& land)
{
	if (!_active || land.IsMultiplayer())
	{
		return;
	}
	if (!_firstTurnDone && _wanted != Wanted::Nothing)
	{
		_wanted = Wanted::On;
		_firstTurnDone = true;
	}
	if (!_landWeather.has_value())
	{
		_landWeather = land.GetBackground();
	}
	if (_storm != entt::null && !land.StormLives(_storm))
	{
		_storm = entt::null;
	}
	if (_wanted == Wanted::Nothing || !_weather.has_value() || _weather->type == real_weather::k_NoType)
	{
		return;
	}

	if (_wanted == Wanted::On)
	{
		if (_landWeatherInPlace)
		{
			const auto applied = real_weather::ApplyPreset(real_weather::FindPreset(_presets, _weather->type),
			                                               land.GetBackground(), _weather->temperature, _weather->visibility,
			                                               real_weather::PresetStorm(land.IslandCentre()));
			land.SetBackground(applied.background);
			if (applied.storm.has_value())
			{
				_storm = land.AddStorm(*applied.storm);
			}
			_landWeatherInPlace = false;
			// The message names the place and shows the picture of the weather
			if (_tell)
			{
				_tell(*GetPlace(), real_weather::PictureOf(_weather->type));
			}
		}
	}
	else if (!_landWeatherInPlace)
	{
		// The land's own weather goes back, and the storm of the real weather ends
		_realWeather = land.GetBackground();
		if (_storm != entt::null)
		{
			land.EndStorm(_storm);
			_storm = entt::null;
		}
		if (_landWeather.has_value())
		{
			land.SetBackground(*_landWeather);
		}
		_landWeatherInPlace = true;
	}
	_wanted = Wanted::Nothing;
}

} // namespace openblack::ecs::systems
