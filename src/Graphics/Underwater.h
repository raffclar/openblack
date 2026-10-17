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
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

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

/// The world's scale for the water: the height of an adult male villager at full size, in units (the bounds of his
/// model in its rest pose), and the height of an adult man in metres. Every size of the water comes from metres
/// through k_UnitsPerMetre. The shaders have the same value as UW_UNITS_PER_METRE in underwater.sh.
inline constexpr float k_VillagerHeight = 1.661f;
inline constexpr float k_VillagerMetres = 1.75f;
inline constexpr float k_UnitsPerMetre = k_VillagerHeight / k_VillagerMetres;
/// A length in metres, in units
[[nodiscard]] constexpr float Metres(float metres)
{
	return metres * k_UnitsPerMetre;
}
/// The acceleration of gravity, in units a second squared
inline constexpr float k_Gravity = 9.81f * k_UnitsPerMetre;

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
/// Under this sum of the two lights' strengths, the direction of the light moves towards straight up
inline constexpr float k_UpBlend = 0.1f;
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
	/// The spray at the cliffs, all of it
	Spray,
	/// The spray's fine mist and the haze at the foot of the cliffs
	SprayMist,
	/// The wet rock of the cliffs after a wave
	WetRock,
	/// The bubbles and the cloud of air under the water at the cliffs
	Bubbles,
	/// The waterspouts in a storm
	Waterspouts,

	/// The cube of the far scene round the eye, which the water reflects where the screen has no reflection (the sky
	/// alone when it is off)
	Environment,
	/// The waves and the glint as each pixel shows them: the waves that a pixel cannot show make the surface rougher
	/// (when it is off, the short waves fade out and the far sea is flat)
	Filtering,
	/// The long waves of the open sea, which show out to the horizon
	FarWaves,
	/// The field of small waves that the things in the water write into: their wakes and rings
	WakeField,
	/// The splashes where things fall into the water
	Splashes,
	/// The hulls that push the water down, with no water inside a boat
	HullMask,
	/// The rogue waves in a strong storm, with their foam and spray
	RogueWaves,
	/// The lightning on the sea: its flash in the water, its glint, and the ring and the steam where it hits the sea
	LightningOnWater,
	/// The foam's thickness: the foam that builds up where the waves break and decays, its light and its bubbles
	FoamVolume,
	/// The silt in the water, which gives the water its colour and takes the view away sooner
	Sediment,
	/// Not an effect: the water is drawn as a flat colour where it shows, to compare it with the original sea
	WaterMask,
	/// The last grade of the colour (exposure, saturation and contrast) that keeps the original game's colours
	Grading,
	/// The water in the rivers: its flow, its foam, its sound and the leaves that drift down it (see
	/// underwater::streams). When it is off, the rivers show only their beds.
	Streams,
	/// The splash where the hand taps the water (see underwater::tap). When it is off, the tap makes the original ring.
	TapSplash,
	/// The volumetric clouds in place of the original game's clouds (the original clouds show when it is off)
	Clouds,
	/// The shadows of the sun and the moon: the hills' shadows over the land and the water, and the things' shadows
	Shadows,
	/// The volumetric fog: the haze, the ground fog at dawn, the mist and the shafts of light
	Fog,
	/// The haze of the air's light over the far land and the sea, and the air's sky near the horizon
	Haze,
	/// The light that the sky and the lit land give to the shade (the grid of light probes and the sky's light)
	GlobalIllumination,
	/// The shade where the things round a point and its own folds hide the sky, from the scene's depth
	AmbientOcclusion,
	/// The pockets of fog over the swamps and the still water
	FogPockets,
	/// The jellyfish that drift up through the deep water and glow (see underwater::jellyfish)
	Jellyfish,
	_Count
};

/// How much of the experiment's water a machine draws. The lower tiers drop the costliest parts that have a cheaper
/// stand-in (the sky for the far cube, flat foam for the foam's thickness, the original haze for the fog, the
/// original clouds and shadows for the new ones), so no part of the scene goes away.
enum class Quality : uint8_t
{
	High,
	Medium,
	Low,
	_Count
};
/// Whether a tier draws an effect
[[nodiscard]] bool InQuality(Effect effect, Quality quality);
/// The next tier, High after Low
[[nodiscard]] Quality NextQuality(Quality quality);

