/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleExteriorMorph.h"

#include <cassert>
#include <cmath>

#include <algorithm>

#include <L3DFile.h>
#include <fmt/format.h>

namespace openblack::TempleExteriorMorph
{

namespace
{
/// The size's index stops short of the largest, and the alignment's target short of 1
constexpr float k_JustShortOfOne = 0.9999f;
/// From neutral the texture goes halfway either way: twice 255 a whole
constexpr float k_TextureWeight = 510.0f;
/// The share of the influence the game gives a player on the first land, and when nobody has any
constexpr float k_SmallShare = 0.01f;

/// Either value more than k_Changed from the one last blended
bool MovedSinceBlend(const State& state)
{
	return std::abs(state.blendedAlignment - state.alignment) > k_Changed ||
	       std::abs(state.blendedSize - state.size) > k_Changed;
}
} // namespace

float Step(float current, float target)
{
	if (std::abs(current - target) <= k_Near)
	{
		return target;
	}
	return current < target ? std::min(current + k_Step, target) : std::max(current - k_Step, target);
}

float AlignmentTarget(float playerAlignment)
{
	const float target = (playerAlignment + 1.0f) * 0.5f;
	// Below 0 it is 0, and from 1 up just short of 1
	if (target < 0.0f)
	{
		return 0.0f;
	}
	return target < 1.0f ? target : k_JustShortOfOne;
}

float SizeTarget(bool firstLand, float ownPower, float allPowers)
{
	const float share = firstLand || allPowers == 0.0f ? k_SmallShare : ownPower / allPowers;
	const float target = 2.0f * share;
	if (target < 0.0f)
	{
		return 0.0f;
	}
	return target > 1.0f ? 1.0f : target;
}

bool Turn(State& state, float alignmentTarget, float sizeTarget)
{
	state.alignmentTarget = alignmentTarget;
	state.sizeTarget = sizeTarget;
	state.alignment = Step(state.alignment, state.alignmentTarget);
	state.size = Step(state.size, state.sizeTarget);
	return MovedSinceBlend(state);
}

bool NeedsBlend(const State& state)
{
	return state.neverBlended || MovedSinceBlend(state);
}

void Blended(State& state)
{
	state.blendedAlignment = state.alignment;
	state.blendedSize = state.size;
	state.neverBlended = false;
}

std::array<Corner, 4> Corners(float size, float alignment)
{
	// Each the index below and the one after, as far as there are, and how much of the one below
	const auto sizeBelow = static_cast<int32_t>(2.0f * std::min(size, k_JustShortOfOne));
	const auto sizeLow = static_cast<uint32_t>(std::clamp(sizeBelow, 0, static_cast<int32_t>(k_Sizes) - 1));
	const auto sizeHigh = static_cast<uint32_t>(std::clamp(sizeBelow + 1, 0, static_cast<int32_t>(k_Sizes) - 1));
	const float sizeWeight = std::clamp(static_cast<float>(sizeHigh) - (2.0f * size), 0.0f, 1.0f);

	const auto stageBelow = static_cast<int32_t>(4.0f * alignment);
	const auto stageLow = static_cast<uint32_t>(std::clamp(stageBelow, 0, static_cast<int32_t>(k_Stages) - 1));
	const auto stageHigh = static_cast<uint32_t>(std::clamp(stageBelow + 1, 0, static_cast<int32_t>(k_Stages) - 1));
	const float stageWeight = static_cast<float>(stageHigh) - (4.0f * alignment);

	return {{
	    {sizeLow, stageLow, sizeWeight * stageWeight},
	    {sizeHigh, stageLow, (1.0f - sizeWeight) * stageWeight},
	    {sizeLow, stageHigh, sizeWeight * (1.0f - stageWeight)},
	    {sizeHigh, stageHigh, (1.0f - sizeWeight) * (1.0f - stageWeight)},
	}};
}

std::string MeshName(uint32_t size, uint32_t stage)
{
	return fmt::format("b_temple{}{}_l3d", size, stage);
}

bool BlendVertices(const std::array<Corner, 4>& corners, const VerticesOf& verticesOf, std::span<l3d::L3DVertex> blended)
{
	std::ranges::fill(blended, l3d::L3DVertex {});
	for (const auto& corner : corners)
	{
		if (corner.weight == 0.0f)
		{
			continue;
		}
		const auto* source = verticesOf(corner.size, corner.stage);
		if (source == nullptr || source->size() != blended.size())
		{
			return false;
		}
		const float w = corner.weight;
		for (size_t i = 0; i < blended.size(); ++i)
		{
			const auto& from = (*source)[i];
			auto& to = blended[i];
			to.position.x += from.position.x * w;
			to.position.y += from.position.y * w;
			to.position.z += from.position.z * w;
			to.texCoord.x += from.texCoord.x * w;
			to.texCoord.y += from.texCoord.y * w;
			to.normal.x += from.normal.x * w;
			to.normal.y += from.normal.y * w;
			to.normal.z += from.normal.z * w;
		}
	}
	return true;
}

void BakeToLand(const land_morph::Ground& ground, const glm::mat4& object, std::span<l3d::L3DVertex> vertices)
{
	std::vector<glm::vec3> positions(vertices.size());
	std::ranges::transform(vertices, positions.begin(),
	                       [](const l3d::L3DVertex& v) { return glm::vec3(v.position.x, v.position.y, v.position.z); });
	land_morph::BakeAgainstY(ground, object, positions);
	for (size_t i = 0; i < vertices.size(); ++i)
	{
		vertices[i].position.y = positions[i].y;
	}
}

TextureBlend TextureOf(float alignment)
{
	const bool good = alignment > 0.5f;
	const float along = good ? alignment - 0.5f : alignment;
	const auto weight = std::clamp(static_cast<int32_t>(along * k_TextureWeight), 0, 255);
	return {
	    .from = good ? Look::Neutral : Look::Evil,
	    .to = good ? Look::Good : Look::Neutral,
	    .weight = static_cast<uint8_t>(weight),
	};
}

std::string ImageName(Look look, uint32_t set)
{
	constexpr std::array<std::string_view, 3> k_Looks {"evil", "neutral", "good"};
	return fmt::format("{}{}", k_Looks.at(static_cast<size_t>(look)), set);
}

void BlendTexels(std::span<const uint16_t> from, std::span<const uint16_t> to, uint8_t weight, std::span<uint16_t> blended)
{
	assert(from.size() == to.size() && to.size() == blended.size());
	const uint32_t toWeight = weight;
	const uint32_t fromWeight = 255 - toWeight;
	// Each channel in its place, the two by their weights over 255, rounded down
	const auto channel = [toWeight, fromWeight](uint32_t a, uint32_t b, uint32_t mask) {
		return ((((b & mask) * toWeight) + ((a & mask) * fromWeight)) / 255) & mask;
	};
	for (size_t i = 0; i < blended.size(); ++i)
	{
		const uint32_t a = from[i];
		const uint32_t b = to[i];
		blended[i] = static_cast<uint16_t>(channel(a, b, 0x00F) | channel(a, b, 0x0F0) | channel(a, b, 0xF00) | (a & 0xF000));
	}
}

} // namespace openblack::TempleExteriorMorph
