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
#include <span>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "CloudLightning.h"

/// EXPERIMENT (not for merge): the rules of the volumetric clouds. The original game has billboards for its clouds.
/// The shaders march rays through a layer of cloud over the land. The density in that layer comes from noise volumes,
/// a weather map and a profile of the height for each type of cloud. These functions make the noise once, ease the
/// weather over time and keep the CPU twins of the shaders' formulas, so that the tests can check them.
namespace openblack::graphics::volumetric_clouds
{

/// The side of the base noise volume, in texels: the large shapes of the clouds
inline constexpr uint32_t k_BaseNoiseSide = 128;
/// The side of the detail noise volume, in texels: the small shapes that wear away the edges
inline constexpr uint32_t k_DetailNoiseSide = 32;
/// The side of the weather noise, in texels: where the clouds are and of what type
inline constexpr uint32_t k_WeatherNoiseSide = 256;
/// The side of the blue noise, in texels, that moves the start of each ray
inline constexpr uint32_t k_BlueNoiseSide = 64;

/// The world units that one tile of each noise covers
inline constexpr float k_BaseNoiseTile = 1500.0f;
inline constexpr float k_DetailNoiseTile = 210.0f;
inline constexpr float k_WeatherTile = 14000.0f;
/// The world units that one tile of the wide weather covers. The shader reads the weather noise again at this size, on
/// turned axes, so that the weather tile does not repeat when the eye is high.
inline constexpr float k_WideWeatherTile = 47000.0f;

/// The bottom of the cloud layer, in world units over the sea. The original game puts its clouds 300 to 500 high.
inline constexpr float k_CloudBase = 300.0f;
/// The top of the layer for each type of cloud, over its bottom
inline constexpr float k_StratusDepth = 160.0f;
inline constexpr float k_CumulusDepth = 520.0f;
inline constexpr float k_CumulonimbusDepth = 1500.0f;
/// The radius of the curve of the layer. The sea is flat to the horizon, so the curve is very slight: the layer goes
/// down a little with distance, and the far clouds stay over the sea.
inline constexpr float k_PlanetRadius = 3000000.0f;
/// The farthest that a ray goes through the clouds
inline constexpr float k_FarthestCloud = 18000.0f;

/// The most storms that the clouds show at a time
inline constexpr size_t k_MaxStorms = 8;

/// The side of the map of the clouds' shadow, in texels, and the world units that it covers round the eye
inline constexpr uint32_t k_ShadowTexels = 256;
inline constexpr float k_ShadowWindow = 6144.0f;
/// Over this height of the eye over the sea, the shadow's map covers twice the units for each doubling of the height
inline constexpr float k_ShadowWindowHeight = 1500.0f;
/// For each unit of the eye's height over the clouds, the farthest cloud is this many units farther away
inline constexpr float k_FarthestPerHeight = 3.0f;
/// The farthest cloud for an eye high over the land
inline constexpr float k_FarthestCloudCap = 200000.0f;

/// The type of cloud: 0 stratus, 0.5 cumulus, 1 cumulonimbus
inline constexpr float k_Stratus = 0.0f;
inline constexpr float k_Cumulus = 0.5f;
inline constexpr float k_Cumulonimbus = 1.0f;

/// The cover of the sky when the weather has no cloud: scattered cumulus, as the original game always shows some
/// clouds
inline constexpr float k_FairCover = 0.38f;

/// The time constants of the weather's ease, in seconds. Two stages ease in turn, so that a change starts slowly,
/// continues and stops slowly. A storm builds sooner than it calms down.
inline constexpr float k_BuildUpSeconds = 12.0f;
inline constexpr float k_CalmDownSeconds = 22.0f;
/// The wind's speed eases with this time constant, in seconds
inline constexpr float k_WindSeconds = 8.0f;
/// How fast the clouds change shape, in units of the base noise's vertical axis a second, and how much faster a storm
/// boils
inline constexpr float k_EvolutionRate = 2.5f;
inline constexpr float k_StormBoil = 3.0f;
/// The wind of a calm day, in world units a second: the original game's clouds move 70 a second along their track
inline constexpr float k_CalmWind = 70.0f;

// Noise. Each noise tiles: its last texel meets its first.

/// A hash of a lattice point and a seed, 0 to 1
[[nodiscard]] float Hash(glm::ivec3 cell, uint32_t seed);
/// Gradient noise at a point, with the lattice repeated every `period` cells: about -1 to 1
[[nodiscard]] float Perlin(glm::vec3 point, int32_t period, uint32_t seed);
/// Cellular noise at a point with one feature point in each of `cells` cells along each axis, repeated over 0 to 1:
/// 1 at a feature point, 0 at the distance of a cell or more
[[nodiscard]] float Worley(glm::vec3 point, int32_t cells, uint32_t seed);
/// Three octaves of cellular noise from `cells`, weighted 0.625, 0.25 and 0.125
[[nodiscard]] float WorleyOctaves(glm::vec3 point, int32_t cells, uint32_t seed);
/// A value moved from one range into another, as the clouds' formulas use it
[[nodiscard]] float Remap(float value, float fromLow, float fromHigh, float toLow, float toHigh);

/// The base noise volume, RGBA8, `side` texels a side, x fastest: r Perlin-Worley noise, g b a cellular noise at
/// three rising frequencies. The work is divided between threads.
[[nodiscard]] std::vector<uint8_t> BaseNoiseVolume(uint32_t side = k_BaseNoiseSide);
/// The detail noise volume, RGBA8: r g b cellular noise at three rising frequencies, a 255
[[nodiscard]] std::vector<uint8_t> DetailNoiseVolume(uint32_t side = k_DetailNoiseSide);
/// The weather noise, RGBA8: r where cloud can be, g the variation of the type, b the variation of the cover for the
/// slow change of the weather, a the small variation of the cloud's bottom
[[nodiscard]] std::vector<uint8_t> WeatherNoise(uint32_t side = k_WeatherNoiseSide);
/// Blue noise, R8, `side` texels a side: each value from 0 to 255 shows the same number of times, and the values near
/// each other differ much (void and cluster). The rays start a fraction of a step on by it.
[[nodiscard]] std::vector<uint8_t> BlueNoise(uint32_t side = k_BlueNoiseSide);

// The shaders' formulas (clouds.sh), for the tests

/// The density of a type of cloud at a height in the layer, 0 at its bottom to 1 at the top of the type, 0 to 1
[[nodiscard]] float HeightProfile(float type, float heightFraction);
/// The top of the layer, over its bottom, for a type of cloud
[[nodiscard]] float LayerDepth(float type);
/// The Henyey-Greenstein phase function: how much light scatters by the angle with a cosine `cosAngle`, for an
/// asymmetry `g`. It integrates to 1 over the sphere.
[[nodiscard]] float HenyeyGreenstein(float cosAngle, float g);
/// Two lobes: a strong one forward (the silver lining towards the sun) and a weak one back
[[nodiscard]] float DualLobePhase(float cosAngle);
/// The light that goes through an optical depth towards the light (Beer-Lambert). The powder term darkens the thin edges
/// of a cloud, where `localDepth` is small, when the eye sees the side that faces the light (`cosAngle` near -1).
[[nodiscard]] float BeerPowder(float opticalDepth, float localDepth, float cosAngle);
/// The light that reaches a point through an optical depth, with the light that scatters many times: octaves of less
/// extinction, less energy and a wider phase
[[nodiscard]] float MultipleScattering(float opticalDepth, float cosAngle);

// The weather of the clouds

/// A storm as the clouds see it
struct StormInput
{
	/// The storm's own number, which stays the same while it lives
	uint32_t serial {0};
	glm::vec2 centre {0.0f};
	float innerRadius {0.0f};
	float outerRadius {0.0f};
	/// How strong it is now, 0 to 1
	float strength {0.0f};
	/// Its rain, 0 to 1
	float rain {0.0f};
};

/// A storm's cell of cloud: a storm, eased in and out. It stays after its storm goes, until it calms down.
struct StormCell
{
	uint32_t serial {0};
	glm::vec2 centre {0.0f};
	float innerRadius {0.0f};
	float outerRadius {0.0f};
	/// The two stages of the ease of its strength and its rain
	glm::vec2 strength {0.0f};
	glm::vec2 rain {0.0f};
	/// Whether its storm is still there
	bool alive {true};
};

/// What the weather asks of the clouds this frame
struct CloudWeather
{
	/// How much of the sky has cloud, 0 to 1
	float cover {k_FairCover};
	/// The type of cloud (see k_Stratus)
	float type {k_Cumulus};
	/// How much rain falls from the clouds, 0 to 1
	float rain {0.0f};
	/// The wind at the clouds, in world units a second
	glm::vec2 wind {0.0f};
};

/// The weather of the clouds over time. It eases each change over a build-up or a calm-down time, so that nothing
/// jumps. It adds the wind's travel to a drift each frame and changes the shapes of the clouds slowly.
class CloudClimate
{
public:
	/// One frame of the given seconds towards the weather that is asked for and the storms there are now
	void Step(float seconds, const CloudWeather& target, std::span<const StormInput> storms);
	/// Puts the weather at once, with no ease: for a new land
	void Reset(const CloudWeather& weather);