/// How much of the haze or of the light in the shade a tier draws. No tier turns them off: each lower tier keeps a
/// cheaper stand-in.
enum class LightDetail : uint8_t
{
	/// The haze's grid with the hills' and the clouds' shadows; the light probes and the shade from the scene's depth
	Full,
	/// The haze's grid without the shadows, in fewer steps; the light probes without the shade from the scene's depth
	Reduced,
	/// The horizon's colour over the distance for the haze; the sky's light and the hills' shade for the light
	Basic,
};
/// The haze's and the light's detail for a tier
[[nodiscard]] LightDetail HazeDetail(Quality quality);
[[nodiscard]] LightDetail LightingDetail(Quality quality);

/// The last grade of the colour: x the exposure, y the saturation, z the contrast about middle grey. It gives a bright,
/// saturated look with soft contrast that keeps the original game's textures readable.
inline constexpr glm::vec3 k_Grade {1.06f, 1.12f, 1.04f};

/// How a body of water looks: what it takes out of the light for each unit (red, green, blue), the colour that it
/// scatters back, how much it reflects against the sea and how smooth it is (GGX roughness, used when it is calm),
/// and how much of the sea's waves it has
struct WaterProfile
{
	glm::vec3 absorption;
	glm::vec3 scatter;
	float reflection;
	float roughness;
	float waveScale;
};
/// The open sea: clear blue-green water (the post pass's own constants and lit colour)
inline constexpr WaterProfile k_SeaProfile {.absorption = {0.085f, 0.032f, 0.018f},
                                            .scatter = {1.0f, 1.0f, 1.0f},
                                            .reflection = 1.0f,
                                            .roughness = 0.06f,
                                            .waveScale = 1.0f};
/// A still pond: murky green-teal water that takes the red first and holds what falls in it, almost smooth (its
/// reflections are soft), no waves
inline constexpr WaterProfile k_PondProfile {.absorption = {0.55f, 0.35f, 0.42f},
                                             .scatter = {0.035f, 0.085f, 0.075f},
                                             .reflection = 1.0f,
                                             .roughness = 0.07f,
                                             .waveScale = 0.0f};
/// A profile as the shaders take it: [0] xyz the absorption, w the reflection; [1] xyz the scatter, w the roughness
[[nodiscard]] std::array<glm::vec4, 2> ProfileUniform(const WaterProfile& profile);

/// The tides: the sea is at its usual height at a full moon's high tide, and lower at every other time. The high tides
/// away from a full moon are lower by up to this much.
inline constexpr float k_NeapHighDrop = 0.6f;
/// The lowest low tide, at a full or a new moon, under the usual height
inline constexpr float k_LowestTide = 2.5f;
/// Whether the moon moves the tides. Off (the user's decision): the sea stays at the original game's water height, the
/// full moon's high tide, all the time. The tide rules stay for when it is on.
inline constexpr bool k_MoonTides = false;
/// How far under the usual height the sea goes at its lowest
inline constexpr float k_LowestSea = k_MoonTides ? k_LowestTide : 0.0f;
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
	/// The phase of the shore wave, in radians. It grows each frame by the wave's angular speed, so a change of the
	/// speed with the weather never makes the waves jump
	float shorePhase {0.0f};
	/// The height of the open sea's waves that come in to the coast (the sum of their heights), in units
	float swellHeight {0.0f};
};

/// The shore wave's angular speed for a stormy share of 0 to 1: longer waves come in a storm
[[nodiscard]] float ShoreOmega(float storm);
/// The shore wave's phase after a time step: it grows at the wave's angular speed, kept in a range of 64 waves, so the
/// number of each wave stays the same for a long time
[[nodiscard]] float AdvanceShorePhase(float phase, float storm, float seconds);
/// The wave number of the shore wave, in radians a unit, in deep water (its length does not change with the weather)
inline constexpr float k_ShoreWaveNumber = 2.0f * 3.14159265f / (100.0f * k_UnitsPerMetre);

