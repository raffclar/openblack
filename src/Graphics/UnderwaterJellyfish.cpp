/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "UnderwaterJellyfish.h"

#include <cmath>

#include <algorithm>
#include <utility>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include "Graphics/Underwater.h"

namespace openblack::graphics::underwater::jellyfish
{

namespace
{

/// A number 0 to 1 from a cell and a salt, the same each time
float Hash(int32_t x, int32_t z, uint32_t salt)
{
	auto h = static_cast<uint32_t>(x) * 0x8DA6B343u ^ static_cast<uint32_t>(z) * 0xD8163841u ^ salt * 0xCB1AB31Fu;
	h ^= h >> 13u;
	h *= 0x5BD1E995u;
	h ^= h >> 15u;
	return static_cast<float>(h & 0xFFFFFFu) / static_cast<float>(0x1000000u);
}

float SmoothStep(float from, float to, float x)
{
	const float t = std::clamp((x - from) / (to - from), 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

} // namespace

std::vector<Jellyfish> Around(glm::vec2 eye, float seconds, float seaLevel, const DepthAt& depthAt)
{
	std::vector<Jellyfish> found;
	const auto reach = static_cast<int32_t>(std::ceil(k_Range / k_Cell));
	const auto centre = glm::ivec2(glm::floor(eye / k_Cell));
	for (int32_t dz = -reach; dz <= reach; ++dz)
	{
		for (int32_t dx = -reach; dx <= reach; ++dx)
		{
			const glm::ivec2 cell = centre + glm::ivec2(dx, dz);
			if (Hash(cell.x, cell.y, 1u) >= k_Share)
			{
				continue;
			}
			// A place in the cell, kept from its edges so that two in cells side by side do not touch
			const glm::vec2 offset {0.15f + 0.7f * Hash(cell.x, cell.y, 2u), 0.15f + 0.7f * Hash(cell.x, cell.y, 3u)};
			const glm::vec2 at = (glm::vec2(cell) + offset) * k_Cell;
			const float distance = glm::distance(at, eye);
			if (distance > k_Range)
			{
				continue;
			}
			const float depth = depthAt(at);
			if (depth < k_LeastDepth)
			{
				continue;
			}
			const float size = k_LeastSize + (k_MostSize - k_LeastSize) * Hash(cell.x, cell.y, 4u);
			// The water column it rises through, from over the sea bed to its least depth, and where it is in it now
			const float column = std::max(depth - k_BedGap - k_TopDepth - size, 0.5f);
			const float seed = Hash(cell.x, cell.y, 5u);
			const float risen = seed + seconds * k_Rise / column;
			const float along = risen - std::floor(risen);
			const float shows = SmoothStep(0.0f, 0.12f, along) * (1.0f - SmoothStep(0.85f, 1.0f, along)) *
			                    (1.0f - SmoothStep(k_Range * (1.0f - k_FadeShare), k_Range, distance)) *
			                    SmoothStep(k_LeastDepth, k_LeastDepth + k_DepthFade, depth);
			if (shows <= 0.0f)
			{
				continue;
			}
			const float top = seaLevel - depth + k_BedGap + size + along * column;
			found.push_back({.position = {at.x, top, at.y},
			                 .size = size,
			                 .phase = Hash(cell.x, cell.y, 6u),
			                 .hue = Hash(cell.x, cell.y, 7u),
			                 .shows = shows,
			                 .seed = seed,
			                 .id = static_cast<uint32_t>(cell.x) * 0x9E3779B1u ^ static_cast<uint32_t>(cell.y) * 0x85EBCA77u});
		}
	}
	std::ranges::sort(found, [eye](const Jellyfish& a, const Jellyfish& b) {
		return glm::distance(glm::vec2(a.position.x, a.position.z), eye) <
		       glm::distance(glm::vec2(b.position.x, b.position.z), eye);
	});
	if (found.size() > k_Most)
	{
		found.resize(k_Most);
	}
	return found;
}

std::array<glm::vec4, 3> InstanceData(const Jellyfish& jellyfish, float flash, int32_t light)
{
	return {glm::vec4(jellyfish.position, jellyfish.size),
	        glm::vec4(jellyfish.phase, jellyfish.hue, jellyfish.shows, jellyfish.seed),
	        glm::vec4(flash, static_cast<float>(light), 0.0f, 0.0f)};
}

float BeatCycle(const Jellyfish& jellyfish, float seconds)
{
	const float beat = seconds * (0.45f + 0.25f * jellyfish.seed) + jellyfish.phase;
	return beat - std::floor(beat);
}

float Squeeze(float cycle)
{
	return SmoothStep(0.0f, 0.16f, cycle) * (1.0f - SmoothStep(0.16f, 0.9f, cycle));
}

glm::vec3 Middle(const Jellyfish& jellyfish)
{
	// The bell is about three quarters of its size high: its light comes from a little under its top
	return jellyfish.position - glm::vec3(0.0f, 0.3f * jellyfish.size, 0.0f);
}

glm::vec3 LightColour(float hue)
{
	return glm::mix(k_LightColour, k_GreenColour, k_MostGreen * std::clamp(hue, 0.0f, 1.0f));
}

float Emission(float squeeze, float flash)
{
	const float rest = k_RestLight * (0.75f + 0.5f * std::clamp(squeeze, 0.0f, 1.0f));
	return std::min(rest + k_FlashLight * std::clamp(flash, 0.0f, 1.0f), k_MostGlow);
}

glm::vec3 LightAt(float distance)
{
	const float d = std::max(distance, 0.0f);
	constexpr float k_Spread = k_LightRadius * k_LightRadius;
	return GlowThroughWater(d) * (k_Spread / (d * d + k_Spread));
}

Light LightOf(const Jellyfish& jellyfish, float seconds, float flash)
{
	const float strength = Emission(Squeeze(BeatCycle(jellyfish, seconds)), flash) * jellyfish.shows;
	return {.position = Middle(jellyfish), .colour = LightColour(jellyfish.hue) * strength};
}

float Stir(std::span<const Touch> touches)
{
	float stir = 0.0f;
	for (const auto& touch : touches)
	{
		if (touch.since < 0.0f || touch.since > k_StirWindow || touch.reach <= 0.0f)
		{
			continue;
		}
		stir = std::max(stir, 1.0f - SmoothStep(touch.reach / 3.0f, touch.reach, touch.distance));
	}
	return stir;
}

float StepFlash(float flash, float stir, float step)
{
	const float dt = std::max(step, 0.0f);
	if (stir > flash)
	{
		return flash + (stir - flash) * (1.0f - std::exp(-dt / k_FlashRise));
	}
	// It goes down to the stir, not under it
	return stir + (flash - stir) * std::exp(-dt / k_FlashFade);
}

void Flashes::Update(std::span<const Jellyfish> jellyfish, std::span<const Disturber> disturbers, float seconds)
{
	// A long step (a pause or a jump of the clock) is one short step
	constexpr float k_LongestStep = 0.1f;
	const float step = _seconds < 0.0f ? 0.0f : std::clamp(seconds - _seconds, 0.0f, k_LongestStep);
	_seconds = seconds;
	std::unordered_map<uint32_t, float> next;
	std::vector<Touch> touches(disturbers.size());
	for (const auto& one : jellyfish)
	{
		const glm::vec3 middle = Middle(one);
		std::ranges::transform(disturbers, touches.begin(), [&middle](const Disturber& disturber) {
			return Touch {
			    .distance = glm::distance(disturber.position, middle), .reach = disturber.reach, .since = disturber.since};
		});
		const float flash = StepFlash(Of(one.id), Stir(touches), step);
		// Less than this shows as nothing
		constexpr float k_Least = 1e-3f;
		if (flash > k_Least)
		{
			next.emplace(one.id, flash);
		}
	}
	_flashes = std::move(next);
}

float Flashes::Of(uint32_t id) const
{
	const auto found = _flashes.find(id);
	return found != _flashes.end() ? found->second : 0.0f;
}

void Mesh(std::vector<Vertex>& vertices, std::vector<uint16_t>& indices)
{
	vertices.clear();
	indices.clear();
	const auto shell = [&](Part part) {
		const auto first = static_cast<uint16_t>(vertices.size());
		for (uint32_t ring = 0; ring <= k_ShellRings; ++ring)
		{
			for (uint32_t side = 0; side <= k_ShellSides; ++side)
			{
				vertices.push_back(
				    {.data = {static_cast<float>(side) / static_cast<float>(k_ShellSides),
				              static_cast<float>(ring) / static_cast<float>(k_ShellRings), static_cast<float>(part), 0.0f}});
			}
		}
		constexpr uint32_t k_Row = k_ShellSides + 1;
		for (uint32_t ring = 0; ring < k_ShellRings; ++ring)
		{
			for (uint32_t side = 0; side < k_ShellSides; ++side)
			{
				const auto a = static_cast<uint16_t>(first + ring * k_Row + side);
				const auto b = static_cast<uint16_t>(a + 1);
				const auto c = static_cast<uint16_t>(a + k_Row);
				const auto d = static_cast<uint16_t>(c + 1);
				indices.insert(indices.end(), {a, c, b, b, c, d});
			}
		}
	};
	const auto tentacles = [&](Part part, uint32_t count) {
		for (uint32_t which = 0; which < count; ++which)
		{
			const auto first = static_cast<uint16_t>(vertices.size());
			for (uint32_t segment = 0; segment <= k_TentacleSegments; ++segment)
			{
				const float along = static_cast<float>(segment) / static_cast<float>(k_TentacleSegments);
				for (const float side : {-1.0f, 1.0f})
				{
					vertices.push_back({.data = {along, side, static_cast<float>(part), static_cast<float>(which)}});
				}
			}
			for (uint32_t segment = 0; segment < k_TentacleSegments; ++segment)
			{
				const auto a = static_cast<uint16_t>(first + segment * 2);
				indices.insert(indices.end(),
				               {a, static_cast<uint16_t>(a + 2), static_cast<uint16_t>(a + 1), static_cast<uint16_t>(a + 1),
				                static_cast<uint16_t>(a + 2), static_cast<uint16_t>(a + 3)});
			}
		}
	};
	shell(Part::InnerShell);
	tentacles(Part::InnerTentacle, k_InnerTentacles);
	tentacles(Part::OuterTentacle, k_OuterTentacles);
	// The halo: a square of four corners round the middle of the bell
	const auto halo = static_cast<uint16_t>(vertices.size());
	for (const float y : {-1.0f, 1.0f})
	{
		for (const float x : {-1.0f, 1.0f})
		{
			vertices.push_back({.data = {x, y, static_cast<float>(Part::Halo), 0.0f}});
		}
	}
	indices.insert(indices.end(),
	               {halo, static_cast<uint16_t>(halo + 2), static_cast<uint16_t>(halo + 1), static_cast<uint16_t>(halo + 1),
	                static_cast<uint16_t>(halo + 2), static_cast<uint16_t>(halo + 3)});
	shell(Part::OuterShell);
}

float WaterPath(glm::vec3 eye, glm::vec3 point, float seaLevel)
{
	const float length = glm::distance(eye, point);
	if (eye.y <= seaLevel)
	{
		return length;
	}
	// The part of the straight line from the eye that is under the sea's level
	const float depth = std::max(seaLevel - point.y, 0.0f);
	const float drop = eye.y - point.y;
	return drop > 0.0f ? length * std::min(depth / drop, 1.0f) : 0.0f;
}

glm::vec3 GlowThroughWater(float path)
{
	return glm::exp(-(k_Absorption + glm::vec3(k_GlowScatter)) * std::max(path, 0.0f));
}

} // namespace openblack::graphics::underwater::jellyfish
