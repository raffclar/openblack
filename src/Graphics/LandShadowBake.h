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
#include <optional>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/Transform.h"
#include "ECS/LandShadowCasters.h"
#include "Graphics/CasterBlocks.h"
#include "Visual/TreeShadows.h"

namespace openblack
{
class SideThread;
class WorkerPool;
} // namespace openblack
namespace openblack::ecs
{
class Registry;
}

namespace openblack::graphics
{

namespace land_shadow_bake
{
struct Caster;
struct MayCast;
} // namespace land_shadow_bake

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
	/// How many times an update got other casters than the update before it (the debug menu can change them)
	[[nodiscard]] uint32_t GetCastersChanges() const noexcept { return _castersChanges; }
	/// Forgets everything, so the next update bakes the whole island
	void Reset();
	/// Starts the test of the watched things on a thread of its own, before the next update needs its answer. The fill of
	/// the drawn frame only reads the world, thus the thread reads it while the fill continues. Call it only in a fill
	/// that calls Update.
	void StartWatchedTest();
	/// Waits for the test that StartWatchedTest started and forgets its answer
	void ForgetWatchedTest();
	/// Bakes the whole island again after a restore of the graphics device, which keeps no copy of the texture. The
	/// texture shows no shadow until the bake ends.
	void BakeAgain();
	/// The baked texture, none before the first update with an island
	[[nodiscard]] const Texture2D* GetTexture() const { return _texture.get(); }
	/// The blocks that the bakes put into the texture since the last call, by their index along x and z
	[[nodiscard]] std::vector<glm::ivec2> TakeNewlyBaked() { return std::exchange(_newlyBaked, {}); }
	/// Whether the bake on the other threads bakes the block now, by its index along x and z
	[[nodiscard]] bool Bakes(glm::ivec2 block) const;

	/// A triangle of a tree's model, in its own space, with its texture's alpha. The triangle keeps a copy of the alpha.
	/// The bake on the other threads can then continue when the resource caches release the model and its textures, as
	/// at the exit of the game.
	struct LeafTriangle
	{
		std::array<glm::vec3, 3> positions;
		std::array<glm::vec2, 3> uvs;
		std::shared_ptr<const tree_shadows::AlphaMask> mask;
	};
	/// A triangle of a solid thing's model, in its own space, and whether it is drawn from both sides
	struct ShapeTriangle
	{
		std::array<glm::vec3, 3> positions;
		bool bothSides;
	};

private:
	/// The values of a caster that its shadow comes from, other than its cells. The bake compares them by their bits.
	struct BakedFrom
	{
		uint32_t shadow;
		uint32_t mesh;
		uint32_t meshReady;
		glm::vec3 position;
		glm::mat3 rotation;
		glm::vec3 scale;
	};

	/// A caster, and what its shadow was last baked from, to tell when it changes
	struct Baked
	{
		entt::entity entity;
		BakedFrom from;
		/// The map cells it was filed in, or the cell under a thing that is never filed
		std::vector<uint32_t> cells;
		ecs::components::Transform transform;
		/// Its cells are the map cells it was filed in, which the test of the watched things compares
		bool filed;
		/// Its shadow can change while its components stay the same, which the test of the watched things checks
		bool shadowChangesInPlace;
		/// A solid thing's triangles flattened where they fall in the world, kept until it changes, and the corners of
		/// the land they fall within, along x and z
		std::shared_ptr<const std::vector<glm::vec2>> cast;
		glm::vec2 castLow {0.0f};
		glm::vec2 castHigh {0.0f};
	};

	/// A thing that casts no shadow now, but that can start to cast while its components stay the same, and what its
	/// shadow depends on
	struct Watched
	{
		entt::entity entity;
		/// Its shadow then, checked again for a thing whose shadow can change in place
		ecs::LandShadow shadow;
		bool shadowChangesInPlace;
		/// Whether its transform, mesh and cells are kept: those of a caster, or of a thing that a move can make cast
		bool watchesPlace;
		ecs::components::Transform transform;
		entt::id_type mesh;
	};
	/// A watched thing that changed: a caster by its place in the casters, or else a thing by its place in the things that
	/// can start to cast
	struct WatchedChange
	{
		bool caster;
		size_t index;
	};

	/// A copy of a caster, as it was when its bake started. Its cells are in the cells of the job.
	struct BakeCaster
	{
		ecs::LandShadow shadow;
		uint32_t firstCell {0};
		uint32_t cellCount {0};
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
		/// The cells of all the casters, one after the other, so that the job makes no list for each caster
		std::vector<uint32_t> cells;
		bool rowsFromFarEnd {false};
		/// The texels along a side of a block: those of the land blocks' textures
		int side {tree_shadows::k_BlockTexels};
	};
	/// The texels of a baked block
	struct BlockShades
	{
		glm::ivec2 position {0};
		glm::vec2 corner {0.0f};
		std::vector<uint8_t> shades;
	};

	/// Bakes the blocks, on as many threads as there are blocks, up to one for each processor
	[[nodiscard]] static std::vector<BlockShades> Bake(const BakeJob& job);
	/// Puts the baked blocks into the texture
	void Apply(const std::vector<BlockShades>& results);
	/// Puts the blocks of the bake on the other threads into the texture, if the bake is done
	void ApplyFinishedBake();