/// How much a wave of wave number `k` (radians a unit, in deep water) grows or shrinks as it comes into water of this
/// depth (units): the shoaling of linear waves, which keep their energy flux (Green's law in shallow water). It is 1
/// in deep water, a little under 1 between, and grows in shallow water. The shaders have the same in UwShoaling.
[[nodiscard]] float Shoaling(float k, float depth);
/// The highest a wave's crest can be over its mean level in water of this depth before it breaks (units): a wave
/// breaks when it is higher than 0.78 of the depth from crest to trough
[[nodiscard]] constexpr float BreakingCrest(float depth)
{
	return 0.39f * (depth > 0.0f ? depth : 0.0f);
}

/// The sea's state for a stormy share of 0 to 1 and the wind's direction
[[nodiscard]] WaveState WavesFor(float storm, float windAngle);

/// The waves' height over the sea's level at a point and time, and the slope of the surface there (its rise along x
/// and z). The shaders use the same sum of waves.
struct WaveSample
{
	float height;
	glm::vec2 slope;
	/// The crest envelope of the wind sea: the sum of each wave's steepness on its crest and for a short time after it
	/// (k_WhitecapSeconds). WhitecapShare makes it the white horses.
	float whitecap {0.0f};
};
/// How long the foam of a white horse stays after its crest, in seconds
inline constexpr float k_WhitecapSeconds = 1.5f;
/// How much of the water is a white horse, 0 to 1, for a crest envelope (WaveSample::whitecap) and a stormy share of 0
/// to 1. UwWhitecap in underwater.sh is the same.
[[nodiscard]] float WhitecapShare(float envelope, float storm);
/// The share of the open sea that white horses cover on average, for a stormy share of 0 to 1: few on a calm day, many
/// in a wind, about a fifth in the worst storm. UwWhitecapCover in underwater.sh is the same.
[[nodiscard]] float WhitecapCover(float storm);
[[nodiscard]] WaveSample WavesAt(glm::vec2 point, float seconds, const WaveState& waves);
/// The same in water of a depth (units) at a distance from the coast (units, out to sea positive): each wave grows as
/// the water gets shallow (Shoaling), all of them break together where they are too high for the depth
/// (BreakingCrest), and far out the waves vary more (OpenSeaSize). UwWavesFiltered in underwater.sh is the same.
[[nodiscard]] WaveSample WavesInDepth(glm::vec2 point, float seconds, const WaveState& waves, float shoreDistance, float depth);
/// How big the waves are at a point against the usual, from the distance to the coast (units): 0.85 near the coast,
/// and 0.9 to 2.2 far out, in wide patches that drift slowly
[[nodiscard]] float OpenSeaSize(glm::vec2 point, float seconds, float shoreDistance);

/// The waves of the sea: a swell from far away and a wind sea of many waves with lengths over a wide range, their
/// directions spread about the wind's, their heights from a spectrum and their speeds from the deep water's
/// dispersion. The shaders take the same table as a uniform array (u_uwWaveTable).
inline constexpr size_t k_WaveCount = 14;
/// Where the swell comes from, in radians about the up axis
inline constexpr float k_SwellAngle = 2.4f;
/// The wind sea's steepness (height for each length): a low base, and a peak at the middle lengths with its width in
/// natural logs of the length
inline constexpr float k_WindSeaBase = 0.005f;
inline constexpr float k_WindSeaPeak = 0.04f;
inline constexpr float k_WindSeaPeakMetres = 10.0f;
inline constexpr float k_WindSeaPeakWidth = 0.75f;
/// How sharp the crests are on a calm day (1 for a sine wave): sharper crests and broader troughs
inline constexpr float k_CrestSharpness = 1.25f;
/// How far the warp moves the wind sea's pattern at most (see WaveVariationAt), in metres
inline constexpr float k_WarpMetres = 6.0f;
/// The range of the wind sea's groups: the waves are 1 - k_GroupRange / 2 to 1 + k_GroupRange / 2 of their mean
inline constexpr float k_GroupRange = 1.2f;
/// The steepest that one wave of the table can be (its height times its wave number), on a calm day and in the worst
/// storm. Real waves break at about 0.44.
inline constexpr float k_SteepestWaveCalm = 0.12f;
inline constexpr float k_SteepestWaveStorm = 0.16f;
/// The chop of the wind: short waves with pointed crests over the long waves. The height of its first octave on a
/// calm day and its length, in metres; the octaves in the height of the surface and in its normal
inline constexpr float k_ChopMetres = 0.25f;
inline constexpr float k_ChopLengthMetres = 30.0f;
inline constexpr int k_ChopGeometryOctaves = 3;
inline constexpr int k_ChopShadingOctaves = 5;
/// The height of the chop over its mean at a point, from `octaves` octaves. Each octave is a grid of crests that noise
/// bends, with a round trough and a pointed crest, in two copies that move in opposite directions. The next octave
/// turns, is 3.8 times shorter, 0.22 times as high and less pointed. UwChop in underwater.sh is the same.
[[nodiscard]] float ChopAt(glm::vec2 point, float seconds, const WaveState& waves, int octaves);
/// A wave's height after a soft cap at a limit: about the height while it is low, never more than the limit, and with
/// no sharp change. `rate` is the change of the result for each change of the height.
[[nodiscard]] float CapSteepness(float height, float limit, float& rate);
/// For each wave, x: its direction (radians), y: its length, z: its height, w: its phase
[[nodiscard]] std::array<glm::vec4, k_WaveCount> WaveTable(const WaveState& waves);

