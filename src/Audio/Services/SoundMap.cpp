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
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <string>

#include <LNDFile.h>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Audio/Game/AudioSystem.h"
#include "Camera/Camera.h"
#include "Common/GUtilsDistance.h"
#include "ECS/Systems/AudioStateInterface.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::audio;

const std::array<AtmosTypeInfo, k_AtmosTypeCount> openblack::audio::k_AtmosTypes = {{
    {"ATMOS_TYPE_NONE", nullptr, false},
    {"ATMOS_TYPE_SEA", "ocean.sad", false},
    {"ATMOS_TYPE_STILL_FRESH_WATER", "lake.sad", false},
    {"ATMOS_TYPE_COASTAL", "shore.sad", false},
    {"ATMOS_TYPE_JUNGLE", "jungle.sad", true},
    {"ATMOS_TYPE_ARCTIC", "arctic.sad", false},
    {"ATMOS_TYPE_DESERT", "desert.sad", true},
    {"ATMOS_TYPE_COUNTRYSIDE", "country.sad", true},
    {"ATMOS_TYPE_SWAMP", "swamp.sad", true},
    {"ATMOS_TYPE_RUNNING_WATER", "stream.sad", false},
    {"ATMOS_TYPE_STRATOSPHERE", "high.sad", false},
    {"ATMOS_TYPE_NIGHT", "night.sad", true},
    {"ATMOS_TYPE_RAIN", "rain.sad", false},
    {"ATMOS_TYPE_WIND", "wind.sad", false},
}};

int32_t openblack::audio::GetAtmosType(int32_t cellX, int32_t cellZ)
{
	if (!Locator::terrainSystem::has_value())
	{
		return 1;
	}
	const auto& island = Locator::terrainSystem::value();
	// The original compares x, z with its 512-cell grid; openblack's editor maps can be bigger
	const int32_t cells = island.GetCellsPerSide();
	if (cellX < 0 || cellX >= cells || cellZ < 0 || cellZ >= cells)
	{
		return 1;
	}
	const glm::u16vec2 cell(cellX, cellZ);
	// No land block at the cell
	if (!island.HasBlockAt(cell))
	{
		return 1;
	}
	return (island.GetCell(cell).flags >> 2) & 0xF;
}

CameraWeatherInfo openblack::audio::CameraWeather()
{
	const auto& queries = Queries();
	if (!Locator::camera::has_value() || !queries.weatherSmooth)
	{
		return {};
	}
	// As the camera's update: the smoothed weather at the camera's position (recalculated)
	return queries.weatherSmooth(Locator::camera::value().GetOrigin());
}

namespace
{
/// One atmos type's cells around the receiver
struct Entry
{
	uint16_t count {0};
	float dist2 {65535.0f}; ///< Squared distance to the nearest cell
	int16_t nearestX {0};
	int16_t nearestZ {0};
};

/// The one sound map
struct SoundMapState
{
	std::array<Entry, k_AtmosTypeCount> entries;
	uint16_t total {0}; ///< Cells of a type other than NONE
	std::array<float, k_AtmosTypeCount> volumes {};
	glm::vec3 receiver {0.0f};
	int32_t receiverX {0}; ///< MapCoords 16.16 cells
	int32_t receiverZ {0};
	float radius {50.0f};
	float heightAboveLand {0.0f};
	float receiverHeight {0.0f};
	uint32_t turn {0};
};

/// The SoundMap state (Locator::audioState)
SoundMapState& SoundMapData()
{
	return openblack::Locator::audioState::value().Get<SoundMapState>();
}

float Altitude(float x, float z)
{
	// The interpolated land height (0 off the island)
	return Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z));
}

/// Every type's count and nearest distance cleared
void Reset()
{
	auto& state = SoundMapData();
	for (auto& entry : state.entries)
	{
		entry.count = 0;
		entry.dist2 = 65535.0f;
	}
	state.total = 0;
}

