/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <string_view>

#include <entt/core/hashed_string.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

/// EXPERIMENT (not for merge): the rules of the underwater view. The original game has no view under the sea.
namespace openblack::graphics::underwater
{

/// The sand texture of the sea bed: a square raw file of 3 bytes a texel, made by tools/experiment/make_underwater_sand.py
inline constexpr std::string_view k_SandPath = "E:/openblack/experiments/underwater/sand.raw";
inline constexpr entt::hashed_string k_SandTextureId = entt::hashed_string("experiment/underwater_sand");
/// The coral atlas, 3 by 3 cells of 256 texels, with its alpha beside it: made by tools/experiment/make_underwater_coral.py
inline constexpr std::string_view k_CoralPath = "E:/openblack/experiments/underwater/coral.raw";
inline constexpr std::string_view k_CoralAlphaPath = "E:/openblack/experiments/underwater/corala.raw";
inline constexpr uint16_t k_CoralSide = 768;
inline constexpr entt::hashed_string k_CoralTextureId = entt::hashed_string("experiment/underwater_coral");
/// The most coral and plants on the reef
inline constexpr size_t k_MostCoral = 7000;
inline constexpr size_t k_MostPlants = 5000;

/// Where the debug key puts the camera: under the sea by the shore of the first land, looking along the sea bed
inline constexpr glm::vec3 k_ShoreEye {1470.0f, -4.0f, 2000.0f};
inline constexpr glm::vec3 k_ShoreFocus {1460.0f, -12.0f, 2030.0f};

/// A pose of the camera on the debug tour: where it is and what it looks at
struct TourPose
{
	glm::vec3 eye;
	glm::vec3 focus;
};
/// How long the debug tour takes, in seconds
inline constexpr float k_TourSeconds = 26.0f;
/// The debug tour by the shore of the first land at a time from its start: down from above the sea, through the
/// surface, along the sea bed, deep down looking up, and back up to the surface
[[nodiscard]] TourPose TourAt(float seconds);
/// How long the far tour takes, in seconds
inline constexpr float k_FarTourSeconds = 32.0f;
/// The far tour along the sea bed, out from the shore of the first land towards the map's west edge: where it is
/// across the ground at a time from its start, and the way it looks. The camera keeps above the sea bed by itself.
[[nodiscard]] glm::vec2 FarTourGround(float seconds);
/// The debug tours
enum class Tour : uint8_t
{
	Shore,
	FarSeabed
};

/// The height of the sea's surface
inline constexpr float k_SeaLevel = 0.0f;
/// The underwater look is on while the eye is lower than this. Between sea level and this height each pixel finds for
/// itself whether it is under the surface.
inline constexpr float k_ActiveBelow = 1.5f;
/// How much the light bends as it goes into the water
inline constexpr float k_RefractiveIndex = 1.333f;
/// How much of each colour the water takes out for each unit the light goes through it: the red goes first, then the
/// green, and the blue goes last
inline constexpr glm::vec3 k_Absorption {0.085f, 0.032f, 0.018f};
/// How bright the full moon is under the water, against the sun at full strength
inline constexpr float k_MoonLight = 0.45f;
/// The steepest that the light under the water can be from straight down (the critical angle), as its upward part
inline constexpr float k_LowestRefractedUp = 0.66f;

/// A light in the sky, as it shines on the sea's surface
struct SkyLight
{
	/// The direction from the surface towards the light
	glm::vec3 towards;
	/// Its colour, 0 to 1
	glm::vec3 colour;
	/// How strongly it shows, 0 to 1
	float strength;
};

/// The light under the water this frame: the sun's and the moon's together
struct WaterLight
{
	/// The direction towards the light under the surface, bent as it goes into the water; always up
	glm::vec3 towards {0.0f, 1.0f, 0.0f};
	/// The direction towards the light above the surface, for its glints on the waves
	glm::vec3 aboveTowards {0.0f, 1.0f, 0.0f};
	/// Its colour and strength together
	glm::vec3 colour {0.0f};
	/// How much of the light is the moon's, 0 to 1
	float moonShare {0.0f};
};

/// How much of the moon's face is lit, 0 at a new moon and 1 at a full moon. The phase is 0 to 2 pi, with a full moon at
/// half a turn.
[[nodiscard]] float PhaseBrightness(float phase);

/// The direction towards a light under the water, after the light bends at the flat surface
[[nodiscard]] glm::vec3 RefractIntoWater(glm::vec3 towards);

/// The sun's light and the moon's light, blended by their strengths. Through dusk and dawn both shine, and the direction
/// and the colour go from one to the other.
[[nodiscard]] WaterLight BlendLights(const std::optional<SkyLight>& sun, const std::optional<SkyLight>& moon);

/// The part of each colour that is left after the light goes this far through the water
[[nodiscard]] glm::vec3 Transmittance(float distance);

/// The ambient light at a depth under the surface: less and more blue the deeper it is
[[nodiscard]] glm::vec3 AmbientAtDepth(const glm::vec3& surfaceAmbient, float depth);

/// The colour of the water far away, which the light that the water scatters gives it: the sky's colour and the light
/// from the sun or the moon, made blue by the water
[[nodiscard]] glm::vec3 ScatterColour(const glm::vec3& skyColour, const WaterLight& light);

/// Whether the eye is under the water, or so near the surface that the surface can cut the view
[[nodiscard]] constexpr bool IsActive(float eyeHeight, float seaLevel = k_SeaLevel)
{
	return eyeHeight < seaLevel + k_ActiveBelow;
}

/// The effects of the experiment that can be turned off one at a time, to measure what each costs
enum class Effect : uint8_t
{
	None,
	Reflections,
	Foam,
	Refraction,
	Shafts,
	Motes,
	Caustics,
	Waves,
	Whitecaps,
	Reef,
	Current,
	/// Not an effect: the water is drawn as a flat colour where it shows, to compare it with the original sea
	WaterMask,
	_Count
};

/// The tides: the sea is at its usual height at a full moon's high tide, and lower at every other time. The high tides
/// away from a full moon are lower by up to this much.
inline constexpr float k_NeapHighDrop = 0.6f;
/// The lowest low tide, at a full or a new moon, under the usual height
inline constexpr float k_LowestTide = 2.5f;
/// The low tide halfway between a full and a new moon is this share of the lowest
inline constexpr float k_NeapLowShare = 0.6f;

/// The height of the sea at an hour of script time (0 to 24) and a phase of the moon (0 to 2 pi, a full moon at half a
/// turn). Two high tides each day, when the moon is highest (midnight) and lowest (midday), and two low tides between.
/// It is never higher than the usual height.
[[nodiscard]] float TideLevel(float scriptHour, float moonPhase);

/// How the sea moves this frame, from the weather
struct WaveState
{
	/// How high the waves are, 1 for a calm sea
	float scale {1.0f};
	/// How stormy the sea is, 0 calm to 1 the worst storm
	float storm {0.0f};
	/// The direction the wind blows towards, in radians about the up axis from +x towards +z
	float windAngle {0.0f};
	/// How far the waves run up the beach and back, in height
	float swash {0.25f};
};

/// The sea's state for a stormy share of 0 to 1 and the wind's direction
[[nodiscard]] WaveState WavesFor(float storm, float windAngle);

/// The waves' height over the sea's level at a point and time, and the slope of the surface there (its rise along x
/// and z). The shaders use the same sum of waves.
struct WaveSample
{
	float height;
	glm::vec2 slope;
};
[[nodiscard]] WaveSample WavesAt(glm::vec2 point, float seconds, const WaveState& waves);

/// The waves of the sea: a swell from far away and a wind sea of many waves with lengths over a wide range, their
/// directions spread about the wind's, their heights from a spectrum and their speeds from the deep water's
/// dispersion. The shaders take the same table as a uniform array (u_uwWaveTable).
inline constexpr size_t k_WaveCount = 14;
/// Where the swell comes from, in radians about the up axis
inline constexpr float k_SwellAngle = 2.4f;
/// For each wave, x: its direction (radians), y: its length, z: its height, w: its phase
[[nodiscard]] std::array<glm::vec4, k_WaveCount> WaveTable(const WaveState& waves);

/// How high the waves are at a point and time against their mean, about 0.4 to 1.6: the sets, a group of bigger waves
/// every few waves, and calmer patches that drift over the sea. x for the wind sea, y for the swell.
[[nodiscard]] glm::vec2 WaveSets(glm::vec2 point, float seconds, const WaveState& waves);

/// The sea's currents this frame: one field that every moving thing in the water follows, the same on the processor and
/// in the shaders (UwCurrent)
struct CurrentState
{
	/// The steady flow, in units a second: the tide's flow in or out and the drift the wind gives the water
	glm::vec2 flow {0.0f};
	/// How strong the swirls are, 1 for a calm sea
	float swirl {1.0f};
	/// How strong the gusts that cross the sea are, in units a second
	float gust {0.4f};
};

/// The currents for the speed the tide rises at (units a second, falling below 0), the wind's direction and the storm
[[nodiscard]] CurrentState CurrentFor(float tideRate, float windAngle, float storm);

/// The water's flow across the ground at a point and time, in units a second: the steady flow, slow swirls that have
/// no sinks or sources, gusts that pass along the wind, and near the surface the waves' own motion
[[nodiscard]] glm::vec2 CurrentAt(glm::vec3 point, float seconds, const CurrentState& current, const WaveState& waves,
                                  float seaLevel);

/// How stormy the weather at the camera is, 0 to 1: from its rain, its cloud and its wind, each in percent
[[nodiscard]] float Storminess(float rain, float overcast, glm::vec2 wind);

/// A value eased towards a target by at most `rate` a second
[[nodiscard]] float EaseTowards(float value, float target, float rate, float seconds);

/// How far under the surface the eye is; 0 above it
[[nodiscard]] float EyeDepth(float eyeHeight, float seaLevel = k_SeaLevel);

} // namespace openblack::graphics::underwater