/// How high the waves are at a point and time against their mean, about 0.4 to 1.6: the sets, a group of bigger waves
/// every few waves, and calmer patches that drift over the sea. x for the wind sea, y for the swell.
[[nodiscard]] glm::vec2 WaveSets(glm::vec2 point, float seconds, const WaveState& waves);

/// How the wind sea varies over the sea, so that its crests bend and its waves are not all the same size. The same on
/// the processor and in the shaders (UwWaveVariation).
struct WaveVariation
{
	/// How far the wind sea's pattern moves at the point, in units
	glm::vec2 warp;
	/// The change of the warp along x (xy: of warp.x and warp.y) and along z (zw)
	glm::vec4 warpSlope;
	/// How big the wind sea's waves are against their mean, about 0.4 to 1.6: x for the even waves, y for the odd
	glm::vec2 groups;
	/// The change of each group along x (xy: of groups.x and groups.y) and along z (zw)
	glm::vec4 groupSlope;
};
[[nodiscard]] WaveVariation WaveVariationAt(glm::vec2 point, float seconds);

/// How strongly the wind sea's mix of lengths changes over the sea (see WaveTiltAt)
inline constexpr float k_TiltStrength = 0.7f;
/// The length of the wind sea's waves that the tilt does not change, in metres
inline constexpr float k_TiltMetres = 10.0f;
/// The tilt of the wind sea's lengths at a point, -1 to 1, and its change along x and z: wide patches, 190 m and 260 m
/// across, where the long waves of the wind sea are higher and the short ones lower (above 0), or the other way
/// (under 0). Seen from high up, some patches show big rounded waves and others fine chop. The same in the shaders
/// (UwWaveTilt).
struct WaveTilt
{
	float tilt;
	glm::vec2 slope;
};
[[nodiscard]] WaveTilt WaveTiltAt(glm::vec2 point, float seconds);
/// How a wind wave of this length is scaled at a tilt: e^(tilt k_TiltStrength ln(length / k_TiltMetres)). `rate` is the
/// change of the scale for each unit of tilt.
[[nodiscard]] float TiltScale(float tilt, float length, float& rate);

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

/// The side of the spray's noise volume, in texels
inline constexpr uint16_t k_SprayNoiseSide = 32;
/// The spray's noise volume: a cube of k_SprayNoiseSide texels a side that tiles in all three directions, 4 bytes a
/// texel, x first, then y, then z. r: smooth noise in four octaves, g: round cells (1 at the middle of a cell, 0 at its
/// edge) for lumps of water, b: finer smooth noise, a: ridges of the smooth noise for thin strands. The spray reads it
/// as if it were a cloud of water, moving through it over time.
[[nodiscard]] std::vector<uint8_t> SprayNoiseVolume();
/// The value noise of the shaders (UwNoise in underwater.sh), 0 to 1, on the processor
[[nodiscard]] float Noise(glm::vec2 point);