/// Counts a cell of the type and keeps the nearest: distances from the receiver to the cell's centre
void AddAtmosType(int32_t type, int32_t cellX, int32_t cellZ)
{
	auto& state = SoundMapData();
	auto& entry = state.entries[(type >= 0 && type < static_cast<int32_t>(k_AtmosTypeCount)) ? type : 0];
	// The cell's MapCoords: the cell in the high words and, in the low ones, (8 >> 1) x 0x2000 = 0x8000 (the map's
	// constants, never changed), the centre of the cell
	constexpr int32_t k_HalfCell = (8 >> 1) * 0x2000;
	const int32_t centreX = cellX * map_coords::k_FixedPerCell + k_HalfCell;
	const int32_t centreZ = cellZ * map_coords::k_FixedPerCell + k_HalfCell;
	// Each MapCoords x 10 x 2^-16, one rounding of the exact product (map_coords::ToMetres); the receiver minus the
	// cell, squared and summed
	const float cellMetresX = map_coords::ToMetres(centreX);
	const float cellMetresZ = map_coords::ToMetres(centreZ);
	const float dx = map_coords::ToMetres(state.receiverX) - cellMetresX;
	const float dz = map_coords::ToMetres(state.receiverZ) - cellMetresZ;
	const float d2 = dx * dx + dz * dz;
	if (d2 < entry.dist2) // Strictly nearer
	{
		entry.dist2 = d2;
		// The cell's metres truncated, kept as 16-bit words
		entry.nearestX = static_cast<int16_t>(static_cast<int32_t>(cellMetresX));
		entry.nearestZ = static_cast<int16_t>(static_cast<int32_t>(cellMetresZ));
	}
	++entry.count;
	if (type != 0)
	{
		++state.total;
	}
}

/// The receiver (the camera), its MapCoords and height above the land, and the radius
void UpdateReceiver(const GSoundInfo& info)
{
	auto& state = SoundMapData();
	// The render camera's position
	const auto camera = Locator::camera::value().GetOrigin();
	state.receiver = camera;
	// Metres x 6553.6, truncated to MapCoords
	state.receiverX = map_coords::ToFixed(camera.x);
	state.receiverZ = map_coords::ToFixed(camera.z);
	state.receiverHeight = camera.y;
	state.heightAboveLand = camera.y - Altitude(camera.x, camera.z);
	state.radius = info.radiusForMinAtmosVolume;
}

/// The (2r/10 + 1)^2 cells around the receiver (11 x 11 with r = 50)
void UpdateFromMap()
{
	auto& state = SoundMapData();
	// Metres / 10 x 65536, truncated
	const auto r = gutils::ConvertMetersToWholeDistance(state.radius);
	Reset();
	// The MapCoords minus / plus r on both axes; the cells are their high words read signed, both ends included
	using map_coords::SignedCellOf;
	for (int32_t cx = SignedCellOf(state.receiverX - r); cx <= SignedCellOf(state.receiverX + r); ++cx)
	{
		for (int32_t cz = SignedCellOf(state.receiverZ - r); cz <= SignedCellOf(state.receiverZ + r); ++cz)
		{
			AddAtmosType(GetAtmosType(cx, cz), cx, cz);
		}
	}
}

/// 1 below normalAtmosFadeStartHeight of the camera over the land at p, 0 above the end height
float HeightFade(const GSoundInfo& info, float x, float z)
{
	const float h = SoundMapData().receiver.y - Altitude(x, z);
	if (h < info.normalAtmosFadeStartHeight)
	{
		return 1.0f;
	}
	if (h <= info.normalAtmosFadeEndHeight)
	{
		return std::max(0.0f, 1.0f - (h - info.normalAtmosFadeStartHeight) /
		                                 (info.normalAtmosFadeEndHeight - info.normalAtmosFadeStartHeight));
	}
	return 0.0f;
}

/// radiusForMax/MinAtmosVolume: 1 up to 20 from the nearest cell of the type, 0 at 50
float Radial(const GSoundInfo& info, size_t type)
{
	const float d = std::sqrt(SoundMapData().entries[type].dist2);
	if (d <= info.radiusForMaxAtmosVolume)
	{
		return 1.0f;
	}
	return std::max(0.0f,
	                1.0f - (d - info.radiusForMaxAtmosVolume) / (info.radiusForMinAtmosVolume - info.radiusForMaxAtmosVolume));
}

float NearestFade(const GSoundInfo& info, size_t type)
{
	auto& state = SoundMapData();
	return HeightFade(info, state.entries[type].nearestX, state.entries[type].nearestZ);
}

