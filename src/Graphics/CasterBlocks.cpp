/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CasterBlocks.h"

#include <algorithm>

#include "3D/MapCoords.h"
#include "Visual/TreeShadows.h"

namespace openblack::graphics
{

uint64_t CasterBlocks::KeyOf(glm::ivec2 block)
{
	return (static_cast<uint64_t>(static_cast<uint32_t>(block.x)) << 32u) | static_cast<uint32_t>(block.y);
}

std::vector<glm::ivec2> CasterBlocks::BlocksOf(std::span<const uint32_t> cells)
{
	std::vector<glm::ivec2> blocks;
	for (const auto index : cells)
	{
		const auto cellX = static_cast<int>(index / map_coords::k_MapCells);
		const auto cellZ = static_cast<int>(index % map_coords::k_MapCells);
		const auto alongX = tree_shadows::BlocksTakingShadowFrom(cellX);
		const auto alongZ = tree_shadows::BlocksTakingShadowFrom(cellZ);
		for (int x = alongX.x; x <= alongX.y; ++x)
		{
			for (int z = alongZ.x; z <= alongZ.y; ++z)
			{
				blocks.emplace_back(x, z);
			}
		}
	}
	// A thing that covers many cells reaches most blocks from more than one cell
	const auto lower = [](glm::ivec2 a, glm::ivec2 b) { return a.x != b.x ? a.x < b.x : a.y < b.y; };
	std::ranges::sort(blocks, lower);
	const auto [repeatsFirst, repeatsLast] = std::ranges::unique(blocks);
	blocks.erase(repeatsFirst, repeatsLast);
	return blocks;
}

void CasterBlocks::Add(entt::entity caster, std::span<const uint32_t> cells)
{
	for (const auto block : BlocksOf(cells))
	{
		_casters[KeyOf(block)].push_back(caster);
	}
}

void CasterBlocks::Remove(entt::entity caster, std::span<const uint32_t> cells)
{
	for (const auto block : BlocksOf(cells))
	{
		const auto found = _casters.find(KeyOf(block));
		if (found == _casters.end())
		{
			continue;
		}
		auto& casters = found->second;
		if (const auto at = std::ranges::find(casters, caster); at != casters.end())
		{
			// The order in a block does not count: CastingInto sorts the casters
			*at = casters.back();
			casters.pop_back();
		}
		if (casters.empty())
		{
			_casters.erase(found);
		}
	}
}

std::vector<entt::entity> CasterBlocks::CastingInto(std::span<const glm::ivec2> blocks) const
{
	std::vector<entt::entity> casters;
	for (const auto block : blocks)
	{
		if (const auto found = _casters.find(KeyOf(block)); found != _casters.end())
		{
			casters.insert(casters.end(), found->second.begin(), found->second.end());
		}
	}
	std::ranges::sort(casters);
	const auto [repeatsFirst, repeatsLast] = std::ranges::unique(casters);
	casters.erase(repeatsFirst, repeatsLast);
	return casters;
}

} // namespace openblack::graphics
