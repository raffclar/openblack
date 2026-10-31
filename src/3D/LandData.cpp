/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandData.h"

#include <cstring>

#include <algorithm>
#include <memory>
#include <span>
#include <type_traits>

using namespace openblack;

namespace
{
/// Appends the bytes of a value as the file keeps it
template <typename Value>
void Append(std::vector<uint8_t>& out, const Value& value)
{
	static_assert(std::is_trivially_copyable_v<Value>);
	const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
	out.insert(out.end(), bytes, bytes + sizeof(Value));
}

template <typename Value>
void AppendAll(std::vector<uint8_t>& out, std::span<const Value> values)
{
	static_assert(std::is_trivially_copyable_v<Value>);
	const auto* bytes = reinterpret_cast<const uint8_t*>(values.data());
	out.insert(out.end(), bytes, bytes + values.size_bytes());
}
} // namespace

LandData LandData::FromLnd(const lnd::LNDFile& lnd)
{
	LandData data;
	data.blocks = lnd.GetBlocks();
	if (data.blocks.size() <= k_MostBlocksInTable)
	{
		std::ranges::copy(lnd.GetHeader().lookUpTable, data.blockIndexLookup.begin());
	}
	else
	{
		for (size_t i = 0; i < data.blocks.size(); ++i)
		{
			const auto& block = data.blocks[i];
			constexpr auto k_Side = static_cast<uint32_t>(k_BlocksPerSide);
			if (block.blockX < k_Side && block.blockZ < k_Side)
			{
				data.blockIndexLookup.at((block.blockX * k_Side) + block.blockZ) = static_cast<uint16_t>(i + 1);
			}
		}
	}
	data.countries = lnd.GetCountries();
	data.materials = lnd.GetMaterials();
	const auto& extra = lnd.GetExtra();
	data.noise.assign(extra.noise.texels.begin(), extra.noise.texels.end());
	data.bump.assign(extra.bump.texels.begin(), extra.bump.texels.end());
	data.coarsePages = lnd.GetLowResolutionTextures();
	return data;
}

std::vector<uint8_t> LandData::LndBytes() const
{
	lnd::LNDHeader header {};
	header.blockCount = static_cast<uint32_t>(blocks.size() + 1);
	for (size_t i = 0; i < header.lookUpTable.size(); ++i)
	{
		const auto index = blockIndexLookup.at(i);
		header.lookUpTable.at(i) = index <= k_MostBlocksInTable ? static_cast<uint8_t>(index) : uint8_t {0};
	}
	header.materialCount = static_cast<uint32_t>(materials.size());
	header.countryCount = static_cast<uint32_t>(countries.size());
	header.blockSize = static_cast<uint32_t>(sizeof(lnd::LNDBlock));
	header.materialSize = static_cast<uint32_t>(sizeof(lnd::LNDMaterial));
	header.countrySize = static_cast<uint32_t>(sizeof(lnd::LNDCountry));
	header.lowResolutionCount = static_cast<uint32_t>(coarsePages.size());

	std::vector<uint8_t> out;
	Append(out, header);
	for (const auto& page : coarsePages)
	{
		auto pageHeader = page.header;
		pageHeader.size = static_cast<uint32_t>(page.texels.size() + sizeof(pageHeader.size));
		Append(out, pageHeader);
		out.insert(out.end(), page.texels.begin(), page.texels.end());
	}
	AppendAll(out, std::span(blocks));
	AppendAll(out, std::span(countries));
	AppendAll(out, std::span(materials));
	// The noise and bump maps are large, so they do not go on the stack
	const auto extra = std::make_unique<lnd::LNDExtraTextures>();
	std::memcpy(extra->noise.texels.data(), noise.data(), std::min(noise.size(), extra->noise.texels.size()));
	std::memcpy(extra->bump.texels.data(), bump.data(), std::min(bump.size(), extra->bump.texels.size()));
	Append(out, *extra);
	return out;
}