/// The 14 volumes
void CalculateVolumes(const GSoundInfo& info, float skyType, const CameraWeatherInfo& weather)
{
	auto& state = SoundMapData();
	auto& vol = state.volumes;
	// the stratosphere, from the camera's height above the land
	const float high = sound_map::StratosphereVolume(state.heightAboveLand, info.atmosphereHeight, info.atmosphereMaxVolHeight,
	                                                 info.spaceHeight);

	// The sky type: 0 day, 1 dusk, 2 night
	const float night = std::max(0.0f, skyType - 1.0f);
	float clear = (weather.snow > 0 ? 1.0f - weather.snow * 0.01f : 1.0f) - weather.rain * 0.01f;
	clear = std::max(clear, 0.0f);
	const float bad = 1.0f - clear;
	const float maxFade = info.weatherPercentageForMaxFade * 0.01f;
	const float weatherFade = bad < maxFade ? 1.0f - bad / maxFade : 0.0f;
	// The int8 wind squared and summed in integers, converted exactly, square root, minus 15, times the double 1 / 30
	// (not the float 1 / 30), stored as a float; then > 1 -> 1, < 0.01f -> 0. The game's FPU runs at 24-bit precision:
	// each step rounds to a float, the double constant itself does not
	const int windX = weather.windX;
	const int windZ = weather.windZ;
	const float speed = std::sqrt(static_cast<float>(windX * windX + windZ * windZ));
	const auto above = static_cast<float>(static_cast<double>(speed) - 15.0);
	auto wind = static_cast<float>(static_cast<double>(above) * 0.033333333333333333);
	wind = wind > 1.0f ? 1.0f : (wind < 0.01f ? 0.0f : wind);

	vol[static_cast<size_t>(AtmosType::Stratosphere)] = high;
	vol[0] = 0.0f;
	for (size_t t = 1; t <= static_cast<size_t>(AtmosType::RunningWater); ++t)
	{
		if (state.entries[t].count == 0)
		{
			vol[t] = 0.0f;
			continue;
		}
		float f = Radial(info, t);
		if (k_AtmosTypes[t].dayOnly)
		{
			f *= weatherFade * (1.0f - night);
		}
		vol[t] = f * NearestFade(info, t);
	}
	// the coast again, then the sea gives way to it (min(sea, 1 - coast))
	constexpr auto k_Sea = static_cast<size_t>(AtmosType::Sea);
	constexpr auto k_Coast = static_cast<size_t>(AtmosType::Coastal);
	float coast = 0.0f;
	if (state.entries[k_Coast].count != 0)
	{
		coast = Radial(info, k_Coast);
		vol[k_Coast] = coast * NearestFade(info, k_Coast);
	}
	if (state.entries[k_Sea].count != 0)
	{
		const float f = std::min(Radial(info, k_Sea), 1.0f - coast);
		vol[k_Sea] = f * NearestFade(info, k_Sea);
	}
	else
	{
		vol[k_Sea] = 0.0f;
	}
	// The sea's share of the typed cells
	const float seaFraction = state.total != 0 ? static_cast<float>(state.entries[k_Sea].count) / state.total : 0.0f;
	vol[static_cast<size_t>(AtmosType::Night)] =
	    HeightFade(info, state.receiver.x, state.receiver.z) * (1.0f - seaFraction) * weatherFade * night;
	const float maxRain = info.weatherPercentageForMaxWeatherVolume;
	vol[static_cast<size_t>(AtmosType::Rain)] = weather.rain < maxRain ? weather.rain / maxRain : 1.0f;
	vol[static_cast<size_t>(AtmosType::Wind)] = wind;
}

const char* SurfaceName(int32_t surface)
{
	// The surface type names
	static constexpr std::array<const char*, 9> k_Names = {
	    "SOUND SURFACE TYPE NONE",       "SOUND SURFACE TYPE GRASS",         "SOUND SURFACE TYPE GRAVEL",
	    "SOUND SURFACE TYPE HARD",       "SOUND SURFACE TYPE MUD",           "SOUND SURFACE TYPE SNOW",
	    "SOUND SURFACE TYPE DEEP WATER", "SOUND SURFACE TYPE SHALLOW WATER", "SOUND SURFACE TYPE LOOSE FOLIAGE"};
	return surface >= 0 && surface < static_cast<int32_t>(k_Names.size()) ? k_Names[surface] : "?";
}

