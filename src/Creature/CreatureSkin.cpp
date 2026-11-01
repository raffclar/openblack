/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureSkin.h"

#include <cassert>
#include <cmath>

#include <algorithm>
#include <array>
#include <string>

using namespace openblack;
using namespace openblack::creature_skin;

namespace
{
/// The axis is scaled to 256 steps before it is capped at the most a weight can be
constexpr float k_WeightSteps = 256.0f;
constexpr std::array<uint32_t, 2> k_Sides {creature_tattoo::k_SkinSize, creature_tattoo::k_LowDetailSkinSize};

/// A place along a side of the skin multiplied by the side, cut towards zero and kept to 32 bits as the game converts
/// it. A value outside the range of a 64-bit integer gives the lowest 32-bit integer.
uint32_t CutTowardsZero(float value)
{
	constexpr float k_Limit = 9223372036854775808.0f;
	if (!(std::abs(value) < k_Limit))
	{
		return 0x80000000u;
	}
	return static_cast<uint32_t>(static_cast<uint64_t>(static_cast<int64_t>(value)));
}

/// The texel along a side: 0 for 0, the last texel for a value that is not lower than the last texel. A value below
/// zero is a very large value here, so it also gives the last texel.
uint32_t TexelAlong(uint32_t cut, uint32_t side)
{
	if (cut == 0)
	{
		return 0;
	}
	return std::min(cut, side - 1);
}
} // namespace

entt::id_type creature_skin::MeshFileId(std::string_view meshName)
{
	const auto name = "creature/skins/" + std::string(meshName);
	return entt::hashed_string(name.c_str()).value();
}

uint8_t creature_skin::BlendWeight(float evilGood)
{
	// Truncated, as the game converts it
	const auto steps = static_cast<int32_t>(std::abs(evilGood) * k_WeightSteps);
	return static_cast<uint8_t>(std::clamp<int32_t>(steps, 0, k_MaxWeight));
}

glm::vec2 creature_skin::SkinPoint(glm::vec2 first, glm::vec2 second, glm::vec2 third, float u, float v)
{
	const auto along = [u, v](float a, float b, float c) {
		const float towardsThird = (c - a) * v;
		const float towardsSecond = (b - a) * u;
		return (towardsThird + towardsSecond) + a;
	};
	return {along(first.x, second.x, third.x), along(first.y, second.y, third.y)};
}

std::optional<glm::u8vec3> creature_skin::ColourAt(std::span<const uint16_t> texels, glm::vec2 point)
{
	const auto side = std::ranges::find_if(k_Sides, [&texels](uint32_t s) { return texels.size() == s * s; });
	if (side == k_Sides.end())
	{
		return std::nullopt;
	}
	const auto width = static_cast<float>(*side);
	const auto x = TexelAlong(CutTowardsZero(point.x * width), *side);
	const auto y = TexelAlong(CutTowardsZero(point.y * width), *side);
	const auto texel = texels[(static_cast<size_t>(y) * *side) + x];
	const auto channel = [texel](uint32_t shift) { return static_cast<uint8_t>(((texel >> shift) & 0xFu) << 4u); };
	return glm::u8vec3(channel(8), channel(4), channel(0));
}

uint16_t creature_skin::BlendTexel(uint16_t base, uint16_t other, uint8_t weight)
{
	uint16_t result = 0;
	for (uint32_t shift = 0; shift < 16; shift += 4)
	{
		const auto from = static_cast<uint8_t>((base >> shift) & 0xFu);
		const auto to = static_cast<uint8_t>((other >> shift) & 0xFu);
		result = static_cast<uint16_t>(result | (BlendChannel(from, to, weight) << shift));
	}
	return result;
}

std::optional<uint32_t> creature_skin::PairedSkin(std::span<const uint32_t> baseSkins, std::span<const uint32_t> variantSkins,
                                                  uint32_t baseSkin)
{
	const auto found = std::ranges::find(baseSkins, baseSkin);
	if (found == baseSkins.end())
	{
		return std::nullopt;
	}
	const auto index = static_cast<size_t>(std::distance(baseSkins.begin(), found));
	if (index >= variantSkins.size())
	{
		return std::nullopt;
	}
	return variantSkins[index];
}

void creature_skin::Compose(std::span<uint16_t> out, std::span<const uint16_t> base, std::span<const uint16_t> variant,
                            uint8_t weight, const Layers& layers)
{
	assert(out.size() == base.size());
	if (variant.size() == base.size() && weight > 0)
	{
		std::ranges::transform(base, variant, out.begin(),
		                       [weight](uint16_t from, uint16_t to) { return BlendTexel(from, to, weight); });
	}
	else
	{
		std::ranges::copy(base, out.begin());
	}
	// The skins are 256 texels along each side, or 128 at the two lowest detail levels
	const auto side = std::ranges::find_if(
	    k_Sides, [&out](uint32_t candidate) { return out.size() == static_cast<size_t>(candidate) * candidate; });
	if (layers.art == nullptr || side == k_Sides.end())
	{
		return;
	}
	if (layers.sites.has_value() && layers.masks != nullptr)
	{
		// A later slot paints over an earlier slot
		for (size_t i = 0; i < layers.tattoos.size(); ++i)
		{
			const auto& slot = layers.tattoos.at(i);
			const auto& mask = layers.masks->at(i);
			if (slot.Empty() || !mask.has_value())
			{
				continue;
			}
			const auto& site = layers.sites->at(slot.site);
			if (!site.enabled || site.skin != layers.skinIndex)
			{
				continue;
			}
			creature_tattoo::Paint(out, *side, *mask, slot.colour, site);
		}
	}
	for (const auto& wound : layers.marks.wounds)
	{
		if (wound.skin == layers.skinIndex)
		{
			creature_marks::PaintWound(out, *side, layers.art->damage, wound);
		}
	}
	for (const auto& blood : layers.marks.blood)
	{
		if (blood.skin == layers.skinIndex)
		{
			creature_marks::PaintBlood(out, *side, blood);
		}
	}
}