/// The hash of the shaders (UwHash in underwater.sh), 0 to 1
[[nodiscard]] float Hash(glm::vec2 point);

/// A texel of the shore map from a corner's shore and how much it is at the foot of a cliff: the direction to the coast
/// is shortened by up to a half at the foot of a cliff
[[nodiscard]] glm::vec4 ShoreTexel(glm::vec4 shore, float cliff, float pond = 0.0f);
/// How much a point is in a still pond, 0 to 1, from its shore value: the direction to the coast is made up to half
/// longer in a pond (UwShorePond in underwater.sh)
[[nodiscard]] float ShorePond(glm::vec4 shore);

/// The shore map's value at a point across the ground, blended between the four corners round it (as the shaders read
/// the shore map): x the distance to the coast, yz the shortened direction to the coast, w the ground's height. The
/// first corner is at the origin.
[[nodiscard]] glm::vec4 ShoreAt(std::span<const glm::vec4> shore, std::span<const float> cliff, glm::ivec2 size, float spacing,
                                glm::vec2 point);

/// How much a point is at the foot of a cliff, 0 to 1, from its shore value (UwShoreCliff in underwater.sh)
[[nodiscard]] float ShoreCliff(glm::vec4 shore);

/// One crash of a wave at a point along the foot of a cliff
struct CliffCrash
{
	/// Which crash it is: it goes up by one with each wave
	float number;
	/// The time since this crash started, in seconds
	float since;
	/// How hard the wave hits, as the spray's power: 0 for no crash, about 0.25 for a usual crash of a calm sea
	float power;
	/// A number for this crash at this point, to pick a different sound each time
	float seed;
};

/// The crash at a foot of a cliff now. It is the same timing and strength as UwCliffCrash in underwater.sh and the
/// spray's power in vs_underwater_spray.sc: change them together. `cliff` is ShoreCliff at the foot.
[[nodiscard]] CliffCrash CliffCrashAt(glm::vec2 foot, const WaveState& waves, float seaLevel, float cliff);

/// Whether a crash starts between two times of the same foot
[[nodiscard]] bool CrashStarts(const CliffCrash& before, const CliffCrash& now);

/// The feet of the cliffs to hear a crash from: at most `count`, the nearest to the eye inside `range` first, each at
/// least `spacing` from the others, so that one crash along a cliff is heard once
[[nodiscard]] std::vector<size_t> HeardCliffFeet(std::span<const glm::vec4> feet, glm::vec2 eye, size_t count, float spacing,
                                                 float range);

/// A waterspout over the open sea in a storm: where its foot is, and how strong it is now, 0 to 1 (it grows in and dies
/// away)
struct Waterspout
{
	glm::vec2 position {0.0f};
	float strength {0.0f};
};
/// The waterspouts there can be at once
inline constexpr size_t k_MostWaterspouts = 2;
/// How long a waterspout lives, in seconds
inline constexpr float k_WaterspoutSeconds = 70.0f;
/// How far out to sea a waterspout stays, from the nearest coast
inline constexpr float k_WaterspoutSeaRoom = 150.0f;
/// The waterspouts at a time: none in a calm sea, at most k_MostWaterspouts in a strong storm, each at its own place
/// between 500 and 1300 units from `centre`, and only where `seaDistance` (the distance from the coast, positive at sea)
/// gives at least k_WaterspoutSeaRoom. Each one drifts slowly while it lives.
[[nodiscard]] std::vector<Waterspout> WaterspoutsAt(float seconds, float storm, glm::vec2 centre,
                                                    const std::function<float(glm::vec2)>& seaDistance);