/// The original's debug message lines
void Dump()
{
	auto& state = SoundMapData();
	std::array<char, 256> line {};
	// X, Z = the high words of the receiver's MapCoords (its cell)
	std::snprintf(line.data(), line.size(), "Sound Map Calc Update X=%d Z=%d %s Count=%d",
	              static_cast<int>(map_coords::CellOf(state.receiverX)), static_cast<int>(map_coords::CellOf(state.receiverZ)),
	              SurfaceName(SurfaceType(state.receiver)), state.total);
	SPDLOG_LOGGER_INFO(spdlog::get("audio"), "{}", line.data());
	// The hand's MapCoords (GameQueries::handPosition, taken as the player's left hand position)
	if (const auto& handPosition = Queries().handPosition; handPosition)
	{
		if (const auto position = handPosition(); position)
		{
			std::snprintf(line.data(), line.size(), "Sound Map At Hand X=%d Z=%d %s Count=%d",
			              static_cast<int>(map_coords::CellOf(map_coords::ToFixed(position->x))),
			              static_cast<int>(map_coords::CellOf(map_coords::ToFixed(position->z))),
			              SurfaceName(SurfaceType(*position)), state.total);
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "{}", line.data());
		}
	}
	std::snprintf(line.data(), line.size(), "Sound Radius=%3.3f Distance=%3.3f DistanceAboveLand=%3.3f",
	              static_cast<double>(state.radius), static_cast<double>(state.receiverHeight),
	              static_cast<double>(state.heightAboveLand));
	SPDLOG_LOGGER_INFO(spdlog::get("audio"), "{}", line.data());
}

uint32_t TracePeriod()
{
	static const uint32_t k_Period = [] {
		const char* value = std::getenv("OPENBLACK_ATMOS_TRACE");
		if (value == nullptr)
		{
			return 0u;
		}
		const int n = std::atoi(value);
		return n > 0 ? static_cast<uint32_t>(n) : 1u;
	}();
	return k_Period;
}
} // namespace

void sound_map::Update(float skyType)
{
	auto& state = SoundMapData();
	++state.turn;
	if (!Locator::terrainSystem::has_value() || !Locator::camera::has_value() || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto& info = Locator::infoConstants::value().sound;
	UpdateReceiver(info);
	UpdateFromMap();
	CalculateVolumes(info, skyType, CameraWeather());
	if (TraceThisTurn())
	{
		Dump();
		// (openblack) the volumes themselves, which the original only shows as the banks' targets
		std::string volumes;
		for (size_t t = 1; t < k_AtmosTypeCount; ++t)
		{
			volumes += fmt::format(" {}={:.3f}({})", k_AtmosTypes[t].name + 11, state.volumes[t], state.entries[t].count);
		}
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "(openblack) Sound map volumes (count):{}", volumes);
		// (openblack) the nearest cell of each type present: the distance and the point of the cell's centre, what Radial
		// and NearestFade read
		std::string nearest;
		for (size_t t = 1; t < k_AtmosTypeCount; ++t)
		{
			if (state.entries[t].count != 0)
			{
				nearest += fmt::format(" {}={:.3f}@({},{})", k_AtmosTypes[t].name + 11, std::sqrt(state.entries[t].dist2),
				                       state.entries[t].nearestX, state.entries[t].nearestZ);
			}
		}
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "(openblack) Sound map nearest:{}", nearest);
	}
}

const std::array<float, k_AtmosTypeCount>& sound_map::GetVolumes()
{
	return SoundMapData().volumes;
}

float sound_map::GetReceiverX()
{
	return SoundMapData().receiver.x;
}

bool sound_map::TraceThisTurn()
{
	const auto period = TracePeriod();
	return period != 0 && SoundMapData().turn % period == 0;
}

float openblack::audio::sound_map::StratosphereVolume(float height, float atmosphere, float maxVolume, float space)
{
	if (height < atmosphere)
	{
		return 0.0f;
	}
	if (height < maxVolume)
	{
		return std::min(1.0f, (height - atmosphere) / (maxVolume - atmosphere));
	}
	if (height < space)
	{
		return std::max(0.0f, 1.0f - (height - maxVolume) / (space - maxVolume));
	}
	return 0.0f;
}