	/// The component stores that the test of the watched things reads
	struct WatchedStores;
	/// The watched things that changed what their shadows depend on since the last look at the casters: the casters
	/// and the things that can start to cast. It reads only the given stores, thus a side thread can do the test while
	/// the game's thread adds stores to the registry. With the marked things, it tests only the casters of the marked
	/// entity numbers and the casters whose shadows can change in place; with none, it tests all the casters. When
	/// casters can be gone, a watched thing that lost its transform, its mesh or its map cells changed.
	[[nodiscard]] std::vector<WatchedChange> FindWatchedChanges(const WatchedStores& stores,
	                                                            const std::optional<std::vector<uint32_t>>& marked,
	                                                            bool castersCanBeGone) const;
	/// The numbers of the things that the registry marked since the last take, none when the test must look at all the
	/// casters. Call it on the game's thread.
	[[nodiscard]] std::optional<std::vector<uint32_t>> TakeMarkedThings();
	/// Waits for the test that StartWatchedTest started, and gives its answer. No test: no answer.
	[[nodiscard]] std::optional<std::vector<WatchedChange>> TakeWatchedTest();
	/// Looks at all the things that can cast, after a caster came or went, and marks the blocks whose shadows changed
	void LookAtAllCasters(Casters casters);
	/// Looks again only at the things that changed, and marks the blocks whose shadows changed. The things are in the
	/// order of their entities, each one time: the watched things that changed, the things that got or lost a component
	/// that their shadows depend on, and the casters whose models became ready or went. It gives the same casters as a
	/// look at all of them.
	void UpdateChanged(std::span<const entt::entity> changed, Casters casters);
	/// The kept caster of the entity, none if it is not a caster
	[[nodiscard]] Baked* FindBaked(entt::entity entity);
	/// Keeps a new caster
	void AddBaked(Baked&& baked);
	/// Forgets a kept caster
	void RemoveBaked(entt::entity entity);
	/// Finds the kept casters again after a change of their order
	void FindBakedPlaces();
	/// A caster as it is now
	[[nodiscard]] static Baked BakedOf(const land_shadow_bake::Caster& caster, const ecs::Registry& registry);
	/// A thing that can start to cast, as it is now
	[[nodiscard]] static Watched WatchedOf(const land_shadow_bake::MayCast& thing);
	/// Keeps a caster as it is now, and marks the blocks of its old and its new place when its shadow changed
	void Rebake(Baked& kept, Baked&& now);
	void MarkBlocksOf(std::span<const uint32_t> cells);
	/// The triangles of a model that cast its shadow, found once
	std::span<const LeafTriangle> LeavesOf(entt::id_type meshId);
	std::span<const ShapeTriangle> ShapeOf(entt::id_type meshId);

	std::unique_ptr<Texture2D> _texture;
	glm::u16vec2 _texels {0, 0};
	/// The texels along a side of a block, as the land blocks' textures have them
	int _side {tree_shadows::k_BlockTexels};
	glm::vec2 _extentMin {0.0f};
	bool _rowsFromFarEnd {false};
	Casters _casters {};
	uint32_t _castersChanges {0};
	/// The casters, in no order. The places of the casters, by the index of their entities, find each one.
	std::vector<Baked> _baked;
	std::vector<uint32_t> _bakedPlaces;
	/// The casters by the blocks that take their shadows
	CasterBlocks _casterBlocks;
	/// The things that can start to cast, the count of the casters' changes and the number of models when the bake last
	/// looked at the casters. The bake then looks again only at the things that changed: the watched things, the things
	/// that the registry gives as changed, and the casters of the models that became ready or went.
	std::vector<Watched> _mayCast;
	std::optional<uint64_t> _castersVersion;
	size_t _meshCount {0};
	std::unordered_map<entt::id_type, std::vector<LeafTriangle>> _leaves;
	std::unordered_map<entt::id_type, std::vector<ShapeTriangle>> _shapes;
	/// The blocks put into the texture since the last TakeNewlyBaked, and the blocks of the bake on the other threads in
	/// order along x and then z
	std::vector<glm::ivec2> _newlyBaked;
	std::vector<glm::ivec2> _bakingBlocks;
	/// The land blocks to bake again, by their index along x and z
	std::vector<glm::ivec2> _dirty;
	bool _bakeAll {true};
	/// The bake that runs on other threads
	std::future<std::vector<BlockShades>> _bake;
	/// The answer of the test of the watched things that runs on the side thread
	std::optional<std::vector<WatchedChange>> _watchedAnswer;
	bool _watchedTestStarted {false};
	/// The next test looks at all the casters: before the first look, and after a test whose answer no update used
	bool _testAllCasters {true};
	/// The registry whose marks the test took last
	const ecs::Registry* _markedRegistry {nullptr};
	/// The lanes that share the test of the watched things when there are many casters
	std::unique_ptr<WorkerPool> _testWorkers;
	/// The last member, so that its thread stops before the other members go
	std::unique_ptr<SideThread> _watchedTest;
};

} // namespace openblack::graphics
