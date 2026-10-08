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
#include <list>

#include <glm/vec3.hpp>

#include "Storms.h"
#include "WeatherInfo.h"

// A climate: the world's (id 0, centre (2560, 2560), radius 5120) and the local ones of the map script
// (CREATE_WEATHER_CLIMATE*), newest first. Each adds its temperature and wind to ComputeWeather within its radius; once
// per game day its rain desire grows and at 1 it makes a natural storm somewhere in it.

namespace openblack::weather::climate
{
/// The rain part (CREATE_WEATHER_CLIMATE_RAIN sets all four)
struct Rain
{
	float desire {0.0f};     ///< 0..1 ("The Desire To Rain")
	int32_t dryDays {0};     ///< days without falling
	int32_t rainingDays {0}; ///< ("Raining Turns", counted per game day): x 0.01 against the season's rainMax
	uint8_t flags {0};       ///< bit 0: a storm of this climate is falling
};

struct Climate
{
	int32_t id {0};
	int32_t info {0}; ///< GClimateInfo index (the world's is always 0, WORLD)
	int32_t x {0};    ///< MapCoords 16.16 (metres x 6553.6)
	int32_t z {0};
	float y {0.0f};
	float innerRadius {5120.0f}; ///< the smaller of the script's radii
	float outerRadius {5120.0f};
	Rain rain;
	float temperature {0.0f};
	float targetTemperature {0.0f}; ///< recomputed every turn from the time of day and the month
	float windX {0.0f};
	float windZ {0.0f};
	float windAngle {0.0f};            ///< radians; the 5th constructor argument, 0 from the script, then _WIND's third
	int32_t maxStorms {10};            ///< 10 for the world, else int(radius2 x 0.001 + 1)
	std::list<storms::StormId> storms; ///< its natural storms, newest first
	bool world {false};
	int32_t stormElevation {500};
	float fallSpeed {0.5f};   ///< windMax[season] x 1/30 + 0.5
	float unknown0x70 {1.0f}; ///< (no known use)
	// 5 / 60 stored at construction
	float sheetMin {5.0f}; ///< the lightning of its hot storms
	float sheetMax {60.0f};
	float forkMin {5.0f};
	float forkMax {60.0f};
	/// lightning below 30 degrees too; zeroed when a local climate is made, no other writer found (UNVERIFIED)
	uint8_t lightning {0};

	/// The centre as every reader of it builds its point: x and z times 10 / 65536 (map_coords::ToMetres).
	/// ComputeWeather, ProcessAll and FindWhereToCreateStorm all do it the same way
	[[nodiscard]] glm::vec3 Centre() const;
	/// The centre as the climate process builds it for its two distance calls: the unsigned high word alone (the
	/// cell), times 10, so without the fraction
	[[nodiscard]] glm::vec3 CellCentre() const;
};

/// The static values' init and a cleared climate list (a new land): no climate, the climate system and the storm
/// creation on, the day, season and id counters back to 0 / 0 / 1
void Reset();

/// CREATE_WEATHER_CLIMATE (case 60): id 0 makes the world climate (which replaces the old one and ignores the rest),
/// any other a local one (radii sorted, the season's rain and temperature). Both go to the head of the list.
Climate& Create(const glm::vec3& position, int32_t info, float radius1, float radius2, float angle, int32_t id);
/// the first climate with that id (the newest), or nullptr
[[nodiscard]] Climate* Find(int32_t id);
/// The world climate, created on demand (Create with zeros) by every caller
Climate& World();
[[nodiscard]] bool HasWorld();

/// CREATE_WEATHER_CLIMATE_RAIN: the four values; id 0 = the world (made if missing)
void SetRain(int32_t id, const Rain& rain);
/// CREATE_WEATHER_CLIMATE_TEMP: temperature and target (id 0 needs the world to exist already)
void SetTemperature(int32_t id, float temperature, float target);
/// CREATE_WEATHER_CLIMATE_WIND: wind x, z and angle
void SetWind(int32_t id, float windX, float windZ, float angle);
/// CREATE_WEATHER_STORM: a storm of that climate (nothing if the id is unknown)
void CreateScriptStorm(int32_t id, const storms::StormDescriptor& descriptor, float age, float speed, const glm::vec3& target);

/// The weather at a point: every climate's temperature and wind within its radius
[[nodiscard]] WeatherInfo ComputeWeather(const glm::vec3& point, bool smooth);

/// Once per game turn, after the scripts
void ProcessAll(uint32_t turn);
/// A natural storm of the climate
void CreateStorm(Climate& climate, uint32_t turn);

/// PAUSE_UNPAUSE_CLIMATE_SYSTEM: the temperature, rain and wind updates
void SetClimateSystemEnabled(bool on);
/// PAUSE_UNPAUSE_STORM_CREATION_IN_CLIMATE_SYSTEM
void SetStormCreationEnabled(bool on);
[[nodiscard]] bool IsClimateSystemEnabled();
[[nodiscard]] bool IsStormCreationEnabled();
/// the season the climates use (changed on the first day of a new season)
[[nodiscard]] uint32_t CurrentSeason();

/// Every climate, newest first
void ForEach(const std::function<void(const Climate&)>& function);
} // namespace openblack::weather::climate
