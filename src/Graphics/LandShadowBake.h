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
#include <future>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/Transform.h"
#include "ECS/LandShadowCasters.h"
#include "Graphics/TreeShadows.h"

namespace openblack::graphics
{

class Texture2D;

/// The shadows Black & White bakes into the land as it paints each land block: the trees' and big forests' through their
/// leaves (see tree_shadows), and the buildings', rocks' and other solid things' by their shapes (see shape_shadows),
/// joined in each texel. They are kept in one texture over the island at the land blocks' texels: each texel holds what
/// the land's colour is scaled by, over 256. A block is baked again only when a thing whose shadow it takes changes:
/// made, built, grown, turned, moved or taken away.
class LandShadowBake
{
public:
	LandShadowBake();
	~LandShadowBake();
	LandShadowBake(const LandShadowBake&) = delete;
	LandShadowBake& operator=(const LandShadowBake&) = delete;

	/// Which things' shadows are baked
	struct Casters
	{
		/// The buildings', rocks' and other solid things'
		bool shapes {true};
		/// The trees' and big forests'
		bool leaves {true};

		bool operator==(const Casters&) const = default;
	};

	/// Bakes again the blocks whose casters changed since the last time. Rows run along z as the island's other textures
	/// do: from its far end down when the renderer's textures start at the top.
	void Update(bool rowsFromFarEnd, Casters casters);
	/// Forgets everything, so the next update bakes the whole island
	void Reset();
	/// The baked texture, none before the first update with an island
	[[nodiscard]] const Texture2D* GetTexture() const { return _texture.get(); }

	/// A triangle of a tree's model, in its own space, with its texture's alpha
	struct LeafTriangle
	{
		std::array<glm::vec3, 3> positions;
		std::array<glm::vec2, 3> uvs;
		const tree_shadows::AlphaMask* mask;
	};
	/// A triangle of a solid thing's model, in its own space, and whether it is drawn from both sides
	struct ShapeTriangle
	{
		std::array<glm::vec3, 3> positions;
		bool bothSides;
	};

private:
	/// What a caster's shadow was last baked from, to tell when it changes
	struct Baked
	{
		uint64_t signature;
		std::vector<uint32_t> cells;
		bool seen;
		/// A solid thing's triangles flattened where they fall in the world, kept until it changes, and the corners of
		/// the land they fall within, along x and z
		std::shared_ptr<const std::vector<glm::vec2>> cast;
		glm::vec2 castLow {0.0f};
		glm::vec2 castHigh {0.0f};
	};

	/// A copy of a caster, as it was when its bake started
	struct BakeCaster
	{
		ecs::LandShadow shadow;
		std::vector<uint32_t> cells;
		ecs::components::Transform transform;
		std::span<const LeafTriangle> leaves;
		std::span<const ShapeTriangle> shape;
		std::shared_ptr<const std::vector<glm::vec2>> cast;
		glm::vec2 castLow {0.0f};
		glm::vec2 castHigh {0.0f};
	};
	/// A land block to bake: its index along x and z, and its corner on the land
	struct BakeBlock
	{
		glm::ivec2 position;
		glm::vec2 corner;
	};
	/// All that a bake reads
	struct BakeJob
	{
		std::vector<BakeBlock> blocks;
		std::vector<BakeCaster> casters;
		bool rowsFromFarEnd {false};
	};
	/// The texels of a baked block
	struct BlockShades
	{
		glm::vec2 corner {0.0f};
		std::vector<uint8_t> shades;
	};

	/// Bakes the blocks, on as many threads as there are blocks, up to one for each processor
	[[nodiscard]] static std::vector<BlockShades> Bake(const BakeJob& job);
	/// Puts the baked blocks into the texture
	void Apply(const std::vector<BlockShades>& results);
	/// Puts the blocks of the bake on the other threads into the texture, if the bake is done
	void ApplyFinishedBake();

	void MarkBlocksOf(std::span<const uint32_t> cells);
	/// The triangles of a model that cast its shadow, found once
	std::span<const LeafTriangle> LeavesOf(entt::id_type meshId);
	std::span<const ShapeTriangle> ShapeOf(entt::id_type meshId);

	std::unique_ptr<Texture2D> _texture;
	glm::u16vec2 _texels {0, 0};
	glm::vec2 _extentMin {0.0f};
	bool _rowsFromFarEnd {false};
	Casters _casters {};
	std::unordered_map<entt::entity, Baked> _baked;
	std::unordered_map<entt::id_type, std::vector<LeafTriangle>> _leaves;
	std::unordered_map<entt::id_type, std::vector<ShapeTriangle>> _shapes;
	/// The land blocks to bake again, by their index along x and z
	std::vector<glm::ivec2> _dirty;
	bool _bakeAll {true};
	/// The bake that runs on other threads
	std::future<std::vector<BlockShades>> _bake;
};

} // namespace openblack::graphics
