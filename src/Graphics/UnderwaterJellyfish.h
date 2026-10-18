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
#include <functional>
#include <span>
#include <unordered_map>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "Graphics/Underwater.h"

/// EXPERIMENT (not for merge): the jellyfish in the deep sea. They are not in the original game. They drift slowly up
/// through the deep water in a loose grid round the eye, one at most in each cell, and a new one starts near the sea bed
/// when one gets near the top of their band. They stay deep: the water hides them fully from the surface and from
/// above it, and they show only to an eye under the water near them. Each is one instance of a small mesh: a bell (an
/// outer and an inner shell) with thick inner tentacles and thin outer ones, and a halo of their soft light in the
/// water round them. The shaders vs_underwater_jelly.sc and fs_underwater_jelly.sc make the beat of the bell, the sway
/// of the tentacles, the glow and the halo.
namespace openblack::graphics::underwater::jellyfish
{

/// The side of a cell of the grid, in units: one jellyfish at most in each cell
inline constexpr float k_Cell = 12.0f;
/// How far from the eye they are, in units; they fade out over the last part of it
inline constexpr float k_Range = 170.0f;
/// The share of the range over which they fade out
inline constexpr float k_FadeShare = 0.3f;
/// The least depth of their tops under the sea's level, in units (about 27 m): the water between them and the
/// surface takes out all of their light
inline constexpr float k_TopDepth = 26.0f;
/// The least depth of the sea bed where they are, in units: their band under the top depth must have space for them
inline constexpr float k_LeastDepth = 33.0f;
/// The depth over which they come in past the least depth
inline constexpr float k_DepthFade = 5.0f;
/// The share of the deep cells that have one
inline constexpr float k_Share = 0.55f;
/// How fast they rise, in units a second
inline constexpr float k_Rise = 0.22f;
/// The space they keep from the sea bed, in units
inline constexpr float k_BedGap = 1.0f;
/// The size of the bell across, the least and the most, in units
inline constexpr float k_LeastSize = 0.5f;
inline constexpr float k_MostSize = 1.4f;
/// The most that are drawn
inline constexpr uint32_t k_Most = 320;

/// The two passes of the shader, as the w of the eye's uniform: the bodies in the scene, and their own light after the
/// water's pass
inline constexpr float k_BodyPass = 0.0f;
inline constexpr float k_GlowPass = 1.0f;
/// Their own light is soft: no colour of it is brighter than this, also in the brightest flash
inline constexpr float k_MostGlow = 0.8f;
/// The water that scatters their light out of its straight way to the eye, in each unit, the same for each colour.
/// With the absorption, it takes out a small light in about 25 units.
inline constexpr float k_GlowScatter = 0.2f;
/// The radius of the water round each jellyfish that its light makes bright: half a metre, so about one cubic metre
inline constexpr float k_LightRadius = 0.5f * k_UnitsPerMetre;
/// How bright the halo of water is through its middle, against the glow of the jellyfish
inline constexpr float k_HaloStrength = 0.35f;

/// The colour of their light: blue-green, about 480 nm. The sea water takes out this colour least, so deep sea
/// animals use it.
inline constexpr glm::vec3 k_LightColour {0.04f, 0.62f, 1.0f};
/// The colour of the light of the most green of them, about 500 nm
inline constexpr glm::vec3 k_GreenColour {0.10f, 0.95f, 0.70f};
/// The most that the light of one of them moves towards the green colour
inline constexpr float k_MostGreen = 0.4f;
/// How bright their light is at rest, and the most that a flash adds to it
inline constexpr float k_RestLight = 0.4f;
inline constexpr float k_FlashLight = 0.5f;
/// The water further than this from a jellyfish gets less than one step of an 8-bit colour of its light, in units
inline constexpr float k_LightReach = 5.0f;
/// The most jellyfish that light the water, the sea bed, the motes and the other jellyfish: the nearest to the eye
inline constexpr uint32_t k_Lights = 16;

/// The reach of each thing that makes them flash, in units: a jellyfish nearer than a third of the reach flashes fully.
/// The camera must come very near: it does not show what is nearer than about 3.5 units. A splash on the surface sends
/// a wave of pressure down through the water.
inline constexpr float k_CameraReach = 9.0f;
inline constexpr float k_HandReach = 6.0f;
inline constexpr float k_CreatureReach = 12.0f;
inline constexpr float k_SplashReach = 60.0f;
/// A splash makes them flash only for this time after it starts, in seconds, while its wave goes past
inline constexpr float k_StirWindow = 0.5f;
/// The time for a flash to come up, and the time for it to go down to about a third, in seconds. A flash goes in about
/// one and a half seconds.
inline constexpr float k_FlashRise = 0.08f;
inline constexpr float k_FlashFade = 0.5f;

/// The parts of the mesh: the outer shell of the bell, its inner shell, the inner tentacles and the outer tentacles
enum class Part : uint8_t
{
	OuterShell,
	InnerShell,
	InnerTentacle,
	OuterTentacle,
	Halo,
};
inline constexpr uint32_t k_InnerTentacles = 6;
inline constexpr uint32_t k_OuterTentacles = 13;
/// Rings and sides of each shell, and lengths along each tentacle
inline constexpr uint32_t k_ShellRings = 7;
inline constexpr uint32_t k_ShellSides = 18;
inline constexpr uint32_t k_TentacleSegments = 10;

/// One jellyfish this frame
struct Jellyfish
{
	/// The top of its bell
	glm::vec3 position {0.0f};
	/// The bell's size across
	float size {1.0f};
	/// The phase of its beat, 0 to 1
	float phase {0.0f};
	/// The colour of its light, 0 blue-green to 1 a little more green
	float hue {0.0f};
	/// How much it shows: it fades in near the sea bed, out near the surface, and out at the far edge of the range
	float shows {1.0f};
	/// A random number of its own, 0 to 1
	float seed {0.0f};
	/// A number of its own that does not change while it rises, from its cell
	uint32_t id {0};
};

/// The depth of the sea bed below the sea at a point (positive under the sea)
using DepthAt = std::function<float(glm::vec2 point)>;

/// The jellyfish round the eye at a time: the cells in range whose sea bed is deep enough each have one or none by a
/// hash of the cell, at a place in the cell and a height that rises with the time. The nearest come first.
[[nodiscard]] std::vector<Jellyfish> Around(glm::vec2 eye, float seconds, float seaLevel, const DepthAt& depthAt);

/// The three vectors of a jellyfish's instance: xyz the top of the bell and w its size; x the phase, y the colour, z how
/// much it shows and w its seed; x its flash, 0 to 1, and y the number of its light, or -1 when it has none
[[nodiscard]] std::array<glm::vec4, 3> InstanceData(const Jellyfish& jellyfish, float flash, int32_t light);

/// Where the beat of the bell is at a time, 0 to 1 (the shaders have the same rule)
[[nodiscard]] float BeatCycle(const Jellyfish& jellyfish, float seconds);
/// How much the bell squeezes at a point of its beat, 0 to 1: a quick squeeze, then a slow release
[[nodiscard]] float Squeeze(float cycle);
/// The middle of the bell, where its light comes from
[[nodiscard]] glm::vec3 Middle(const Jellyfish& jellyfish);
/// The colour of the light of a jellyfish, from its colour number 0 to 1: blue-green, a little more green for some
[[nodiscard]] glm::vec3 LightColour(float hue);
/// How bright the light of a jellyfish is: dim at rest, a little brighter as the bell squeezes, and brighter in a flash.
/// It is never more than k_MostGlow.
[[nodiscard]] float Emission(float squeeze, float flash);
/// The part of each colour of a jellyfish's light that comes to a point at a distance from its middle. The light
/// spreads out as the square of the distance (from the size of the bell) and the water takes it out on the way.
[[nodiscard]] glm::vec3 LightAt(float distance);

/// One light of a jellyfish this frame: where it is, and its colour times how bright it is
struct Light
{
	glm::vec3 position {0.0f};
	glm::vec3 colour {0.0f};
};
[[nodiscard]] Light LightOf(const Jellyfish& jellyfish, float seconds, float flash);

/// A thing near a jellyfish that can make it flash: its distance, its reach, and the time since it started (0 for a
/// thing that is there now)
struct Touch
{
	float distance {0.0f};
	float reach {1.0f};
	float since {0.0f};
};
/// How much the things near a jellyfish stir it, 0 to 1: fully nearer than a third of the reach of a thing, none past
/// its reach. A thing that started more than k_StirWindow ago (a splash) does not stir it.
[[nodiscard]] float Stir(std::span<const Touch> touches);
/// The flash of a jellyfish after a step of time, from its flash before and the stir now. The flash comes up quickly to
/// the stir, and goes down slowly when the stir is less.
[[nodiscard]] float StepFlash(float flash, float stir, float step);

/// A thing in the world that can make the jellyfish flash: where it is, its reach and the time since it started
struct Disturber
{
	glm::vec3 position {0.0f};
	float reach {1.0f};
	float since {0.0f};
};

/// The flashes of the jellyfish round the eye. Each frame, the things near each jellyfish stir it, and its flash comes
/// up and goes down by StepFlash. A jellyfish with no flash is not kept.
class Flashes
{
public:
	/// Steps the flashes to a time. The time of the first step and a time that goes back give no step.
	void Update(std::span<const Jellyfish> jellyfish, std::span<const Disturber> disturbers, float seconds);
	/// The flash of a jellyfish, 0 to 1
	[[nodiscard]] float Of(uint32_t id) const;

private:
	std::unordered_map<uint32_t, float> _flashes;
	float _seconds {-1.0f};
};

/// The length of the way from the eye to a point that goes through the water: all of it with the eye under the water,
/// and the part under the sea's level with the eye above it
[[nodiscard]] float WaterPath(glm::vec3 eye, glm::vec3 point, float seaLevel);

/// The part of each colour of a jellyfish's own light that comes to the eye through a length of water. The water absorbs
/// it and scatters it out of its way, so a small light fades faster than the light of the scene round it.
[[nodiscard]] glm::vec3 GlowThroughWater(float path);

/// A vertex of the mesh: x along (round the shell, or down the tentacle, 0 to 1), y across (down the shell 0 to 1, or
/// the side of the tentacle, -1 or 1), z the part, w which tentacle
struct Vertex
{
	glm::vec4 data {0.0f};
};
/// The mesh of one jellyfish, with the inner shell, the tentacles and the halo (a square that turns to face the eye)
/// first and the outer shell last, so that the outer shell glows over the rest
void Mesh(std::vector<Vertex>& vertices, std::vector<uint16_t>& indices);

} // namespace openblack::graphics::underwater::jellyfish
