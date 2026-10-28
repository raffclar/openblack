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

/// The longest line that the game reads from Scripts/country.lhw at one time. The rest of a longer line is read as the
/// next line.
constexpr size_t k_CountryLineBytes = 2047;
/// The text that the message shows for a country code that is not in the list
constexpr std::string_view k_NoCountryName = "(null)";

/// One line of Scripts/country.lhw: "code=Name"
struct Country
{
	/// The text that a code of the weather must be equal to
	std::string code;
	std::string name;
};

/// The countries of Scripts/country.lhw, in the order of the file. The game reads the file as text: it ignores a
/// carriage return before a line feed, and a byte 26 stops the file. Each line gives one country, also an empty line:
/// - The code is the text before the first "=" or the end of the line, and the name is the text after the "=".
/// - The game keeps the code and the name in one buffer, with the name three bytes after the start. Thus a code of three
///   or more letters keeps only its first three letters, followed by the name.
[[nodiscard]] std::vector<Country> ParseCountries(std::string_view text);

/// The name of a country by its code, as the message shows it. The code must be equal to the code of the line, letter
/// for letter. When two lines have the same code, the game uses the last line. A code that is not in the list gives
/// k_NoCountryName.
[[nodiscard]] std::string_view CountryName(std::span<const Country> countries, std::string_view code);

/// The size of the header and of one day in the file weather.dat
constexpr size_t k_DaysHeaderBytes = 8;
constexpr size_t k_DayBytes = 0x144;
/// The space for the country, the area and the city in one day of weather.dat, with the zero at the end
constexpr size_t k_DayCountryBytes = 3;
constexpr size_t k_DayPlaceBytes = 150;

/// One day of weather that the game got, as the file weather.dat keeps it
struct KeptDay
{
	std::string_view country;
	std::string_view area;
	std::string_view city;
	int32_t day {0};
	int32_t month {0};
	int32_t year {0};
	int32_t temperature {0};
	int32_t visibility {0};
	int32_t type {k_NoType};
	/// The three numbers of a day that the game keeps but does not use
	int32_t unusedFirst {0};
	int32_t unusedSecond {0};
	int32_t unusedThird {0};
};

/// The bytes of weather.dat, which the game writes in the player's folder each time it gets the weather: "LHWD", the
/// number of days, then each day. A day keeps the country, the area and the city as text with a zero at the end, then
/// the numbers in the width that the game keeps them in. This function cuts a text that is too long for its space.
[[nodiscard]] std::vector<uint8_t> DaysFile(std::span<const KeptDay> days);

} // namespace openblack::ecs::real_weather
