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

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace openblack
{

struct TempleLight;

/// The temple's depth fog. Everything drawn in its rooms but the hand, the glows and the light shafts fades with its
/// distance in front of the camera, from where the fog starts to where it ends: its colour is scaled down towards 100
/// of 256 and a share of the fog's colour, two fifths of the temple's light, is added after the texture. The fog comes
/// in 1024 even steps; past its end it stays at its last step. The creature's room has a thicker fog than the others.
///
/// A model is fogged once, at its position. The rooms' lightmapped walls are fogged at each vertex instead, between the
/// wall's own colours and the fog's by the step again, so they thicken more slowly at first.
struct TempleFog
{
	/// The steps the fog's depth is cut into
	static constexpr int32_t k_Steps = 1024;
	/// What a colour is scaled down to at the end of the fog, of 256
	static constexpr float k_FarShade = 100.0f;

	/// Where the fog starts and ends, in front of the camera
	float start {60.0f};
	float end {200.0f};
	/// The colour added at the end of the fog, of 255 per channel
	glm::vec3 colour {102.0f};

	/// A colour scaled by the fog, and the colour it adds, of 255 per channel
	struct Colours
	{
		glm::vec3 diffuse;
		glm::vec3 added;
	};

	/// The fog of the room the player is in, in the temple's light
	[[nodiscard]] static TempleFog In(bool creatureRoom, const TempleLight& light);

	/// Which of the fog's steps a depth is in, 0 to 1023
	[[nodiscard]] int32_t Step(float depth) const;
	/// What a colour is scaled by at a step, of 256
	[[nodiscard]] static float Shade(int32_t step);
	/// The colour added at a step, of 255 per channel
	[[nodiscard]] glm::vec3 Added(int32_t step) const;
	/// A model of a colour fogged at its depth
	[[nodiscard]] Colours AtModel(glm::vec3 modelColour, float depth) const;
	/// A lightmapped wall's vertex of its own colour and added colour, fogged at its depth
	[[nodiscard]] Colours AtVertex(glm::vec3 diffuse, glm::vec3 added, float depth) const;

	/// The haze uniform the object shaders read: x the start, y the end, z the far shade and w 2 for the temple's fog
	[[nodiscard]] glm::vec4 Uniform() const;
};

} // namespace openblack
