/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BlockTexture.h"

#include <algorithm>

#include <LNDFile.h>
#include <glm/common.hpp>

#include "3D/CoastAlpha.h"
#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "Graphics/Argb4444.h"

using namespace openblack;

namespace
{
/// A cell of open sea, which isn't drawn: its texels are left clear
constexpr uint8_t k_OpenSeaFlag = 0x02;
constexpr size_t k_MapTexels = static_cast<size_t>(lnd::LNDMaterial::k_Width) * lnd::LNDMaterial::k_Height;

uint16_t MaterialTexel(const block_texture::Materials& materials, uint32_t material, size_t texelIndex)
{
	if (material >= materials.count || texelIndex >= k_MapTexels)
	{
		return 0;
	}
	return materials.texels[material * k_MapTexels + texelIndex];
}
} // namespace

uint16_t block_texture::CountryTexel(const lnd::LNDCountry& country, int32_t h, uint8_t noise, uint8_t bump,
                                     const Materials& materials, size_t texelIndex)
{
	if (h < 0x100)
	{
		return 0;
	}
	// The country's materials for the height, lifted by the noise
	const auto level = static_cast<size_t>(std::min((h >> 8) + static_cast<int32_t>(noise), 255));
	const auto& entry = country.materials.at(level);
	const uint32_t b = bump;
	const uint32_t first = MaterialTexel(materials, entry.indices[0], texelIndex);
	uint32_t red;
	uint32_t green;
	uint32_t blue;
	if (entry.indices[0] == entry.indices[1])
	{
		// The 5-bit channels shaded by the bump map into the top 4 bits of their nibbles
		red = ((first & 0x7C00u) * b) >> 10;
		green = ((first & 0x3E0u) * b) >> 9;
		blue = ((first & 0x1Fu) * b) >> 8;
	}
	else
	{
		// The first material weighs the coefficient out of 256, the second the rest, in 32-bit products
		const uint32_t second = MaterialTexel(materials, entry.indices[1], texelIndex);
		const uint32_t k0 = entry.coefficient;
		const uint32_t k1 = 0x100u - k0;
		red = (((first & 0x7C00u) * k0 + (second & 0x7C00u) * k1) * b) >> 18;
		green = (((first & 0x3E0u) * k0 + (second & 0x3E0u) * k1) * b) >> 17;
		blue = (((first & 0x1Fu) * k0 + (second & 0x1Fu) * k1) * b) >> 16;
	}
	// A bright bump saturates each channel
	red = red > 0xF00u ? 0xF00u : (red & 0xF00u);
	green = green > 0xF0u ? 0xF0u : (green & 0xF0u);
	blue = blue > 0xFu ? 0xFu : (blue & 0xFu);
	const uint32_t alpha = static_cast<uint32_t>(coast::CoastAlphaNibble(h, noise)) << 12;
	return static_cast<uint16_t>(alpha | red | green | blue);
}

uint16_t block_texture::BlendCorners(const std::array<uint16_t, 4>& texels, const std::array<uint8_t, 4>& weights)
{
	uint32_t out = 0;
	for (const uint32_t mask : {0xF00u, 0xF0u, 0xFu, 0xF000u})
	{
		uint32_t sum = 0;
		for (size_t k = 0; k < texels.size(); ++k)
		{
			sum += (texels.at(k) & mask) * weights.at(k);
		}
		out |= (sum / 255u) & mask;
	}
	return static_cast<uint16_t>(out);
}

block_texture::TexelBox block_texture::TexelsOf(const CellBox& box, uint16_t texelsPerBlock)
{
	// texel t of the block lies in cell (t x 256 / texelsPerBlock) / 16: the first texel of each end of the box
	const auto firstTexelOf = [texelsPerBlock](int cell) {
		const int originalTexel = cell * coast::k_TexelsPerCell;
		return (originalTexel * texelsPerBlock + coast::k_TexelsPerBlock - 1) / coast::k_TexelsPerBlock;
	};
	const glm::ivec2 low = glm::clamp(box.min, 0, k_CellsPerBlock - 1);
	const glm::ivec2 high = glm::clamp(box.max, 0, k_CellsPerBlock - 1);
	return {
	    .min = {firstTexelOf(low.x), firstTexelOf(low.y)},
	    .max = {firstTexelOf(high.x + 1) - 1, firstTexelOf(high.y + 1) - 1},
	};
}

