/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the rules of the experiment's streams

#include "UnderwaterStreams.h"

#include <cmath>

#include <algorithm>
#include <limits>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/packing.hpp>
#include <glm/mat3x3.hpp>

namespace openblack::graphics::underwater::streams
{

namespace
{
/// The length of the original's channel model along its x (see ecs::components::StreamSegment)
constexpr float k_MeshLength = 30.0f;
/// The heights along a river are found this many units apart, and smoothed over the next and the last
constexpr float k_HeightStep = 2.0f;
/// The speeds and the white water along a river are smoothed this many times
constexpr int k_SmoothPasses = 2;

/// The distance from a point to a segment, and how far along the segment the nearest point is, 0 to 1
glm::vec2 DistanceAlong(glm::vec2 point, glm::vec2 from, glm::vec2 to)
{
	const glm::vec2 line = to - from;
	const float length2 = glm::dot(line, line);
	const float t = length2 > 0.0f ? std::clamp(glm::dot(point - from, line) / length2, 0.0f, 1.0f) : 0.0f;
	return {glm::distance(point, from + line * t), t};
}

/// The mean height of the land along a stretch, found every k_HeightStep units
float MeanHeight(glm::vec2 from, glm::vec2 to, const HeightAt& heightAt)
{
	const float length = glm::distance(from, to);
	const int steps = std::max(1, static_cast<int>(std::ceil(length / k_HeightStep)));
	float sum = 0.0f;
	for (int i = 0; i <= steps; ++i)
	{
		sum += heightAt(glm::mix(from, to, static_cast<float>(i) / static_cast<float>(steps)));
	}
	return sum / static_cast<float>(steps + 1);
}

/// Smooths the values along a river with the values at the points next to them
void Smooth(std::vector<float>& values)
{
	if (values.size() < 3)
	{
		return;
	}
	for (int pass = 0; pass < k_SmoothPasses; ++pass)
	{
		auto before = values;
		for (size_t i = 1; i + 1 < values.size(); ++i)
		{
			values[i] = 0.25f * before[i - 1] + 0.5f * before[i] + 0.25f * before[i + 1];
		}
	}
}
} // namespace

Channel ChannelFrom(glm::ivec2 size, std::vector<uint8_t> alpha, std::span<const glm::vec2, 3> model,
                    std::span<const glm::vec2, 3> texels)
{
	// The texel is an affine map of the point: solve it from the three corners
	const glm::mat3 corners {glm::vec3(model[0], 1.0f), glm::vec3(model[1], 1.0f), glm::vec3(model[2], 1.0f)};
	const glm::mat3 inverse = glm::inverse(corners);
	const glm::vec3 us {texels[0].x, texels[1].x, texels[2].x};
	const glm::vec3 vs {texels[0].y, texels[1].y, texels[2].y};
	// Each row of the map, for x, y and 1
	const glm::vec3 uRow = glm::transpose(inverse) * us;
	const glm::vec3 vRow = glm::transpose(inverse) * vs;
	return {.size = size,
	        .alpha = std::move(alpha),
	        .column0 = {uRow.x, vRow.x},
	        .column1 = {uRow.y, vRow.y},
	        .column2 = {uRow.z, vRow.z}};
}

float ChannelAlpha(const Channel& channel, glm::vec2 model)
{
	const glm::vec2 texel = channel.column0 * model.x + channel.column1 * model.y + channel.column2;
	const glm::ivec2 at {static_cast<int>(std::floor(texel.x)), static_cast<int>(std::floor(texel.y))};
	if (at.x < 0 || at.y < 0 || at.x >= channel.size.x || at.y >= channel.size.y || channel.alpha.empty())
	{
		return 1.0f;
	}
	return static_cast<float>(channel.alpha.at(static_cast<size_t>(at.y * channel.size.x + at.x))) / 15.0f;
}

float SpeedForSlope(float slope)
{
	return std::clamp(k_SpeedForSlope * std::sqrt(std::max(slope, 0.0f)), k_LeastSpeed, k_MostSpeed);
}

float WhiteWaterForSlope(float slope)
{
	const float t = std::clamp((slope - k_CalmSlope) / (k_WhiteSlope - k_CalmSlope), 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

float BendFactor(glm::vec2 in, glm::vec2 out)
{
	// A right angle slows the water to 0.6 of its speed, and a turn back to 0.4
	const float turn = 1.0f - std::clamp(glm::dot(in, out), -1.0f, 1.0f);
	return 1.0f - 0.3f * std::min(turn, 2.0f);
}

Network BuildNetwork(std::span<const std::vector<glm::vec2>> rivers, const HeightAt& heightAt)
{
	Network network;
	network.low = glm::vec2(std::numeric_limits<float>::max());
	network.high = glm::vec2(std::numeric_limits<float>::lowest());
	for (uint32_t river = 0; river < rivers.size(); ++river)
	{
		const auto& given = rivers[river];
		if (given.size() < 2)
		{
			continue;
		}
		for (const auto& point : given)
		{
			network.low = glm::min(network.low, point);
			network.high = glm::max(network.high, point);
		}
		// The water flows from the higher end to the lower end
		const bool reversed = heightAt(given.front()) < heightAt(given.back());
		std::vector<glm::vec2> points = given;
		if (reversed)
		{
			std::ranges::reverse(points);
		}
		const size_t count = points.size() - 1;
		// The height of each point: the mean of the land along the stretches next to it
		std::vector<float> heights(points.size());
		for (size_t i = 0; i < points.size(); ++i)
		{
			const glm::vec2 before = i > 0 ? glm::mix(points[i], points[i - 1], 0.5f) : points[i];
			const glm::vec2 after = i < count ? glm::mix(points[i], points[i + 1], 0.5f) : points[i];
			heights[i] = 0.5f * (MeanHeight(before, points[i], heightAt) + MeanHeight(points[i], after, heightAt));
		}
		// The slope of each stretch. Where the land rises downstream, the water stands in a pool.
		std::vector<float> speeds(count);
		std::vector<float> whites(count);
		for (size_t i = 0; i < count; ++i)
		{
			const float length = std::max(glm::distance(points[i], points[i + 1]), 0.5f);
			const float slope = std::max(heights[i] - heights[i + 1], 0.0f) / length;
			float bend = 1.0f;
			if (i > 0)
			{
				const glm::vec2 in = glm::normalize(points[i] - points[i - 1] + glm::vec2(1e-6f, 0.0f));
				const glm::vec2 out = glm::normalize(points[i + 1] - points[i] + glm::vec2(1e-6f, 0.0f));
				bend = BendFactor(in, out);
			}
			speeds[i] = SpeedForSlope(slope) * bend;
			whites[i] = WhiteWaterForSlope(slope);
		}
		Smooth(speeds);
		Smooth(whites);
		for (size_t i = 0; i < count; ++i)
		{
			const float speedFrom = i > 0 ? 0.5f * (speeds[i - 1] + speeds[i]) : speeds[i];
			const float speedTo = i + 1 < count ? 0.5f * (speeds[i] + speeds[i + 1]) : speeds[i];
			const float whiteFrom = i > 0 ? 0.5f * (whites[i - 1] + whites[i]) : whites[i];
			const float whiteTo = i + 1 < count ? 0.5f * (whites[i] + whites[i + 1]) : whites[i];
			// The original's channel lies from the script's point to its next, whichever way the water flows
			const size_t first = reversed ? count - i : i;
			const size_t next = reversed ? count - i - 1 : i + 1;
			network.segments.push_back({.from = points[i],
			                            .to = points[i + 1],
			                            .speedFrom = std::max(speedFrom, k_LeastSpeed),
			                            .speedTo = std::max(speedTo, k_LeastSpeed),
			                            .whiteFrom = whiteFrom,
			                            .whiteTo = whiteTo,
			                            .river = river,
			                            .modelFrom = given[std::min(first, next)],
			                            .modelTo = given[std::max(first, next)]});
		}
		// The lower end at the sea's height is a mouth
		if (heightAt(points[count]) <= k_MouthHeight && network.mouths.size() < k_MostMouths)
		{
			const glm::vec2 direction = glm::normalize(points[count] - points[count - 1] + glm::vec2(1e-6f, 0.0f));
			network.mouths.push_back({.position = points[count], .direction = direction, .speed = speeds.back()});
		}
	}
	if (network.segments.empty())
	{
		network.low = network.high = glm::vec2(0.0f);
	}
	return network;
}

FlowMap BuildFlowMap(const Network& network, float spacing)
{
	FlowMap map;
	map.spacing = spacing;
	if (network.segments.empty())
	{
		return map;
	}
	const glm::vec2 low = network.low - k_MapMargin;
	const glm::vec2 high = network.high + k_MapMargin;
	map.origin = low;
	map.size = glm::ivec2(glm::ceil((high - low) / spacing)) + 1;
	const auto count = static_cast<size_t>(map.size.x) * static_cast<size_t>(map.size.y);
	map.texels.assign(count, FlowTexel {});
	struct Sum
	{
		glm::vec2 flow {0.0f};
		float white {0.0f};
		float weight {0.0f};
	};
	std::vector<Sum> sums(count);
	for (const auto& segment : network.segments)
	{
		const glm::vec2 line = segment.to - segment.from;
		const float length = glm::length(line);
		if (length <= 0.0f)
		{
			continue;
		}
		const glm::vec2 direction = line / length;
		const glm::vec2 boxLow = glm::min(segment.from, segment.to) - k_FarDistance;
		const glm::vec2 boxHigh = glm::max(segment.from, segment.to) + k_FarDistance;
		const glm::ivec2 first = glm::max(glm::ivec2(glm::floor((boxLow - map.origin) / spacing)), glm::ivec2(0));
		const glm::ivec2 last = glm::min(glm::ivec2(glm::ceil((boxHigh - map.origin) / spacing)), map.size - 1);
		for (int z = first.y; z <= last.y; ++z)
		{
			for (int x = first.x; x <= last.x; ++x)
			{
				const glm::vec2 point = map.origin + glm::vec2(x, z) * spacing;
				const glm::vec2 found = DistanceAlong(point, segment.from, segment.to);
				const float distance = found.x;
				const float t = found.y;
				const auto index = static_cast<size_t>(z) * static_cast<size_t>(map.size.x) + static_cast<size_t>(x);
				auto& texel = map.texels[index];
				texel.distance = std::min(texel.distance, distance);
				if (distance > k_MapReach)
				{
					continue;
				}
				// The nearer river counts more, so that the flow turns smoothly where two rivers meet
				const float weight = std::exp(-distance * distance / 4.0f) + 1e-4f;
				auto& sum = sums[index];
				sum.flow += direction * glm::mix(segment.speedFrom, segment.speedTo, t) * weight;
				sum.white += glm::mix(segment.whiteFrom, segment.whiteTo, t) * weight;
				sum.weight += weight;
			}
		}
	}
	for (size_t i = 0; i < count; ++i)
	{
		if (sums[i].weight > 0.0f)
		{
			map.texels[i].flow = sums[i].flow / sums[i].weight;
			map.texels[i].white = sums[i].white / sums[i].weight;
		}
	}
	return map;
}

FlowTexel Sample(const FlowMap& map, glm::vec2 point)
{
	if (map.texels.empty())
	{
		return {};
	}
	const glm::vec2 at = (point - map.origin) / map.spacing;
	if (at.x < 0.0f || at.y < 0.0f || at.x > static_cast<float>(map.size.x - 1) || at.y > static_cast<float>(map.size.y - 1))
	{
		return {};
	}
	const glm::ivec2 low = glm::min(glm::ivec2(glm::floor(at)), map.size - 2);
	const glm::vec2 f = at - glm::vec2(low);
	const auto texel = [&map](int x, int z) {
		return map.texels[static_cast<size_t>(z) * static_cast<size_t>(map.size.x) + static_cast<size_t>(x)];
	};
	const auto mix = [](const FlowTexel& a, const FlowTexel& b, float t) {
		return FlowTexel {.flow = glm::mix(a.flow, b.flow, t),
		                  .white = glm::mix(a.white, b.white, t),
		                  .distance = glm::mix(a.distance, b.distance, t)};
	};
	const auto bottom = mix(texel(low.x, low.y), texel(low.x + 1, low.y), f.x);
	const auto top = mix(texel(low.x, low.y + 1), texel(low.x + 1, low.y + 1), f.x);
	return mix(bottom, top, f.y);
}

std::vector<uint16_t> PackFlowMap(const FlowMap& map)
{
	std::vector<uint16_t> packed;
	packed.reserve(map.texels.size() * 4);
	for (const auto& texel : map.texels)
	{
		packed.push_back(glm::packHalf1x16(texel.flow.x));
		packed.push_back(glm::packHalf1x16(texel.flow.y));
		packed.push_back(glm::packHalf1x16(texel.white));
		packed.push_back(glm::packHalf1x16(texel.distance));
	}
	return packed;
}

float ChannelAt(const Network& network, const Channel& channel, glm::vec2 point)
{
	float alpha = 1.0f;
	for (const auto& segment : network.segments)
	{
		const glm::vec2 line = segment.modelTo - segment.modelFrom;
		const float length = glm::length(line);
		// The model is k_MeshLength long and a little more; nothing past its box can lay its channel here
		if (length <= 0.0f || DistanceAlong(point, segment.modelFrom, segment.modelTo).x > 8.0f)
		{
			continue;
		}
		const glm::vec2 along = line / length;
		const glm::vec2 across {-along.y, along.x};
		const glm::vec2 away = point - segment.modelFrom;
		const float stretch = length / k_MeshLength;
		alpha = std::min(alpha, ChannelAlpha(channel, {glm::dot(away, along) / stretch, glm::dot(away, across)}));
	}
	return alpha;
}

float Loudness(const Network& network, glm::vec2 listener)
{
	// Each stretch sounds from its nearest point, louder for fast and white water, and quieter with the distance
	float sum = 0.0f;
	for (const auto& segment : network.segments)
	{
		const glm::vec2 found = DistanceAlong(listener, segment.from, segment.to);
		const float distance = found.x;
		const float t = found.y;
		const float speed = glm::mix(segment.speedFrom, segment.speedTo, t) / k_MostSpeed;
		const float white = glm::mix(segment.whiteFrom, segment.whiteTo, t);
		const float length = glm::distance(segment.from, segment.to);
		const float source = (0.25f + 0.35f * speed + 0.6f * white) * std::min(length / 15.0f, 1.0f);
		sum += source * 30.0f / (30.0f + distance * distance / 10.0f);
	}
	return std::clamp(sum * 0.5f, 0.0f, 1.0f);
}

glm::vec2 DriftVelocity(const FlowTexel& texel, glm::vec2 towardsMiddle)
{
	// A drifter near the bank is pushed back towards the middle, as the faster water in the middle pulls it
	const float bank = std::clamp((texel.distance - 0.3f) / 0.6f, 0.0f, 1.0f);
	const float speed = glm::length(texel.flow);
	return texel.flow * (1.0f - 0.4f * bank) + towardsMiddle * bank * std::max(speed, k_LeastSpeed) * 0.8f;
}

float DepthForAlpha(float alpha)
{
	// The original's channel leaves two thirds of the land's alpha at its middle: that is the deepest water
	const float t = std::clamp((1.0f - alpha) / (1.0f - 2.0f / 3.0f), 0.0f, 1.0f);
	return k_MostDepth * std::sqrt(t);
}

} // namespace openblack::graphics::underwater::streams
