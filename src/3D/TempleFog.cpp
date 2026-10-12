/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleFog.h"

#include <cmath>

#include <algorithm>

#include <glm/common.hpp>

#include "TempleLight.h"

using namespace openblack;

namespace
{
/// The creature's room is fogged from 40 to 150, the others from 60 to 200
constexpr float k_CreatureRoomStart = 40.0f;
constexpr float k_CreatureRoomEnd = 150.0f;
constexpr float k_RoomStart = 60.0f;
constexpr float k_RoomEnd = 200.0f;
/// The fog's colour is this share of the temple's light
constexpr float k_ColourShare = 0.4f;
/// The fog's colour is added in full only past its end; at a step it is the step's share of it
constexpr float k_StepShare = 1.0f / static_cast<float>(TempleFog::k_Steps);

/// Moves each byte of a colour towards another's by the step's share, rounding down as whole bytes do
glm::vec3 Towards(glm::vec3 from, glm::vec3 to, int32_t step)
{
	// A quarter of the step is the share, of 256, the colour moves by
	const auto share = static_cast<float>(step / 4);
	return from + glm::floor((to - from) * share / 256.0f);
}
} // namespace

TempleFog TempleFog::In(bool creatureRoom, const TempleLight& light)
{
	// The light's colour in whole bytes
	const auto bytes = glm::floor(light.multiply * 255.0f + 0.5f);
	return {
	    .start = creatureRoom ? k_CreatureRoomStart : k_RoomStart,
	    .end = creatureRoom ? k_CreatureRoomEnd : k_RoomEnd,
	    .colour = bytes * k_ColourShare,
	};
}

int32_t TempleFog::Step(float depth) const
{
	if (depth < start)
	{
		return 0;
	}
	if (depth > end)
	{
		return k_Steps - 1;
	}
	const float perUnit = static_cast<float>(k_Steps) / (end - start);
	return std::clamp(static_cast<int32_t>((depth - start) * perUnit), 0, k_Steps - 1);
}

float TempleFog::Shade(int32_t step)
{
	// The scale falls from 256 to the far shade in whole steps, and is then taken as a byte of 255
	const float share = static_cast<float>(step) * k_StepShare;
	const auto scale = 256 - static_cast<int32_t>((256.0f - k_FarShade) * share);
	return static_cast<float>((scale * 255) >> 8);
}

glm::vec3 TempleFog::Added(int32_t step) const
{
	// Rounded to the nearest whole number, halves to even
	const float share = static_cast<float>(step) * k_StepShare;
	const auto added = colour * share;
	return {std::nearbyint(added.r), std::nearbyint(added.g), std::nearbyint(added.b)};
}

TempleFog::Colours TempleFog::AtModel(glm::vec3 modelColour, float depth) const
{
	const auto step = Step(depth);
	return {.diffuse = glm::floor(modelColour * Shade(step) / 256.0f), .added = Added(step)};
}

TempleFog::Colours TempleFog::AtVertex(glm::vec3 diffuse, glm::vec3 added, float depth) const
{
	// Up to the start the wall keeps its colours, and past the end it takes the fog's last step's alone, grey
	if (!(depth > start))
	{
		return {.diffuse = diffuse, .added = added};
	}
	if (!(depth < end))
	{
		return {.diffuse = glm::vec3(Shade(k_Steps - 1)), .added = Added(k_Steps - 1)};
	}
	const auto step = Step(depth);
	return {.diffuse = Towards(diffuse, glm::vec3(Shade(step)), step), .added = Towards(added, Added(step), step)};
}

glm::vec4 TempleFog::Uniform() const
{
	return {start, end, k_FarShade, 2.0f};
}
