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

#include <array>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Graphics/TreeShadows.h"

namespace openblack::graphics
{

class Texture2D;

/// The trees' and big forests' shadows baked into the land (see tree_shadows), kept in one texture over the island at
/// the land blocks' texels: each texel holds what the land's colour is scaled by, over 256. A block is baked again only
/// when a tree whose shadow it takes changes: planted, grown, turned, moved, felled or taken away.
class TreeShadowBake
{
public:
	TreeShadowBake();
	~TreeShadowBake();
	TreeShadowBake(const TreeShadowBake&) = delete;
	TreeShadowBake& operator=(const TreeShadowBake&) = delete;

	/// Bakes again the blocks whose trees changed since the last time. Rows run along z as the island's other textures
	/// do: from its far end down when the renderer's textures start at the top.
	void Update(bool rowsFromFarEnd);
	/// Forgets everything, so the next update bakes the whole island
	void Reset();
	/// The baked texture, none before the first update with an island
	[[nodiscard]] const Texture2D* GetTexture() const { return _texture.get(); }

	/// A triangle of a model, in its own space, with its texture's alpha
	struct Triangle
	{
		std::array<glm::vec3, 3> positions;
		std::array<glm::vec2, 3> uvs;
		const tree_shadows::AlphaMask* mask;
	};

private:
	/// What a tree's shadow was last baked from, to tell when it changes
	struct Baked
	{
		uint64_t signature;
		std::vector<uint32_t> cells;
		bool seen;
	};

	void MarkBlocksOf(const std::vector<uint32_t>& cells);
	/// The triangles of a model that cast its shadow, found once
	std::span<const Triangle> ShapeOf(entt::id_type meshId);

	std::unique_ptr<Texture2D> _texture;
	glm::u16vec2 _texels {0, 0};
	glm::vec2 _extentMin {0.0f};
	bool _rowsFromFarEnd {false};
	std::unordered_map<entt::entity, Baked> _baked;
	std::unordered_map<entt::id_type, std::vector<Triangle>> _shapes;
	/// The land blocks to bake again, by their index along x and z
	std::vector<glm::ivec2> _dirty;
	bool _bakeAll {true};
};

} // namespace openblack::graphics
