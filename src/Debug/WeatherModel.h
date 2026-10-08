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
#include <string_view>

#include <glm/ext/vector_int2_sized.hpp>
#include <glm/vec2.hpp>

#include "ECS/Weather/Storms.h"

// What the Weather window works out: its presets, the storm it forces over the whole island from its settings, and how
// far through its life a storm is. Free of the game's state, so it is tested with plain values.

namespace openblack::debug::weather_window
{

/// The island the atmosphere covers is 128 cells of 40 m on a side; a storm centred on it with this inner radius covers
/// it to the corners at full strength
constexpr glm::vec2 k_IslandCentre {2560.0f, 2560.0f};
constexpr float k_IslandInnerRadius = 3700.0f;
constexpr float k_IslandOuterRadius = 3800.0f;

/// How often a lightning storm flashes, in seconds: with thunder (sheet lightning), then with a bolt (fork lightning)
constexpr glm::vec2 k_ThunderWait {3.0f, 15.0f};
constexpr glm::vec2 k_BoltWait {2.0f, 8.0f};

/// The storm the window forces, as its controls set it
struct StormSettings
{
	int8_t rain {80};
	int8_t overcast {90};
	int8_t temperature {12};
	/// The direction the wind blows from, in degrees, and its strength
	float windDegrees {0.0f};
	int windStrength {0};
	bool lightning {false};
	/// Seconds between two sheet lightnings (with thunder) and two fork lightnings (bolts); zero when there is none
	glm::vec2 thunderWait {0.0f};
	glm::vec2 boltWait {0.0f};
	float seconds {600.0f};
	float fadeSeconds {5.0f};
	/// The clouds' height above the land and the rain's fall speed (the storm descriptor's defaults)
	float cloudHeight {160.0f};
	float rainSpeed {1.0f};
};

struct Preset
{
	std::string_view name;
	std::string_view tooltip;
	std::array<float, 4> colour;
	int8_t rain;
	int8_t overcast;
	int8_t temperature;
	int windStrength;
	bool lightning;
};

constexpr std::array<Preset, 4> k_Presets {{
    {"Clear", "Every storm clears: calm air over the whole island", {0.95f, 0.75f, 0.25f, 1.0f}, 0, 0, 20, 0, false},
    {"Drizzle", "A light rain under grey skies", {0.45f, 0.65f, 0.80f, 1.0f}, 30, 60, 15, 5, false},
    {"Rain", "A steady downpour", {0.25f, 0.45f, 0.85f, 1.0f}, 80, 90, 12, 15, false},
    {"Thunderstorm", "Heavy rain, wind, thunder and bolts of lightning", {0.55f, 0.35f, 0.85f, 1.0f}, 100, 100, 20, 30, true},
}};

/// The wind bytes (x, z) of a wind from `degrees` at `strength`
[[nodiscard]] glm::i8vec2 WindBytes(float degrees, int strength);

/// The settings take the preset's weather, wind strength and lightning, keeping their own direction and length
void ApplyPreset(const Preset& preset, StormSettings& settings);

/// A preset with neither rain nor cloud only clears the storms, it forces none
[[nodiscard]] constexpr bool ForcesStorm(const Preset& preset)
{
	return preset.rain != 0 || preset.overcast != 0;
}

/// Lightning on gives the default intervals if none are set yet; off sets them to none
void SetLightning(StormSettings& settings, bool on);

/// The storm descriptor of the settings: a still storm over the whole island, without clouds of its own
[[nodiscard]] weather::storms::StormDescriptor IslandStorm(const StormSettings& settings);

enum class StormPhase : uint8_t
{
	Live,     ///< Coming in or at full strength
	Clearing, ///< In its last fading time
	Ending,   ///< Marked for deletion, gone in a turn or two
};
[[nodiscard]] StormPhase PhaseOf(const weather::storms::Storm& storm);

/// The storm has sheet or fork lightning
[[nodiscard]] bool HasLightning(const weather::storms::StormDescriptor& descriptor);

/// A weather byte of 0 to 127 (the snow lying, the flash of 0 to 255) as a percentage
[[nodiscard]] int LyingPercent(int8_t snowCover);
[[nodiscard]] int FlashPercent(uint8_t flash);

} // namespace openblack::debug::weather_window
