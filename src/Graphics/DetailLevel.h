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

#include <algorithm>
#include <array>

/// Black & White's graphics detail levels, 0 to 6. The game's options name the first five Minimum, Low, Medium, High and
/// Maximum Detail; 5 is Custom Detail, whose values here are where the player's own settings start, and 6 "SUPER
/// Detail!", which only differs from Maximum in the sea's texture. Maximum (4) is the default, which is what the game
/// chose on any machine fast enough.
///
/// The detail level is read once when the game starts: a change in the options takes effect the next time it starts.
namespace openblack::graphics::detail_level
{

inline constexpr uint8_t k_Default = 4;
inline constexpr uint8_t k_Maximum = 4;
inline constexpr uint8_t k_Custom = 5;
inline constexpr uint8_t k_Count = 7;

/// Everything a detail level switches
struct Settings
{
	/// How far out models keep their finer meshes (see mesh_detail::Reach): the lowest levels swap them sooner
	float modelDetail {0.5f};
	/// The fine bump texture tiled over the land near the camera
	bool smallBump {true};
	/// The clouds in the sky
	bool clouds {true};
	/// The clouds' shadows on the land
	bool cloudShadows {true};
	/// How finely the sea's texture repeats, from 0 to 1 (see sea_rows::Period). At 0 the sea is a still square.
	float waterTiling {0.8f};
	/// The land mirrored in the sea
	bool landReflection {true};
	/// The temple's floor reflections
	bool templeReflections {true};
	/// The temple's light shafts
	bool templeVolumeLight {true};
	/// The temple's glows
	bool templeGlows {true};
	/// The temple's walking worshippers
	bool templePeople {true};
	/// The temple rooms' light maps
	bool templeLightmaps {true};
	/// Splashes rain makes on the ground each frame
	uint8_t rainSplashes {8};
	/// The haze that fades distant land, models and sprites into the sky's colour, and the overcast's effect on the sky
	bool fog {true};
	/// The weather: rain, snow and storms, and the sky's response to them
	bool weather {true};
	/// The sun lights models (without it they take the land's light evenly); the creatures and the hand are always lit
	bool objectLighting {true};
	/// The creatures' and the hand's moving shadows fall on models too, not just on the land
	bool objectShadows {true};
	/// Textures are loaded at a lower resolution
	bool lowResTextures {false};
	/// The whole land is drawn with its coarse texture pages, without painting each block's texture
	bool coarseLand {false};
};

// The game's tables, one value per level
inline constexpr std::array<float, k_Count> k_ModelDetail = {0.2f, 0.3f, 0.4f, 0.5f, 0.5f, 0.5f, 0.5f};
inline constexpr std::array<float, k_Count> k_WaterTiling = {0.0f, 0.2f, 0.4f, 0.6f, 0.8f, 0.5f, 1.0f};
inline constexpr std::array<uint8_t, k_Count> k_RainSplashes = {0, 0, 3, 5, 8, 8, 8};

/// The settings of a level; any level above the highest is the highest
[[nodiscard]] constexpr Settings For(uint8_t level)
{
	const auto i = std::min<uint8_t>(level, k_Count - 1);
	return {
	    .modelDetail = k_ModelDetail.at(i),
	    .smallBump = true,
	    .clouds = i >= 3,
	    .cloudShadows = i >= 3,
	    .waterTiling = k_WaterTiling.at(i),
	    .landReflection = i >= 3,
	    .templeReflections = i >= 4,
	    .templeVolumeLight = i >= 3,
	    .templeGlows = i >= 2,
	    .templePeople = i >= 2,
	    .templeLightmaps = i >= 1,
	    .rainSplashes = k_RainSplashes.at(i),
	    .fog = i >= 3,
	    .weather = i >= 3,
	    .objectLighting = i >= 2,
	    .objectShadows = i >= 3,
	    .lowResTextures = i <= 1,
	    .coarseLand = i == 0,
	};
}

/// The options' detail selector steps round the first five levels; a level beyond them shows as the one it wraps to
[[nodiscard]] constexpr uint8_t MenuLevel(uint8_t level)
{
	return level % (k_Maximum + 1);
}

/// Whether the temple's main room is mirrored in its floor: that needs its light maps too
[[nodiscard]] constexpr bool TempleReflected(const Settings& settings)
{
	return settings.templeReflections && settings.templeLightmaps;
}

/// Whether the temple's spot light cones and window light are drawn
[[nodiscard]] constexpr bool TempleLightShafts(const Settings& settings)
{
	return settings.templeGlows && settings.templeVolumeLight;
}

/// Whether the temple's glows are drawn: in the room itself only with its light shafts, while the floor's reflection
/// shows them whenever there are glows
[[nodiscard]] constexpr bool TempleGlowsShown(const Settings& settings, bool inReflection)
{
	return settings.templeGlows && (inReflection || settings.templeVolumeLight);
}

[[nodiscard]] constexpr bool Fog(uint8_t level)
{
	return For(level).fog;
}

[[nodiscard]] constexpr bool Weather(uint8_t level)
{
	return For(level).weather;
}

[[nodiscard]] constexpr bool Clouds(uint8_t level)
{
	return For(level).clouds;
}

[[nodiscard]] constexpr bool CloudShadows(uint8_t level)
{
	return For(level).cloudShadows;
}

[[nodiscard]] constexpr float ModelDetail(uint8_t level)
{
	return For(level).modelDetail;
}

[[nodiscard]] constexpr float WaterTiling(uint8_t level)
{
	return For(level).waterTiling;
}

} // namespace openblack::graphics::detail_level
