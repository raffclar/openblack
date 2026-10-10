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

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The sparks of a mana path: each runs from where it is let out to where it goes, along a straight line at a steady
/// speed or along a curve over a set time, weaving from side to side by noise and kept a little above the land
namespace openblack::particles::mana_path
{

/// A spark to let out: from where to where, in a colour 0xRRGGBB
struct Spark
{
	glm::vec3 from {0.0f};
	glm::vec3 to {0.0f};
	uint32_t rgb {0};
};

/// A spark's path, worked out once when it is made
struct Path
{
	/// Where it starts and where it goes
	glm::vec3 from {0.0f};
	glm::vec3 to {0.0f};
	/// Across the path on the ground, a unit long: what it weaves along
	glm::vec3 side {0.0f};
	/// The path's length across the ground
	float length {0.0f};
	/// The curve's terms, from the cube's to the constant's: it leaves the start and arrives at the end both heading
	/// straight along the path
	glm::vec3 cubic {0.0f};
	glm::vec3 square {0.0f};
	glm::vec3 linear {0.0f};
	glm::vec3 constant {0.0f};
	/// How far it weaves, how fast the weave changes along the path, and where along the noise it starts
	float amplitude {0.0f};
	float frequency {0.0f};
	float phase {0.0f};
};

/// The random numbers a spark draws as it is made: each in 0..0.8 for its weave's frequency and size, and in 0..1 for
/// where along the noise it starts
struct Draws
{
	float frequency;
	float amplitude;
	float phase;
};

/// The rule's settings from the particle file
struct Settings
{
	/// Metres a second when it keeps a steady speed
	float speed;
	/// Seconds along the curve when it does not
	float timeToTravel;
	float noiseFrequency;
	float noiseAmplitude;
	/// How high above the land it is drawn
	float height;
	bool constantSpeed;
};

/// A path shorter than this across the ground makes no spark
constexpr float k_ShortestPath = 0.01f;

/// The path from a start to an end with the spark's own draws
[[nodiscard]] Path MakePath(glm::vec3 from, glm::vec3 to, const Settings& settings, const Draws& draws);

/// The heading the spark's sprite is turned to about the vertical: along the path, a quarter turn round
[[nodiscard]] float Heading(const Path& path);

/// How far along its path a spark of an age is: by its speed over the path's length, or by its time to travel. Past
/// 1 it has arrived and goes on the next step.
[[nodiscard]] float Progress(const Path& path, const Settings& settings, float age);

/// How much of its weave a spark shows along its path: growing over the first fifth, full until four fifths along and
/// dying away by its end
[[nodiscard]] float WeaveShare(float along);

/// Where the spark is along its path before it weaves: on the straight line at the progress held to 0..1, or on the
/// curve at the progress as it is
[[nodiscard]] glm::vec3 PointOnPath(const Path& path, const Settings& settings, float progress);

/// Where along the noise the weave is read at a progress held to 0..1
[[nodiscard]] float WeaveNoiseAt(const Path& path, float along);

/// Where the spark is across the ground once woven aside by the noise read there, before it is put on the land
[[nodiscard]] glm::vec2 Woven(const Path& path, glm::vec3 point, float along, float noise);

/// The ground position the land's height is read at, to the map's fixed point grid as the game rounds it
[[nodiscard]] glm::vec2 GroundPoint(glm::vec2 xz);

} // namespace openblack::particles::mana_path