void block_texture::BuildBlock(std::span<const lnd::LNDCell> cells, const Sources& sources, const Target& target,
                               const CellBox& box)
{
	if (cells.size() < static_cast<size_t>(k_CellsPerSide) * k_CellsPerSide || target.texelsPerBlock == 0)
	{
		return;
	}
	constexpr size_t k_MapSize = static_cast<size_t>(coast::k_TexelsPerBlock) * coast::k_TexelsPerBlock;
	const auto& countries = sources.countries;
	const auto& coneWeights = coast::GetConeWeights();
	const auto countryOf = [&countries](const lnd::LNDCell& cell) -> const lnd::LNDCountry& {
		return countries[std::min<size_t>(cell.properties.country, countries.size() - 1)];
	};
	const auto texels = TexelsOf(box, target.texelsPerBlock);
	for (int tx = texels.min.x; tx <= texels.max.x; ++tx)
	{
		// the original's texel under this one (the same one with 256 texels per block)
		const int ox = tx * coast::k_TexelsPerBlock / target.texelsPerBlock;
		const int cx = ox / coast::k_TexelsPerCell;
		const int i = ox % coast::k_TexelsPerCell;
		for (int tz = texels.min.y; tz <= texels.max.y; ++tz)
		{
			const int oz = tz * coast::k_TexelsPerBlock / target.texelsPerBlock;
			const int cz = oz / coast::k_TexelsPerCell;
			const int j = oz % coast::k_TexelsPerCell;
			// the block's own 17 x 17 cells: +1 along z, +17 along x; corners (x, z), (x, z + 1), (x + 1, z + 1),
			// (x + 1, z) like the weights
			const auto base = static_cast<size_t>(cx) * k_CellsPerSide + static_cast<size_t>(cz);
			const std::array<const lnd::LNDCell*, 4> corners = {
			    &cells[base], &cells[base + 1], &cells[base + k_CellsPerSide + 1], &cells[base + k_CellsPerSide]};
			uint16_t texel = 0;
			if ((corners[0]->flags & k_OpenSeaFlag) == 0 && !countries.empty())
			{
				const auto& weights = coneWeights.at(static_cast<size_t>(i * coast::k_TexelsPerCell + j));
				int32_t h = 0;
				for (size_t k = 0; k < corners.size(); ++k)
				{
					h += static_cast<int32_t>(weights.at(k)) * corners.at(k)->Altitude(sources.altitudeBits);
				}
				const size_t index = static_cast<size_t>(ox) * coast::k_TexelsPerBlock + static_cast<size_t>(oz);
				const uint8_t noise = sources.noise.size() >= k_MapSize ? sources.noise[index] : uint8_t {0};
				const uint8_t bump = sources.bump.size() >= k_MapSize ? sources.bump[index] : uint8_t {0x80};
				// one build when the four corners share the country, else one per corner country blended by
				// BlendCorners (the same result for equal ones, the weights add up to 255)
				const auto own = corners[0]->properties.country;
				const bool single = std::all_of(corners.begin(), corners.end(),
				                                [own](const lnd::LNDCell* c) { return c->properties.country == own; });
				if (single)
				{
					texel = CountryTexel(countryOf(*corners[0]), h, noise, bump, sources.materials, index);
				}
				else
				{
					std::array<uint16_t, 4> painted {};
					for (size_t k = 0; k < corners.size(); ++k)
					{
						painted.at(k) = CountryTexel(countryOf(*corners.at(k)), h, noise, bump, sources.materials, index);
					}
					texel = BlendCorners(painted, weights);
				}
			}
			const auto row = static_cast<size_t>(target.origin.y + tz);
			const auto column = static_cast<size_t>(target.origin.x + tx);
			std::ranges::copy(graphics::argb4444::Unpack(texel),
			                  target.rgba.subspan((row * target.width + column) * 4, 4).begin());
		}
	}
}

block_texture::CellBox block_texture::CellsOfCorner(glm::ivec2 corner)
{
	return {
	    .min = glm::clamp(corner - 1, 0, k_CellsPerBlock - 1),
	    .max = glm::clamp(corner, 0, k_CellsPerBlock - 1),
	};
}

block_texture::CellBox block_texture::Union(const CellBox& a, const CellBox& b)
{
	return {.min = glm::min(a.min, b.min), .max = glm::max(a.max, b.max)};
}

block_texture::Patch block_texture::PaintBox(std::span<const lnd::LNDCell> cells, const Sources& sources,
                                             uint16_t texelsPerBlock, const CellBox& box)
{
	Patch patch {.texels = TexelsOf(box, texelsPerBlock), .rgba = {}};
	if (patch.texels.Empty())
	{
		return patch;
	}
	const glm::ivec2 size = patch.texels.max - patch.texels.min + 1;
	patch.rgba.assign(static_cast<size_t>(size.x) * static_cast<size_t>(size.y) * 4, 0);
	// the block's first texel sits before the patch's, so that the box's texels land at its start
	const Target target {
	    .rgba = patch.rgba,
	    .width = static_cast<size_t>(size.x),
	    .origin = -patch.texels.min,
	    .texelsPerBlock = texelsPerBlock,
	};
	BuildBlock(cells, sources, target, box);
	return patch;
}

std::vector<uint8_t> block_texture::BuildIslandBlockTexture(const LandIslandInterface& island, glm::u16vec2 extentIndexMin,
                                                            glm::u16vec2 indexSize, uint16_t texelsPerBlock,
                                                            std::span<const uint8_t> noise, std::span<const uint8_t> bump,
                                                            const Materials& materials)
{
	const size_t width = static_cast<size_t>(indexSize.x) * texelsPerBlock;
	const size_t height = static_cast<size_t>(indexSize.y) * texelsPerBlock;
	std::vector<uint8_t> texels(width * height * 4, 0);
	const Sources sources {
	    .countries = island.GetCountries(),
	    .materials = materials,
	    .noise = noise,
	    .bump = bump,
	    .altitudeBits = island.GetAltitudeBits(),
	};
	for (const auto& block : island.GetBlocks())
	{
		const auto* cells = block.GetCells();
		const auto position = block.GetBlockPosition() - glm::ivec2(extentIndexMin);
		if (cells == nullptr || position.x < 0 || position.y < 0 || position.x >= indexSize.x || position.y >= indexSize.y)
		{
			continue;
		}
		const Target target {
		    .rgba = texels,
		    .width = width,
		    .origin = position * static_cast<int>(texelsPerBlock),
		    .texelsPerBlock = texelsPerBlock,
		};
		BuildBlock(std::span(cells, static_cast<size_t>(k_CellsPerSide) * k_CellsPerSide), sources, target);
	}
	return texels;
}
