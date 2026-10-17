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

#include <functional>
#include <span>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "Graphics/Underwater.h"

/// EXPERIMENT (not for merge): the rules of the experiment's streams. The original game draws each river as a channel
/// cut into the land's alpha, with its sea under the land. The experiment draws flowing water in those channels: its
/// flow comes from the slope of the land along each river, and it is fast and white where the river is steep and slow
/// and smooth in the pools.
namespace openblack::graphics::underwater::streams
{

/// Game units in one metre: the water's one scale, from the height of an adult male villager
using underwater::k_UnitsPerMetre;
/// The gravity, in units a second a second
inline constexpr float k_Gravity = 9.81f * k_UnitsPerMetre;

/// The flow map: one texel each k_MapSpacing units, over all the rivers and k_MapMargin units round them
inline constexpr float k_MapSpacing = 1.0f;
inline constexpr float k_MapMargin = 30.0f;
/// The texels nearer than this to a river's line take its flow
inline constexpr float k_MapReach = 8.0f;
/// The largest distance that the flow map keeps
inline constexpr float k_FarDistance = 16.0f;

/// The water is slowest in a pool and fastest down a steep fall, in units a second
inline constexpr float k_LeastSpeed = 0.15f * k_UnitsPerMetre;
inline constexpr float k_MostSpeed = 2.6f * k_UnitsPerMetre;
/// The speed of the water for a slope of 1, before the limits: the speed goes as the root of the slope
inline constexpr float k_SpeedForSlope = 3.2f * k_UnitsPerMetre;
/// A slope less than this makes no white water; a slope more than k_WhiteSlope is all white water
inline constexpr float k_CalmSlope = 0.1f;
inline constexpr float k_WhiteSlope = 0.38f;
/// The deepest the water is, in the middle of a channel
inline constexpr float k_MostDepth = 0.35f * k_UnitsPerMetre;
/// A point is in the water where the channel leaves less than this of the land's alpha
inline constexpr float k_WaterAlpha = 0.9f;
/// A river's end that is at most this high is where it meets the sea or a pond
inline constexpr float k_MouthHeight = 0.5f;
/// The most ends of rivers that the water's shader knows
inline constexpr size_t k_MostMouths = 8;

/// The look of a river's water, one value for each control. It has the same controls as the experiment's profile of a
/// body of water, so that a river can use that profile when the experiment has one.
struct RiverProfile
{
	/// How much of each colour the water takes out for each metre that the light goes through it
	glm::vec3 absorption {0.0f};
	/// The colour of the light that the water scatters back towards the eye
	glm::vec3 scatter {0.0f};
	/// How clear the water is, 0 to 1: 1 shows the bed through the water at full strength
	float clarity {1.0f};
	/// How strongly the water reflects the sky, 0 to 1
	float reflection {1.0f};
	/// How high the small waves are, 1 for the usual height
	float waveScale {1.0f};
	/// How much the wind roughens the water, 0 to 1: most on the slow pools
	float windResponse {0.0f};
};

/// The river's look: clear, shallow fresh water that takes out the red first, and a little green and brown scatter
inline constexpr RiverProfile k_River {.absorption = {0.45f, 0.12f, 0.09f},
                                       .scatter = {0.16f, 0.26f, 0.27f},
                                       .clarity = 0.9f,
                                       .reflection = 0.9f,
                                       .waveScale = 1.0f,
                                       .windResponse = 0.6f};

/// The channel's footprint, from the river's model: the alpha that the channel leaves in the land, and how its texels
/// lie on the model. The model lies along its x for k_MeshLength units, and its y goes across the channel.
struct Channel
{
	/// The texture's size in texels
	glm::ivec2 size {0};
	/// The alpha of each texel, 0 to 15, row by row
	std::vector<uint8_t> alpha;
	/// The texel at a point of the model: texel = column0 * x + column1 * y + column2
	glm::vec2 column0 {0.0f};
	glm::vec2 column1 {0.0f};
	glm::vec2 column2 {0.0f};
};

/// The channel of a model's footprint, from three corners of one of its triangles on the model and in the texture
[[nodiscard]] Channel ChannelFrom(glm::ivec2 size, std::vector<uint8_t> alpha, std::span<const glm::vec2, 3> model,
                                  std::span<const glm::vec2, 3> texels);
/// The alpha that the channel leaves at a point of the model, 0 to 1: the nearest texel, as the original reads it, and 1
/// outside the footprint
[[nodiscard]] float ChannelAlpha(const Channel& channel, glm::vec2 model);

/// A stretch of river from one point to the next, downstream
struct Segment
{
	glm::vec2 from {0.0f};
	glm::vec2 to {0.0f};
	/// The speed of the water at the two ends, in units a second
	float speedFrom {0.0f};
	float speedTo {0.0f};
	/// How much white water there is at the two ends, 0 to 1
	float whiteFrom {0.0f};
	float whiteTo {0.0f};
	/// The river that it is part of
	uint32_t river {0};
	/// The direction of the original's channel model: from its first point to its next, as the land's script gives
	/// them. The channel is laid the same way whatever way the water flows.
	glm::vec2 modelFrom {0.0f};
	glm::vec2 modelTo {0.0f};
};

/// Where a river meets the sea or a pond
struct Mouth
{
	glm::vec2 position {0.0f};
	/// The direction in which the water leaves the river, a unit long
	glm::vec2 direction {1.0f, 0.0f};
	/// How fast the water leaves the river
	float speed {0.0f};
};

/// All the rivers of a land
struct Network
{
	std::vector<Segment> segments;
	std::vector<Mouth> mouths;
	/// The box of all the rivers' points across the ground
	glm::vec2 low {0.0f};
	glm::vec2 high {0.0f};
};

/// The height of the land at a point
using HeightAt = std::function<float(glm::vec2)>;

/// The speed of the water down a slope (the drop over the length): as the root of the slope, between the least and the
/// most speed
[[nodiscard]] float SpeedForSlope(float slope);
/// The white water down a slope, 0 to 1
[[nodiscard]] float WhiteWaterForSlope(float slope);
/// How much a bend slows the water, from the directions into it and out of it (each a unit long): 1 for no bend, less
/// for a sharper bend
[[nodiscard]] float BendFactor(glm::vec2 in, glm::vec2 out);

/// The rivers as the land's script gives their points, with the land's height. Each river flows from its higher end to
/// its lower end. Where the land rises along the way, the water is a slow pool. An end at the sea's height is a mouth.
[[nodiscard]] Network BuildNetwork(std::span<const std::vector<glm::vec2>> rivers, const HeightAt& heightAt);

/// One texel of the flow map
struct FlowTexel
{
	/// The water's velocity across the ground, in units a second
	glm::vec2 flow {0.0f};
	/// How much white water there is, 0 to 1
	float white {0.0f};
	/// The distance to the nearest river's line, at most k_FarDistance
	float distance {k_FarDistance};
};

/// The flow of all the rivers on a grid across the ground
struct FlowMap
{
	/// The point at the middle of the first texel
	glm::vec2 origin {0.0f};
	glm::ivec2 size {0};
	float spacing {k_MapSpacing};
	std::vector<FlowTexel> texels;
};

/// The flow map of the rivers. Near two rivers the flow is the mean of the two, weighted by the distance to each.
[[nodiscard]] FlowMap BuildFlowMap(const Network& network, float spacing = k_MapSpacing);
/// The flow map at a point, between its four nearest texels; no flow and the far distance outside the map
[[nodiscard]] FlowTexel Sample(const FlowMap& map, glm::vec2 point);
/// The flow map as RGBA16F texels: the flow along x and z, the white water and the distance
[[nodiscard]] std::vector<uint16_t> PackFlowMap(const FlowMap& map);

/// The alpha that the rivers' channels leave in the land at a point, 0 to 1 (1 away from them): the lowest of all the
/// channels laid over the point, as the original lays them
[[nodiscard]] float ChannelAt(const Network& network, const Channel& channel, glm::vec2 point);

/// How loud the rivers sound at a point, 0 to 1: louder near them, and near fast and white water
[[nodiscard]] float Loudness(const Network& network, glm::vec2 listener);

/// A thing that drifts down a river and out into the sea: a leaf or a twig
struct Drifter
{
	glm::vec2 position {0.0f};
	/// Seconds since it started, and how long it lives
	float age {0.0f};
	float life {0.0f};
	/// Which way it is turned, in radians, and how fast it turns
	float angle {0.0f};
	float spin {0.0f};
	/// A number of its own, for its kind and its colour
	uint32_t seed {0};
	/// Whether it has left the river for the sea
	bool atSea {false};
};

/// The velocity of a drifter on the river from the flow there: the flow, pushed back towards the middle of the channel
/// when it comes near the bank
[[nodiscard]] glm::vec2 DriftVelocity(const FlowTexel& texel, glm::vec2 towardsMiddle);

/// The water's height in a river at a point over the land's height there: it is deepest in the middle of the channel,
/// from the alpha that the channel leaves (see ChannelAt)
[[nodiscard]] float DepthForAlpha(float alpha);

} // namespace openblack::graphics::underwater::streams