/// A rogue wave in a strong storm: one wave much higher than the storm's waves, that rolls across the open sea as a
/// local event. It has its own crest line, a steep face, a deep trough in front and a smaller wave behind. It grows,
/// travels with the speed of its length and dies away. It dies before it comes near the coast.
struct RogueWave
{
	/// The middle of its crest line now, across the ground
	glm::vec2 position {0.0f};
	/// The direction it travels in, a unit vector
	glm::vec2 direction {1.0f, 0.0f};
	/// The height of its crest over the sea's mean level now, with its growth and its death
	float crest {0.0f};
	/// Its length from crest to crest, in units
	float length {100.0f};
	/// Half the length of its crest line, in units
	float halfWidth {120.0f};
	/// How far the ends of the crest line stay behind its middle, as a share of the length
	float bend {0.15f};
	/// How much its crest breaks into white water, 0 to 1
	float breaking {0.0f};
	/// How strong it is now, 0 to 1: its growth, its death, the storm and the room it has from the coast
	float strength {0.0f};
	/// The time since it started, in seconds
	float age {0.0f};
	/// A number for this wave, for the noise of its foam and spray
	float seed {0.0f};
};
/// How long a rogue wave lives, and the time from the start of one chance of a rogue wave to the next, in seconds
inline constexpr float k_RogueSeconds = 30.0f;
inline constexpr float k_RoguePeriod = 45.0f;
/// The rogue wave fades out between these distances from the coast, so that it never comes to the land
inline constexpr float k_RogueShoreNear = 60.0f;
inline constexpr float k_RogueShoreFar = 200.0f;
/// How far out to sea the path of a rogue wave stays, from the nearest coast
inline constexpr float k_RogueSeaRoom = 160.0f;

/// The height of the storm's waves: the significant height of the wave table (4 times the deviation of the surface)
[[nodiscard]] float SignificantWaveHeight(const WaveState& waves);

/// The rogue wave at a time, or none. In each period a rogue wave comes with a chance that grows with the storm (none
/// in a calm sea). Its place is 120 to 450 units from `centre`, or farther when the sea there has no room. It travels
/// about the wind's direction, and it is 2 to 3 times the storm's waves from its crest to its trough. Its whole path
/// must be at least k_RogueSeaRoom out to sea by `seaDistance` (the distance from the coast, positive at sea), and it
/// fades as it comes near the coast. With `firstCertain`, the first period always has a wave (for the debug storm).
[[nodiscard]] std::optional<RogueWave> RogueWaveAt(float seconds, const WaveState& waves, glm::vec2 centre,
                                                   const std::function<float(glm::vec2)>& seaDistance,
                                                   bool firstCertain = false);

/// The rogue wave's part of the sea's height at a point, and its slope. `shoreDistance` and `depth` are the distance
/// from the coast and the water's depth there: the wave fades out near the coast and in shallow water. The same as
/// UwRogueWave in underwater.sh: change them together.
[[nodiscard]] WaveSample RogueWaveHeight(glm::vec2 point, const RogueWave& rogue, float shoreDistance, float depth);

/// The rogue wave as the shaders take it (u_uwRogue): x, y the middle of its crest, z its direction (radians), w its
/// crest; then its length, half the length of its crest line, its breaking, and its bend. All zero for no wave.
[[nodiscard]] std::array<glm::vec4, 2> RogueUniform(const std::optional<RogueWave>& rogue);

/// The waves' height and slope at a point with a rogue wave added (see RogueWaveHeight)
[[nodiscard]] WaveSample WavesAt(glm::vec2 point, float seconds, const WaveState& waves, const std::optional<RogueWave>& rogue,
                                 float shoreDistance, float depth);

/// A strike of lightning as the sea sees it: where it struck, how long ago, and whether it hit the sea
struct SeaStrike
{
	/// Where it struck, across the ground
	glm::vec2 position {0.0f};
	/// The time since it struck, in seconds; large when there was no strike
	float since {1000.0f};
	/// How strong the strike was: the strength of its flash (a bolt is weaker than a flash with thunder)
	float strength {0.0f};
	/// Whether it hit the sea, far enough from the coast
	bool overSea {false};
	/// The age of the flash that the last update saw, to find a new strike
	float flashTime {1000.0f};
};
/// How long the flash ring and the steam of a strike on the sea last, in seconds
inline constexpr float k_StrikeSeconds = 4.0f;
/// A strike hits the sea where it is at least this far out from the coast
inline constexpr float k_StrikeSeaRoom = 8.0f;

