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
#include <string>
#include <string_view>
#include <vector>

/// The settings of this computer, which all the players' profiles share: settings.json, next to the profile folder.
///
/// The file is a JSON object. Each value is optional: a missing value, or a value of the wrong type or out of its range,
/// gives openblack's default. Other members of the object are kept when the game writes the file again.
///
/// - "detail_level": the graphics detail level, 0 to 6
/// - "max_detail_level": how many detail levels the options' selector steps through, 1 to 7 (5 by default)
/// - "effects_volume", "music_volume": the volumes of the options, 0 to 1
/// - "width", "height": the window's size in pixels, 1 to 65535
/// - "window_mode": "windowed", "fullscreen" or "borderless"
/// - "vsync": true to wait for the screen's vertical sync
/// - "custom_detail": an object with the values of the custom detail level (5): "model_detail" (more than 0, at most
///   1), "small_bump", "clouds", "cloud_shadows", "water_tiling" (0 to 1), "land_reflection", "temple_reflections",
///   "temple_lightmaps", "temple_glows", "temple_volume_light", "temple_people", "rain_splashes" (0 to 255), "fog",
///   "weather", "object_lighting", "object_shadows", "low_res_textures", "coarse_land", "light_boost" (at most 1.53; a larger
///   value would divide the light by zero)
namespace openblack::computer_settings
{

/// The name of the file
inline constexpr std::string_view k_FileName = "settings.json";

/// The values of the custom detail level. A missing value is the value of the level's own table.
struct CustomDetail
{
	std::optional<float> modelDetail;
	std::optional<bool> smallBump;
	std::optional<bool> clouds;
	std::optional<bool> cloudShadows;
	std::optional<float> waterTiling;
	std::optional<bool> landReflection;
	std::optional<bool> templeReflections;
	std::optional<bool> templeLightmaps;
	std::optional<bool> templeGlows;
	std::optional<bool> templeVolumeLight;
	std::optional<bool> templePeople;
	std::optional<uint8_t> rainSplashes;
	std::optional<bool> fog;
	std::optional<bool> weather;
	std::optional<bool> objectLighting;
	std::optional<bool> objectShadows;
	std::optional<bool> lowResTextures;
	std::optional<bool> coarseLand;
	/// Brightens the light of the sky and the land; 0 for every level of the table
	std::optional<float> lightBoost;

	bool operator==(const CustomDetail&) const = default;
};

/// The values of the file
struct Settings
{
	std::optional<uint8_t> detailLevel;
	std::optional<uint8_t> maxDetailLevel;
	std::optional<float> effectsVolume;
	std::optional<float> musicVolume;
	std::optional<uint16_t> width;
	std::optional<uint16_t> height;
	/// "windowed", "fullscreen" or "borderless"
	std::optional<std::string> windowMode;
	std::optional<bool> vsync;
	CustomDetail customDetail;

	bool operator==(const Settings&) const = default;
};

/// The values of a file, and what was wrong in it
struct ParseResult
{
	Settings settings;
	/// One line for each value that was refused, or for a file that is not a JSON object
	std::vector<std::string> problems;
};

/// The values of the text of a file. Text that is not a JSON object gives no values and one problem.
[[nodiscard]] ParseResult Parse(std::string_view text);

/// The text of the file with the values that changed holds, each over the one of the old text. Every other member of
/// the old text stays. Old text that is not a JSON object is replaced.
[[nodiscard]] std::string Update(std::string_view oldText, const Settings& changed);

/// The values that the game starts with
struct StartValues
{
	uint16_t width {1280};
	uint16_t height {1024};
	std::string windowMode {"windowed"};
	bool vsync {false};
	uint8_t detailLevel {4};
	uint8_t maxDetailLevel {5};

	bool operator==(const StartValues&) const = default;
};

/// Which start values the command line gave
struct GivenOnCommandLine
{
	bool width {false};
	bool height {false};
	bool windowMode {false};
	bool vsync {false};
	bool detailLevel {false};
};

/// The values the game starts with: those that the command line gave, then the file's, then the command line's
/// defaults
[[nodiscard]] StartValues Resolve(const StartValues& commandLine, const GivenOnCommandLine& given, const Settings& file);

} // namespace openblack::computer_settings
