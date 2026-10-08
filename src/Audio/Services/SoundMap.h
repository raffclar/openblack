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

#include <array>

#include <glm/vec3.hpp>

#include "Audio/GameQueries.h"

// The sound map (the atmos zones around the camera) and the atmos types. The sound map's surface type is
// ecs::sea_cells::GetSurfaceType (the game calls it; src/Audio asks GameQueries::surfaceType, audio::SurfaceType). The
// weather comes from GameQueries::weatherSmooth.

namespace openblack::audio
{

/// ATMOS_TYPE: the ambient zones (14 x {name, bank, day only})
enum class AtmosType : uint8_t
{
	None,
	Sea,
	StillFreshWater,
	Coastal,
	Jungle,
	Arctic,
	Desert,
	Countryside,
	Swamp,
	RunningWater,
	Stratosphere,
	Night,
	Rain,
	Wind,

	_Count
};
inline constexpr size_t k_AtmosTypeCount = static_cast<size_t>(AtmosType::_Count);

struct AtmosTypeInfo
{
	const char* name; ///< "ATMOS_TYPE_SEA" (the Dump's names)
	const char* bank; ///< the .sad of Audio\SFX\Atmos as openblack names its sound group ("ocean.sad"), nullptr for NONE
	bool dayOnly;     ///< Faded by the weather and silenced at night (jungle, desert, countryside, swamp, night)
};
/// The 14 atmos types
extern const std::array<AtmosTypeInfo, k_AtmosTypeCount> k_AtmosTypes;

/// Bits 2..5 of the cell's flags byte (LNDCell::flags); 1 (SEA) outside the 512 x 512 cells or where no land block is.
[[nodiscard]] int32_t GetAtmosType(int32_t cellX, int32_t cellZ);

/// The camera's weather (CameraWeatherInfo, GameQueries.h): GameQueries::weatherSmooth (recalculated) at the render
/// camera's position, as the camera's update asks it (byte 3 = overcast, then wind x / z).
[[nodiscard]] CameraWeatherInfo CameraWeather();

namespace sound_map
{

/// The receiver and radius, the cells around it and the volumes, then the Dump lines when OPENBLACK_ATMOS_TRACE is set.
/// Once per game turn, at its end. `skyType` = the sky type 0..2 (sky_type::Frame(), audio::ProcessTurn).
void Update(float skyType);

/// The 14 atmos volumes 0..1 of the last Update (the audio system copies them as the banks' targets)
[[nodiscard]] const std::array<float, k_AtmosTypeCount>& GetVolumes();
/// The receiver's (camera's) x of the last Update. The original copies 15 floats from the volumes, so this one lands in
/// the banks' current[0] (NONE, no bank): only the atmos trace shows it.
[[nodiscard]] float GetReceiverX();

/// OPENBLACK_ATMOS_TRACE=<n>: the original's Dump lines every n turns (1 = every turn, like its debug messages)
[[nodiscard]] bool TraceThisTurn();

/// The sound map's stratosphere volume from the camera's height above the land: 0 below `atmosphere`, rising to 1 at
/// `maxVolume`, falling back to 0 at `space`, 0 above it
[[nodiscard]] float StratosphereVolume(float height, float atmosphere, float maxVolume, float space);

} // namespace sound_map

} // namespace openblack::audio
