/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the rules of the volumetric fog

#include "VolumetricFog.h"

#include <cmath>

#include <algorithm>
#include <ranges>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

using namespace openblack;
using namespace openblack::graphics;
using namespace openblack::graphics::volumetric_fog;

namespace
{
float Smoothstep(float low, float high, float x)
{
	const float t = std::clamp((x - low) / (high - low), 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}
} // namespace

float volumetric_fog::SliceDistance(float slice)
{
	return k_Nearest * std::pow(k_Farthest / k_Nearest, (slice + 0.5f) / static_cast<float>(k_Slices));
}

float volumetric_fog::SliceAt(float distance)
{
	return std::log(std::max(distance, 1e-3f) / k_Nearest) / std::log(k_Farthest / k_Nearest) * static_cast<float>(k_Slices) -
	       0.5f;
}

float volumetric_fog::DawnFog(float hour)
{
	const float h = std::fmod(std::fmod(hour, 24.0f) + 24.0f, 24.0f);
	// It builds through the night, is thickest round sunrise, and burns off through the morning
	const float night = 0.35f * Smoothstep(21.0f, 24.0f, h) + 0.35f * (1.0f - Smoothstep(0.0f, 0.5f, h));
	const float dawn = Smoothstep(0.0f, 4.5f, h) * (1.0f - Smoothstep(7.0f, 10.5f, h));
	return std::clamp(std::max(night, dawn * (0.35f + 0.65f * Smoothstep(2.0f, 5.0f, h))), 0.0f, 1.0f);
}

float volumetric_fog::FogTop(float hour)
{
	const float h = std::fmod(std::fmod(hour, 24.0f) + 24.0f, 24.0f);
	// Low and flat before sunrise; it lifts through the morning as it thins
	return 14.0f + 46.0f * Smoothstep(6.0f, 10.5f, h);
}

float volumetric_fog::StepWetness(float wetness, float rain, float seconds)
{
	if (seconds <= 0.0f)
	{
		return wetness;
	}
	if (rain > wetness)
	{
		return wetness + (rain - wetness) * (1.0f - std::exp(-seconds / 20.0f));
	}
	return wetness + (std::clamp(rain, 0.0f, 1.0f) - wetness) * (1.0f - std::exp(-seconds / 400.0f));
}

FogState volumetric_fog::FogFor(float hour, float wetness, float storm)
{
	const float calm = 1.0f - std::clamp(storm, 0.0f, 1.0f);
	FogState fog;
	// A thin haze always, denser in a storm's murk
	fog.hazeDensity = 0.00012f + 0.0009f * std::clamp(storm, 0.0f, 1.0f);
	fog.hazeHeight = 160.0f + 260.0f * std::clamp(storm, 0.0f, 1.0f);
	// The ground fog: dawn's and the wet land's, none in a storm
	fog.groundDensity = 0.016f * DawnFog(hour) * (1.0f + 1.5f * std::clamp(wetness, 0.0f, 1.0f)) * calm;
	fog.groundTop = FogTop(hour);
	return fog;
}

float volumetric_fog::PocketDensity(const ecs::components::FogPocket& pocket, glm::vec3 point, float dawnFog)
{
	using Shape = ecs::components::FogPocket::Shape;
	// How far inside the surface the point is (negative outside it)
	float inside = 0.0f;
	if (pocket.shape == Shape::Ball)
	{
		inside = pocket.halfSize.x - glm::length(point - pocket.centre);
	}
	else
	{
		const glm::vec3 q = glm::abs(point - pocket.centre) - pocket.halfSize;
		inside = -std::max({q.x, q.y, q.z});
	}
	const float time = pocket.atDawn ? std::clamp(dawnFog, 0.0f, 1.0f) : 1.0f;
	return pocket.density * time * Smoothstep(0.0f, std::max(pocket.edge, 1e-3f), inside);
}

PocketUniforms volumetric_fog::PackPockets(std::span<const ecs::components::FogPocket> pockets, glm::vec3 eye, float dawnFog)
{
	std::vector<const ecs::components::FogPocket*> nearest;
	nearest.reserve(pockets.size());
	for (const auto& pocket : pockets)
	{
		nearest.push_back(&pocket);
	}
	std::ranges::sort(nearest, {}, [&eye](const auto* pocket) { return glm::length(pocket->centre - eye); });
	PocketUniforms uniforms;
	for (size_t i = 0; i < std::min(nearest.size(), k_MostPockets); ++i)
	{
		const auto& pocket = *nearest[i];
		const float time = pocket.atDawn ? std::clamp(dawnFog, 0.0f, 1.0f) : 1.0f;
		const float shape = pocket.shape == ecs::components::FogPocket::Shape::Box ? 1.0f : 0.0f;
		uniforms.pockets.at(i) = {pocket.centre, shape};
		uniforms.pockets.at(k_MostPockets + i) = {pocket.halfSize, pocket.density * time};
		uniforms.edges.at(i / 4)[static_cast<int>(i % 4)] = std::max(pocket.edge, 1e-3f);
	}
	return uniforms;
}

std::vector<ecs::components::FogPocket> volumetric_fog::StillWaterPockets(std::span<const uint8_t> soundTypes,
                                                                          std::span<const float> heights, int side,
                                                                          float cellSize, int fewest)
{
	constexpr uint8_t k_StillWater = 2;
	constexpr uint8_t k_Swamp = 8;
	constexpr int k_BlockCells = 16;
	std::vector<ecs::components::FogPocket> pockets;
	const auto count = static_cast<size_t>(side) * static_cast<size_t>(side);
	if (side <= 0 || soundTypes.size() < count || heights.size() < count)
	{
		return pockets;
	}
	for (int blockZ = 0; blockZ < side; blockZ += k_BlockCells)
	{
		for (int blockX = 0; blockX < side; blockX += k_BlockCells)
		{
			int found = 0;
			glm::ivec2 low {side, side};
			glm::ivec2 high {-1, -1};
			float lowest = 1e9f;
			float highest = -1e9f;
			for (int z = blockZ; z < std::min(blockZ + k_BlockCells, side); ++z)
			{
				for (int x = blockX; x < std::min(blockX + k_BlockCells, side); ++x)
				{
					const auto i = static_cast<size_t>(z) * static_cast<size_t>(side) + static_cast<size_t>(x);
					if (soundTypes[i] != k_StillWater && soundTypes[i] != k_Swamp)
					{
						continue;
					}
					++found;
					low = glm::min(low, glm::ivec2(x, z));
					high = glm::max(high, glm::ivec2(x, z));
					lowest = std::min(lowest, heights[i]);
					highest = std::max(highest, heights[i]);
				}
			}
			if (found < fewest)
			{
				continue;
			}
			// The box covers the cells and a cell round them, and lies from the lowest ground to 8 units over the highest
			const glm::vec2 from = (glm::vec2(low) - 1.0f) * cellSize;
			const glm::vec2 to = (glm::vec2(high) + 2.0f) * cellSize;
			const float bottom = lowest - 2.0f;
			const float top = highest + 8.0f;
			ecs::components::FogPocket pocket;
			pocket.shape = ecs::components::FogPocket::Shape::Box;
			pocket.centre = {(from.x + to.x) * 0.5f, (bottom + top) * 0.5f, (from.y + to.y) * 0.5f};
			pocket.halfSize = {(to.x - from.x) * 0.5f, (top - bottom) * 0.5f, (to.y - from.y) * 0.5f};
			pocket.density = 0.03f;
			pocket.edge = 4.0f;
			pocket.atDawn = true;
			pockets.push_back(pocket);
		}
	}
	return pockets;
}
