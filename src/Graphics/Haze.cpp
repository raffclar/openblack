/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Haze.h"

#include <cmath>

#include <algorithm>

#include <LNDFile.h>

#include "3D/LandBlock.h"
#include "3D/LandLight.h"
#include "EngineConfig.h"
#include "Graphics/DetailLevel.h"
#include "Locator.h"

namespace openblack::graphics
{

haze::Params haze::FromTable(const LandLightTable::Haze& haze, bool on) noexcept
{
	Params params {
	    .on = on,
	    .nearDistance = haze.nearDistance,
	    .farDistance = haze.farDistance,
	    .range = haze.farDistance - haze.nearDistance,
	};
	// k is an integer
	params.k = static_cast<int>(haze.k);
	params.colour = haze.colour;
	// each float truncated, b in byte 0, g in byte 1, r in byte 2, alpha 0
	params.packed = static_cast<uint32_t>(static_cast<int32_t>(haze.colour.b)) & 0xFFu;
	params.packed |= (static_cast<uint32_t>(static_cast<int32_t>(haze.colour.g)) & 0xFFu) << 8;
	params.packed |= (static_cast<uint32_t>(static_cast<int32_t>(haze.colour.r)) & 0xFFu) << 16;
	return params;
}

haze::Params haze::Frame() noexcept
{
	const bool on = Locator::config::has_value() && GetDetailLevel(Locator::config::value().detailLevel).fog;
	return FromTable(land_light::CurrentTable().GetHaze(), on);
}

float haze::Depth(const glm::mat4& view, const glm::vec3& point) noexcept
{
	return (view * glm::vec4(point, 1.0f)).z;
}

int haze::RoundHalfEven(float value) noexcept
{
	// std::nearbyint rounds in the current mode, to nearest with halves to even by default, as the original's FPU
	return static_cast<int>(std::nearbyint(value));
}

float haze::HazeFraction(const Params& params, float depth) noexcept
{
	const float z = std::min(std::max(depth, params.nearDistance), params.farDistance);
	return (z - params.nearDistance) / params.range;
}

int haze::Factor(const Params& params, float t) noexcept
{
	return 256 - static_cast<int>(static_cast<float>(256 - params.k) * t);
}

uint32_t haze::ScaleDiffuse(uint32_t argb, int f) noexcept
{
	if (static_cast<uint32_t>(f) >= 256u)
	{
		return argb;
	}
	const auto factor = static_cast<uint32_t>(f);
	uint32_t out = argb & 0xFF000000u;
	for (const uint32_t shift : {16u, 8u, 0u})
	{
		out |= ((((argb >> shift) & 0xFFu) * factor) >> 8u) << shift;
	}
	return out;
}

uint32_t haze::Colour(const Params& params, float t) noexcept
{
	const auto channel = [t](float c) { return static_cast<uint32_t>(RoundHalfEven(c * t)) & 0xFFu; };
	return channel(params.colour.r) << 16 | channel(params.colour.g) << 8 | channel(params.colour.b);
}

uint32_t haze::AddSaturated(uint32_t a, uint32_t b) noexcept
{
	uint32_t out = 0;
	for (const uint32_t shift : {16u, 8u, 0u})
	{
		out |= std::min(((a >> shift) & 0xFFu) + ((b >> shift) & 0xFFu), 0xFFu) << shift;
	}
	return out;
}

uint32_t haze::ApplyObject(const Params& params, float depth, uint32_t specular, uint32_t* diffuse) noexcept
{
	// the "Fog" key off -> the specular as it came
	if (!params.on)
	{
		return specular;
	}
	// strictly closer than near -> untouched
	if (depth < params.nearDistance)
	{
		return specular;
	}
	const float t = HazeFraction(params, depth);
	if (diffuse != nullptr)
	{
		*diffuse = ScaleDiffuse(*diffuse, Factor(params, t));
	}
	// alpha 0xFF
	const uint32_t fog = 0xFF000000u | Colour(params, t);
	// a zero specular takes the haze colour as it is
	if (specular == 0)
	{
		return fog;
	}
	// saturated per channel, alpha 0xFF
	return 0xFF000000u | AddSaturated(specular, fog);
}

int haze::BlockClass(const Params& params, const std::array<float, 8>& cornerDepths) noexcept
{
	uint32_t bits = 0;
	for (const float z : cornerDepths)
	{
		// z > near, then z > far
		if (z > params.nearDistance)
		{
			bits |= z > params.farDistance ? 2u : 1u;
		}
	}
	if (!params.on || bits == 0)
	{
		return 0;
	}
	return (bits & 1u) != 0 ? 1 : 2;
}

std::array<glm::vec3, 8> haze::BlockCorners(glm::vec2 mapPosition, float highestAltitude, bool landRef) noexcept
{
	constexpr float k_Half = 80.0f; // half a block's side
	// the centre is the map position + 80; y: h = highest altitude x 0.67, centre h / 2 and half h / 2, or centre 0 and
	// half h with LandRef
	const float height = highestAltitude * 0.67f;
	const float centreY = landRef ? 0.0f : height * 0.5f;
	const float halfY = landRef ? height : height * 0.5f;
	const glm::vec2 centre = mapPosition + glm::vec2(k_Half);
	std::array<glm::vec3, 8> corners {};
	for (size_t corner = 0; corner < corners.size(); ++corner)
	{
		// every sign of the three half sizes once
		corners.at(corner) =
		    glm::vec3(centre.x + ((corner & 1u) != 0 ? k_Half : -k_Half), centreY + ((corner & 2u) != 0 ? halfY : -halfY),
		              centre.y + ((corner & 4u) != 0 ? k_Half : -k_Half));
	}
	return corners;
}

int haze::BlockClassOf(const Params& params, const glm::mat4& view, const LandBlock& block, bool landRef) noexcept
{
	const auto& lnd = block.GetLndBlock();
	const float height = lnd ? static_cast<float>(static_cast<int32_t>(lnd->highestAltitude)) : 0.0f; // as an integer
	const auto corners = BlockCorners(block.GetMapPosition(), height, landRef);
	std::array<float, 8> depths {};
	for (size_t corner = 0; corner < depths.size(); ++corner)
	{
		depths.at(corner) = Depth(view, corners.at(corner));
	}
	return BlockClass(params, depths);
}

void haze::ApplyVertex(const Params& params, int blockClass, float depth, uint32_t& diffuse, uint32_t& specular) noexcept
{
	if (blockClass == 0)
	{
		return; // no haze
	}
	int f = params.k;
	uint32_t fog = params.packed; // class 2, no t
	if (blockClass != 2)
	{
		const float t = HazeFraction(params, depth);
		f = Factor(params, t);
		fog = Colour(params, t); // alpha 0
	}
	// the specular first, its own alpha kept
	specular = specular == 0 ? fog : (specular & 0xFF000000u) | AddSaturated(specular, fog);
	diffuse = ScaleDiffuse(diffuse, f);
}

std::array<glm::vec4, 2> haze::Uniforms(const Params& params) noexcept
{
	return {glm::vec4(params.nearDistance, params.farDistance, static_cast<float>(params.k), params.on ? 1.0f : 0.0f),
	        glm::vec4(params.colour, 0.0f)};
}

} // namespace openblack::graphics
