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

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec3.hpp>

#include "ECS/Components/Weather.h"
#include "ECS/ScriptWeatherRules.h"

/// The rules of the real-world weather. The weather of the player's place changes the background weather of a
/// single-player land, which is the weather of each place before the storms add to it. A preset of the weather type can
/// also make a storm over the middle of the island.
namespace openblack::ecs::real_weather
{

/// The size of one preset in Scripts/weatherinfo.lhw
constexpr size_t k_PresetBytes = 112;
/// The type that means "no weather"
constexpr int32_t k_NoType = -1;
/// The most that a preset gives to a value of the background weather, and to the number of clouds of its storm
constexpr int32_t k_MostAmount = 100;
/// The most shade of the clouds of a storm
constexpr float k_MostCloudShade = 1.0f;
/// The radii of the storm of a preset
constexpr float k_StormInnerRadius = 1000.0f;
constexpr float k_StormOuterRadius = 2000.0f;
/// The storm of a preset comes in over one second and lives for 800000 seconds, which is longer than a game
constexpr float k_StormFadeSeconds = 1.0f;
constexpr float k_StormLastsFor = 800000.0f;

/// What a preset does to one value
enum class Operation : uint8_t
{
	/// Each byte other than 1 and 2 sets the value
	Set = 0,
	Add = 1,
	Subtract = 2,
};

struct Change
{
	uint8_t operation {0};
	float amount {0.0f};

	/// The value after the change
	[[nodiscard]] float Apply(float value) const;
};

/// One preset of the file. A preset changes the background weather, and can make a storm.
struct Preset
{
	uint16_t type {0};
	bool makesStorm {false};

	// The background weather
	Change rain;
	Change snowCover;
	Change snowfall;
	Change windX;
	Change windZ;
	Change overcast;

	// The storm
	Change rainSpeed;
	/// The least and most seconds between the flashes with thunder, then between the bolts
	Change thunderLeast;
	Change thunderMost;
	Change boltLeast;
	Change boltMost;
	Change cloudShade;
	Change cloudCount;
	Change cloudHeight;
};

/// The presets of Scripts/weatherinfo.lhw, in the order of the file. Bytes after the last whole preset are ignored.
[[nodiscard]] std::vector<Preset> ParsePresets(std::span<const uint8_t> bytes);

/// The preset of a type. The game puts each preset that it reads before the presets read earlier, so the last preset of
/// the file with the type is the one used. None for k_NoType or a type with no preset.
[[nodiscard]] const Preset* FindPreset(std::span<const Preset> presets, int32_t type);

/// The storm of a preset before the preset changes it: the defaults of a storm, over a point, with the radii, the
/// fading time and the life of a preset's storm
[[nodiscard]] script_weather::LandStorm PresetStorm(glm::vec3 centre);

/// What a preset did
struct Applied
{
	components::WeatherInfo background;
	/// The storm to make, when the preset makes one. Its weather is the new background weather.
	std::optional<script_weather::LandStorm> storm;
};

/// The real weather and its preset change the background weather of the land:
/// - The temperature becomes the real temperature, and the overcast becomes the real visibility, each as one byte.
/// - The preset then changes the overcast, the snow cover, the snowfall, the rain and the winds, in this order. Each is
///   at most 100. With no preset nothing more changes, and there is no storm.
///
/// The game keeps the result of each change in one number and starts the next change from that number, not from the
/// value of the weather. So a change that adds or subtracts starts from the result of the change before it. The storm
/// of a preset puts its number of clouds in that number first.
[[nodiscard]] Applied ApplyPreset(const Preset* preset, components::WeatherInfo background, int32_t temperature,
                                  int32_t visibility, script_weather::LandStorm storm);

/// The picture of the message about the weather, 0 to 8, by the weather type
[[nodiscard]] uint8_t PictureOf(int32_t type);

/// The place in the message about the weather: "City (Country)", or "City (Area,Country)" when there is an area. An
/// area that is empty or "(null)" counts as no area.
[[nodiscard]] std::string PlaceText(std::string_view city, std::string_view area, std::string_view country);

} // namespace openblack::ecs::real_weather
