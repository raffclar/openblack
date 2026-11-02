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
#include <ranges>
#include <unordered_map>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>

#include "3D/LowResolutionTextures.h"

namespace openblack::TempleExteriorMorph
{

namespace
{
/// The size's index stops short of the largest
constexpr float k_SizeShort = 0.9999f;
/// From neutral the texture goes halfway either way: twice 255 a whole
constexpr float k_TextureWeight = 510.0f;
} // namespace

bool NeedsBlend(std::optional<glm::vec2> blendedFor, glm::vec2 look, bool meshLoaded)
{
	return !meshLoaded || blendedFor != look;
}

bool BlendsAfterLoad(std::optional<glm::vec2> blendedFor, std::optional<Blend> inCache, Blend wanted)
{
	return blendedFor.has_value() && inCache != wanted;
}

std::vector<std::optional<size_t>> MeshesToKeep(std::span<const MeshChange> changes, size_t groups,
                                                std::span<const size_t> others)
{
	// The number of temples that have each mesh and do not have their mesh of the call yet
	std::unordered_map<size_t, size_t> holders;
	for (const auto& change : changes)
	{
		if (change.mesh.has_value())
		{
			++holders[*change.mesh];
		}
	}
	for (const auto mesh : others)
	{
		++holders[mesh];
	}
	std::vector<std::optional<size_t>> kept(groups);
	for (size_t group = 0; group < groups; ++group)
	{
		const auto inGroup = [group](const MeshChange& change) { return change.group == group && change.mesh.has_value(); };
		for (const auto& change : changes | std::views::filter(inGroup))
		{
			const auto mesh = *change.mesh;
			const auto sharers = std::ranges::count_if(
			    changes, [&inGroup, mesh](const MeshChange& other) { return inGroup(other) && *other.mesh == mesh; });
			if (holders[mesh] == static_cast<size_t>(sharers))
			{
				kept[group] = mesh;
				break;
			}
		}
		for (const auto& change : changes | std::views::filter(inGroup))
		{
			--holders[*change.mesh];
		}
	}
	return kept;
}

float Step(float current, float target)
{
	if (std::abs(current - target) <= k_Near)
	{
		return target;
	}
	return current < target ? std::min(current + k_Step, target) : std::max(current - k_Step, target);
}

float SizeTarget(int32_t landNumber, std::optional<float> share)
{
	constexpr int32_t k_FirstLand = 1;
	return 2.0f * (landNumber == k_FirstLand || !share.has_value() ? k_ShareOfNoInfluence : *share);
}

std::array<Corner, 4> Corners(float size, float alignment)
{
	// Each the index below and the one after, as far as there are, and how much of the one below
	const auto sizeBelow = static_cast<int32_t>(2.0f * std::min(size, k_SizeShort));
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

std::string OwnMeshName(uint32_t temple)
{
	return fmt::format("temple/exterior/{}", temple);
}

std::string SiteMeshName(uint32_t templeMesh)
{
	return fmt::format("temple/worship/{}", templeMesh);
}

std::array<uint32_t, 2> MeshesMadeFor(uint32_t templeMesh)
{
	return {templeMesh, entt::hashed_string(SiteMeshName(templeMesh).c_str()).value()};
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

std::optional<std::vector<uint16_t>> SkinTexels(std::span<const uint16_t> from, std::span<const uint16_t> to, uint8_t weight,
                                                bool halfSize)
{
	constexpr auto k_Texels = static_cast<size_t>(low_resolution_textures::k_FullSide) * low_resolution_textures::k_FullSide;
	if (from.size() != k_Texels || to.size() != k_Texels)
	{
		return std::nullopt;
	}
	std::optional<std::vector<uint16_t>> halfFrom;
	std::optional<std::vector<uint16_t>> halfTo;
	if (halfSize)
	{
		halfFrom = low_resolution_textures::HalveTexels(from);
		halfTo = low_resolution_textures::HalveTexels(to);
		if (!halfFrom.has_value() || !halfTo.has_value())
		{
			return std::nullopt;
		}
		from = *halfFrom;
		to = *halfTo;
	}
	std::vector<uint16_t> blended(from.size());
	BlendTexels(from, to, weight, blended);
	return blended;
}

} // namespace openblack::TempleExteriorMorph