/// A flash of lightning for the sea: where it struck, its age and strength, and whether it is on
struct StrikeFlash
{
	glm::vec2 position {0.0f};
	float time {0.0f};
	float strength {0.0f};
	bool active {false};
};

/// The strike after `seconds` more, from the storm's last flash (none when no storm flashes). A new flash (a younger
/// one, or one in another place) starts a new strike; `seaDistance` is the distance from the coast at its point.
[[nodiscard]] SeaStrike TrackStrike(const SeaStrike& before, const std::optional<StrikeFlash>& flash, float seconds,
                                    float seaDistance);

/// The radius of the bright ring that a strike on the sea sends out, at a time since it struck
[[nodiscard]] float StrikeRingRadius(float since);

/// The far scene round the eye, in the six faces of a cube that the water reflects: one face is drawn again each
/// frame. The faces are in the order of a cube texture: +x, -x, +y, -y, +z, -z.
inline constexpr uint16_t k_EnvironmentSize = 512;
inline constexpr uint8_t k_EnvironmentFaces = 6;
/// The face under the eye shows only the sea bed, which the water never reflects, so it is not drawn
inline constexpr uint8_t k_EnvironmentFaceDown = 3;

/// The face of the cube to draw after `face`. It skips the face that looks down.
[[nodiscard]] constexpr uint8_t NextEnvironmentFace(uint8_t face)
{
	auto next = static_cast<uint8_t>((face + 1) % k_EnvironmentFaces);
	return next == k_EnvironmentFaceDown ? static_cast<uint8_t>(next + 1) : next;
}

/// The light and the place of the eye in one frame, to find a jump between two frames
struct FrameLight
{
	glm::vec3 towardsLight;
	glm::vec3 skyColour;
	glm::vec3 eye;
};

/// The light can turn by this angle in one frame at most, in radians, before it is a jump of the time of day
inline constexpr float k_LightJumpRadians = 0.035f;
/// The sky's colour can change by this much in one frame at most, in each channel from 0 to 1
inline constexpr float k_SkyJumpChange = 0.06f;
/// The eye can move by this distance in one frame at most, in world units
inline constexpr float k_EyeJumpUnits = 300.0f;

/// Whether the time of day or the eye jumped between two frames. The effects that keep the frames before (the cube of
/// the far scene, the clouds' history, the light probes, the shadows) must then start again.
[[nodiscard]] bool LightJumped(const FrameLight& before, const FrameLight& now);

/// The direction a face of the cube looks along, and its up direction, as a cube texture keeps them
struct EnvironmentFaceAxes
{
	glm::vec3 forward;
	glm::vec3 up;
};
[[nodiscard]] EnvironmentFaceAxes EnvironmentFaceAxesOf(uint8_t face);

/// Where the cube is for an eye: on the sea's level under the eye, so the water near the eye reflects from its
/// middle
[[nodiscard]] glm::vec3 EnvironmentCentre(glm::vec3 eye, float seaLevel);

/// The long waves of the open sea: longer than the wind sea's waves and lower for their length, so that they show as
/// far as the horizon. A storm makes them higher. x: the direction (radians), y: the length, z: the height, w: the
/// phase, as in WaveTable. The shaders take the same table (u_uwFarWaves).
inline constexpr size_t k_FarWaveCount = 3;
[[nodiscard]] std::array<glm::vec4, k_FarWaveCount> FarWaveTable(const WaveState& waves);

/// How much of a wave a pixel shows, from the wave's length and the size of the pixel on the water along the wave's
/// direction. The rest of the wave's slope makes the surface rougher.
struct WaveFilter
{
	/// The share of the wave's height that the surface keeps, 0 to 1
	float height;
	/// The share of the wave's slope that the normal keeps, 0 to 1
	float slope;
	/// The variance of the slope that the pixel cannot show, which adds to the surface's roughness (alpha squared)
	float variance;
};
[[nodiscard]] WaveFilter FilterWave(float length, float amplitude, float footprint);

/// The roughness (GGX alpha) of the water at a pixel, from the variance of the slopes that the pixel cannot show
[[nodiscard]] float FilteredRoughness(float variance);

} // namespace openblack::graphics::underwater