	/// The cover, the type and the rain as eased now
	[[nodiscard]] CloudWeather Current() const;
	/// How far the wind has moved the clouds, in world units
	[[nodiscard]] glm::vec2 Drift() const { return _drift; }
	/// How far the shapes of the clouds have changed, in units of the base noise's vertical axis
	[[nodiscard]] float Evolution() const { return _evolution; }
	/// The storms' cells as eased now, at most k_MaxStorms
	[[nodiscard]] std::span<const StormCell> Storms() const { return _storms; }
	/// The strength and the rain of a cell as eased now
	[[nodiscard]] static float Strength(const StormCell& cell) { return cell.strength.y; }
	[[nodiscard]] static float Rain(const StormCell& cell) { return cell.rain.y; }

private:
	glm::vec2 _cover {k_FairCover};
	glm::vec2 _type {k_Cumulus};
	glm::vec2 _rain {0.0f};
	glm::vec2 _wind {0.0f};
	glm::vec2 _drift {0.0f};
	float _evolution {0.0f};
	std::vector<StormCell> _storms;
};

/// One step of a two-stage ease towards a target, with a time constant for the rise and one for the fall
[[nodiscard]] glm::vec2 EaseTwoStage(glm::vec2 stages, float target, float seconds, float riseSeconds, float fallSeconds);

/// What the weather at the eye asks of the clouds: the overcast and the rain, 0 to 1 and a little over in a storm,
/// and the wind at the clouds (see CloudWind). No overcast gives the fair weather's scattered cumulus. Overcast with no
/// rain gives a wide, flat layer; rain gives the towers of a storm.
[[nodiscard]] CloudWeather WeatherFor(float overcast, float rain, glm::vec2 cloudWind);

/// The wind at the clouds for the wind at the ground (a direction across the ground and a speed, in world units a
/// second). The clouds move faster than the wind at the ground: a calm day's wind moves them at k_CalmWind, as fast
/// as the original game's clouds move.
[[nodiscard]] glm::vec2 CloudWind(glm::vec2 direction, float groundSpeed);

/// The share of the sun's or the moon's light, for each of red, green and blue, that comes through the air to the
/// clouds at an elevation (the sine of the angle over the horizon), as a share of the light from straight above. The
/// air takes out more blue than red, and more through the long path near the horizon: the light goes orange and red
/// at sunrise and sunset.
[[nodiscard]] glm::vec3 SunThroughAir(float elevation);

/// A debug choice of the cloud cover that takes the place of the weather's: none, clear, scattered, broken, overcast
enum class CoverChoice : uint8_t
{
	Weather,
	Clear,
	Scattered,
	Broken,
	Overcast,
	_Count
};
/// The weather that a choice gives, or the weather as it is for CoverChoice::Weather
[[nodiscard]] CloudWeather ChosenWeather(CoverChoice choice, const CloudWeather& weather);

/// The units that the map of the clouds' shadow covers for the eye's height over the sea: k_ShadowWindow near the land,
/// doubled for each doubling of the height over k_ShadowWindowHeight, so that the shadow reaches as far as the eye sees
[[nodiscard]] float ShadowWindowFor(float eyeHeight);

/// Where the map of the clouds' shadow starts, on whole texels round the eye, so that the shadow does not crawl as the
/// eye moves; `window` is the units that the map covers
[[nodiscard]] glm::vec2 ShadowOrigin(glm::vec2 eye, float window);

/// How far away the clouds show for the eye's height over the sea: k_FarthestCloud near the land, farther from high up
/// so that the layer under a high eye has no edge, at most k_FarthestCloudCap
[[nodiscard]] float FarthestCloudFor(float eyeHeight);

/// The shaders' uniforms of the clouds for a frame
struct CloudUniforms
{
	/// x the bottom of the layer, y the radius of the world's curve, z the farthest cloud, w the sea's level
	glm::vec4 layer {k_CloudBase, k_PlanetRadius, k_FarthestCloud, 0.0f};
	/// x the cover, y the type, z the rain, w the number of storms
	glm::vec4 cover {0.0f};
	/// xy the drift, z the evolution, w the frame's number for the jitter
	glm::vec4 drift {0.0f};
	/// xyz the direction towards the sun or the moon, w how much of the light is the moon's
	glm::vec4 light {0.0f, 1.0f, 0.0f, 0.0f};
	/// rgb the light's colour and strength, w 1 while there is light
	glm::vec4 lightColour {0.0f};
	/// rgb the light of the sky, w the flash of the lightning in the sky
	glm::vec4 ambient {0.0f};
	/// xyz the eye, w 1 while the eye is above the water
	glm::vec4 eye {0.0f};
	/// The flashes of the lightning inside the clouds (see cloud_lightning::Pack)
	std::array<glm::vec4, cloud_lightning::k_MaxFlashes * 2> flashes {};
	/// xy where the map of the shadow starts, z the units it covers, w how dark the shadow is
	glm::vec4 shadow {0.0f};
	/// The storms: [i] xy the middle, z the inner radius, w the outer radius; [k_MaxStorms + i] x the strength, y the rain
	std::array<glm::vec4, k_MaxStorms * 2> storms {};
};

/// The storms' part of the uniforms, from the cells as eased now. Cells with no strength are left out.
void PackStorms(std::span<const StormCell> cells, CloudUniforms& uniforms);

} // namespace openblack::graphics::volumetric_clouds
