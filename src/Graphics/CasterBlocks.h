/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <span>
#include <unordered_map>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

namespace openblack::graphics
{

/// The casters of the land's baked shadows by the land blocks that take their shadows. A caster's shadow falls into each
/// block that takes the shadows of one of its map cells (see tree_shadows::BlocksTakingShadowFrom). A bake of some blocks
/// then finds their casters with no walk of every caster.
class CasterBlocks
{
public:
	/// Adds a caster with its map cells, by their index. Add a caster only one time, and remove it with the same cells.
	void Add(entt::entity caster, std::span<const uint32_t> cells);
	/// Removes a caster that was added with these cells
	void Remove(entt::entity caster, std::span<const uint32_t> cells);
	void Clear() { _casters.clear(); }
	/// The casters whose shadows fall into one or more of the blocks, by their index along x and z. Each caster comes one
	/// time, in the order of the entities.
	[[nodiscard]] std::vector<entt::entity> CastingInto(std::span<const glm::ivec2> blocks) const;

	/// The blocks that take the shadows of the map cells, each one time
	[[nodiscard]] static std::vector<glm::ivec2> BlocksOf(std::span<const uint32_t> cells);

private:
	[[nodiscard]] static uint64_t KeyOf(glm::ivec2 block);

	std::unordered_map<uint64_t, std::vector<entt::entity>> _casters;
};

} // namespace openblack::graphics
