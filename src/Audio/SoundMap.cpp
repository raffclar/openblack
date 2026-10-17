/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SoundMap.h"

#include <cmath>

#include <algorithm>

#include <LNDFile.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/X87.h"
#include "InfoConstants.h"

namespace openblack::audio
{

namespace
{
// Map coordinates are 16.16 fixed point cells, a cell is 10 world units
constexpr float k_MapCoordsPerCell = 65536.0f;
constexpr float k_UnitsPerCell = 10.0f;
constexpr float k_CellsPerMapCoord = 1.0f / 65536.0f;
// Scanned cells are measured at their centre: half the map's cell size of 8, times 0x2000
constexpr int32_t k_HalfCell = 0x8000;
constexpr uint16_t k_MapSize = 0x200;

constexpr float k_Percent = 0.01f;
// The game keeps these two wind constants as doubles
constexpr double k_WindVolumeStart = 15.0;
constexpr double k_WindVolumeRange = 1.0 / 30.0;
constexpr float k_MinimumWindVolume = 0.01f;

int16_t CellOf(int32_t coordinate)
{
	return static_cast<int16_t>(static_cast<uint32_t>(coordinate) >> 16);
}

/// Distance in world units of a map coordinate, as the game's scan calculates it. The product with 10 rounds to a
/// float, and then the division by 65536 is exact
float ToUnits(int32_t coordinate)
{
	return x87::Mul(static_cast<double>(coordinate), static_cast<double>(k_UnitsPerCell)) * k_CellsPerMapCoord;
}

/// The fade from 1 at the start of a range to 0 at its end. At the start or before it, the fade is 1
float RangeFade(float value, float start, float end)
{
	if (value <= start)
	{
		return 1.0f;
	}
	const auto fade = 1.0f - ((value - start) / (end - start));
	return 0.0f <= fade ? fade : 0.0f;
}

} // namespace

float sound_map::DistanceSquared(int32_t centerX, int32_t centerZ, int32_t cellX, int32_t cellZ)
{
	const auto dz = ToUnits(centerZ) - ToUnits(cellZ);
	const auto dx = ToUnits(centerX) - ToUnits(cellX);
	return (dx * dx) + (dz * dz);
}

uint16_t sound_map::WholeMetres(int32_t coordinate)
{
	return static_cast<uint16_t>(map_coords::FtoL(ToUnits(coordinate)));
}

void SoundMap::Update(const LandIslandInterface& island, const GSoundInfo& info, const Inputs& inputs)
{
	CalculateRadiusPointAndDistance(island, info, inputs.receiver);
	UpdateFromMap(island, _center);
	CalculateVolumes(island, info, inputs);
}

void SoundMap::CalculateRadiusPointAndDistance(const LandIslandInterface& island, const GSoundInfo& info,
                                               const glm::vec3& receiver)
{
	_receiver = receiver;
	_center.x = map_coords::ToFixed(receiver.x);
	_center.z = map_coords::ToFixed(receiver.z);
	_receiverHeight = receiver.y;

	const auto altitude =
	    GetAltitude(island, LandIslandInterface::ToMapCoords(receiver.x), LandIslandInterface::ToMapCoords(receiver.z));
	_heightAboveLand = _receiverHeight - altitude;
	_radius = info.radiusForMinAtmosVolume;
}

void SoundMap::UpdateFromMap(const LandIslandInterface& island, const MapCoords& center)
{
	const auto radius = map_coords::FtoL(_radius / k_UnitsPerCell * k_MapCoordsPerCell);
	const MapCoords min {.x = center.x - radius, .z = center.z - radius};
	const MapCoords max {.x = center.x + radius, .z = center.z + radius};

	Reset();

	for (int32_t x = CellOf(min.x); x <= CellOf(max.x); ++x)
	{
		for (int32_t z = CellOf(min.z); z <= CellOf(max.z); ++z)
		{
			const auto type = GetAtmosType(island, static_cast<uint16_t>(x), static_cast<uint16_t>(z));
			const MapCoords cell {
			    .x = static_cast<int32_t>((static_cast<uint32_t>(static_cast<uint16_t>(x)) << 16) | k_HalfCell),
			    .z = static_cast<int32_t>((static_cast<uint32_t>(static_cast<uint16_t>(z)) << 16) | k_HalfCell),
			};
			AddAtmosType(type, cell);
		}
	}
}

void SoundMap::Reset()
{
	for (auto& scan : _scans)
	{
		scan.count = 0;
		scan.distanceSquared = 65535.0f;
	}
	_totalCount = 0;
}

// Counts a cell of a type, and keeps it if it is the nearest yet
void SoundMap::AddAtmosType(uint32_t type, const MapCoords& coords)
{
	auto& scan = _scans.at(type < k_AtmosTypeCount ? type : 0);

	const auto distanceSquared = sound_map::DistanceSquared(_center.x, _center.z, coords.x, coords.z);
	if (distanceSquared < scan.distanceSquared)
	{
		scan.distanceSquared = distanceSquared;
		scan.nearestX = sound_map::WholeMetres(coords.x);
		scan.nearestZ = sound_map::WholeMetres(coords.z);
	}
	++scan.count;

	if (type != 0)
	{
		++_totalCount;
	}
}

uint32_t SoundMap::GetAtmosType(const LandIslandInterface& island, uint16_t x, uint16_t z)
{
	const auto* cell = x < k_MapSize && z < k_MapSize ? island.FindCell({x, z}) : nullptr;
	if (cell == nullptr)
	{
		// Off the map is open sea
		return static_cast<uint32_t>(AtmosType::Sea);
	}
	return (cell->flags >> 2) & 0xF;
}

float SoundMap::GetAltitude(const LandIslandInterface& island, int32_t x, int32_t z)
{
	return island.GetAltitude(x, z);
}

float SoundMap::DistanceFade(const GSoundInfo& info, AtmosType type) const
{
	const auto distance = std::sqrt(_scans.at(static_cast<size_t>(type)).distanceSquared);
	return RangeFade(distance, info.radiusForMaxAtmosVolume, info.radiusForMinAtmosVolume);
}

float SoundMap::HeightFade(const LandIslandInterface& island, const GSoundInfo& info, float x, float z) const
{
	const auto height =
	    _receiver.y - GetAltitude(island, LandIslandInterface::ToMapCoords(x), LandIslandInterface::ToMapCoords(z));
	if (height < info.normalAtmosFadeStartHeight)
	{
		return 1.0f;
	}
	if (height > info.normalAtmosFadeEndHeight)
	{
		return 0.0f;
	}
	const auto fade =
	    1.0f - ((height - info.normalAtmosFadeStartHeight) / (info.normalAtmosFadeEndHeight - info.normalAtmosFadeStartHeight));
	return 0.0f <= fade ? fade : 0.0f;
}

float SoundMap::StratosphereVolume(const GSoundInfo& info) const
{
	const auto height = _heightAboveLand;
	if (height < info.atmosphereHeight)
	{
		return 0.0f;
	}
	if (height < info.atmosphereMaxVolHeight)
	{
		const auto rise = (height - info.atmosphereHeight) / (info.atmosphereMaxVolHeight - info.atmosphereHeight);
		return 1.0f < rise ? 1.0f : rise;
	}
	if (height < info.spaceHeight)
	{
		const auto fall = 1.0f - ((height - info.atmosphereMaxVolHeight) / (info.spaceHeight - info.atmosphereMaxVolHeight));
		return 0.0f <= fall ? fall : 0.0f;
	}
	return 0.0f;
}

void SoundMap::CalculateVolumes(const LandIslandInterface& island, const GSoundInfo& info, const Inputs& inputs)
{
	const auto stratosphere = StratosphereVolume(info);

	// Full night only past dusk
	const auto nightDelta = inputs.skyType - 1.0f;
	const auto night = nightDelta < 0.0f ? 0.0f : nightDelta;

	// Wildlife fades out as the weather worsens
	const auto& weather = inputs.weather;
	auto clear = weather.snow > 0 ? 1.0f - (static_cast<float>(weather.snow) * k_Percent) : 1.0f;
	clear -= static_cast<float>(weather.rain) * k_Percent;
	clear = std::max(clear, 0.0f);
	const auto cutoff = info.weatherPercentageForMaxFade * k_Percent;
	const auto badWeather = 1.0f - clear;
	const auto wildlifeWeather = badWeather < cutoff ? 1.0f - (badWeather / cutoff) : 0.0f;

	// The square root rounds to a float before the two double constants apply
	const auto windSquared =
	    (static_cast<int32_t>(weather.windX) * weather.windX) + (static_cast<int32_t>(weather.windZ) * weather.windZ);
	const auto windSpeed = std::sqrt(static_cast<float>(windSquared));
	auto wind = x87::Mul(x87::Sub(windSpeed, k_WindVolumeStart), k_WindVolumeRange);
	if (wind > 1.0f)
	{
		wind = 1.0f;
	}
	else if (wind < k_MinimumWindVolume)
	{
		wind = 0.0f;
	}

	_volumes[static_cast<size_t>(AtmosType::Stratosphere)] = stratosphere;
	_volumes[static_cast<size_t>(AtmosType::None)] = 0.0f;

	const auto nearestHeightFade = [this, &island, &info](AtmosType type) {
		const auto& scan = _scans.at(static_cast<size_t>(type));
		return HeightFade(island, info, static_cast<float>(scan.nearestX), static_cast<float>(scan.nearestZ));
	};

	// Land painted types
	for (auto i = static_cast<size_t>(AtmosType::Sea); i <= static_cast<size_t>(AtmosType::RunningWater); ++i)
	{
		const auto type = static_cast<AtmosType>(i);
		if (_scans.at(i).count == 0)
		{
			_volumes.at(i) = 0.0f;
			continue;
		}
		auto volume = DistanceFade(info, type);
		if (k_AtmosTypeInfos.at(i).wildlife)
		{
			volume = (volume * wildlifeWeather) * (1.0f - night);
		}
		_volumes.at(i) = nearestHeightFade(type) * volume;
	}

	// The shore takes over from the open sea as it gets closer
	auto coastal = 0.0f;
	if (_scans[static_cast<size_t>(AtmosType::Coastal)].count == 0)
	{
		_volumes[static_cast<size_t>(AtmosType::Coastal)] = 0.0f;
	}
	else
	{
		coastal = DistanceFade(info, AtmosType::Coastal);
		_volumes[static_cast<size_t>(AtmosType::Coastal)] = nearestHeightFade(AtmosType::Coastal) * coastal;
	}

	if (_scans[static_cast<size_t>(AtmosType::Sea)].count == 0)
	{
		_volumes[static_cast<size_t>(AtmosType::Sea)] = 0.0f;
	}
	else
	{
		auto sea = DistanceFade(info, AtmosType::Sea);
		const auto limit = 1.0f - coastal;
		if (!(sea < limit))
		{
			sea = limit;
		}
		_volumes[static_cast<size_t>(AtmosType::Sea)] = nearestHeightFade(AtmosType::Sea) * sea;
	}

	// Night sounds only over land
	const auto seaFraction = _totalCount != 0 ? static_cast<float>(_scans[static_cast<size_t>(AtmosType::Sea)].count) /
	                                                static_cast<float>(_totalCount)
	                                          : 0.0f;
	const auto land = 1.0f - seaFraction;
	const auto receiverHeightFade = HeightFade(island, info, _receiver.x, _receiver.z);
	_volumes[static_cast<size_t>(AtmosType::Night)] = ((receiverHeightFade * land) * wildlifeWeather) * night;

	const auto rain = static_cast<float>(weather.rain);
	_volumes[static_cast<size_t>(AtmosType::Rain)] =
	    rain < info.weatherPercentageForMaxWeatherVolume ? rain / info.weatherPercentageForMaxWeatherVolume : 1.0f;

	_volumes[static_cast<size_t>(AtmosType::Wind)] = wind < 1.0f ? wind : 1.0f;
}

} // namespace openblack::audio
