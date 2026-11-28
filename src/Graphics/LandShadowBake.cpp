/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandShadowBake.h"

#include <cassert>

#include <algorithm>
#include <chrono>
#include <iterator>
#include <limits>
#include <ranges>
#include <span>
#include <thread>
#include <tuple>
#include <utility>
#include <variant>

#include <glm/mat3x3.hpp>

#include "3D/L3DMesh.h"
#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/IntegerSteps.h"
#include "Common/SameBits.h"
#include "Common/SideThread.h"
#include "Common/WorkerPool.h"
#include "Common/WorkerPools.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/RepaintsLandWhenFiled.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/LandShadowCasters.h"
#include "ECS/Registry.h"
#include "Graphics/ShapeShadows.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::graphics
{

using ecs::LandShadow;
using ecs::components::MapCellResident;
using ecs::components::Transform;
using MeshComponent = ecs::components::Mesh;

/// The stores are found on the game's thread. The registry keeps its stores in a list that grows when the registry gets
/// the first component of a kind. Thus a side thread must not look in that list while the game's thread can add to it.
/// A store stays at the same place while the registry exists, thus the side thread can read the stores found before.
struct LandShadowBake::WatchedStores
{
	explicit WatchedStores(const ecs::Registry& registry)
	    : transforms(registry.Underlying().storage<Transform>())
	    , meshes(registry.Underlying().storage<MeshComponent>())
	    , residents(registry.Underlying().storage<MapCellResident>())
	    , inPlace(ecs::LandShadowInPlaceStores(registry))
	    , shadows(registry)
	{
	}
	const entt::storage_for_t<Transform>* transforms;
	const entt::storage_for_t<MeshComponent>* meshes;
	const entt::storage_for_t<MapCellResident>* residents;
	/// The things whose shadows can change in place
	std::array<const entt::sparse_set*, 2> inPlace;
	ecs::LandShadowLookup shadows;
};

namespace land_shadow_bake
{

/// A thing casting its shadow into the land, standing in the map's cells
struct Caster
{
	entt::entity entity;
	LandShadow shadow;
	const MeshComponent* mesh;
	const Transform* transform;
	/// The cells it was filed in, or none for a thing that is never filed: then the cell under it
	const MapCellResident* resident;
	uint32_t cellUnder;

	[[nodiscard]] std::span<const uint32_t> Cells() const
	{
		return resident != nullptr ? std::span<const uint32_t>(resident->cells) : std::span<const uint32_t>(&cellUnder, 1);
	}
};

/// A thing that casts no shadow now, but can start to cast without a change of its components: its shadow now, and its
/// transform and mesh if a move can make it cast
struct MayCast
{
	entt::entity entity;
	LandShadow shadow;
	bool shadowChangesInPlace;
	const MeshComponent* mesh;
	const Transform* transform;
};

} // namespace land_shadow_bake

namespace
{

using land_shadow_bake::Caster;
using land_shadow_bake::MayCast;

/// What one thing of a kind that can cast is now: a caster, a thing that may cast, or neither
using Sorted = std::variant<std::monostate, Caster, MayCast>;

/// Sorts one thing of a kind that can cast. A thing has to be in the map's cells to cast (one carried by the hand casts
/// none), except a worship site and its altar, which stand at the temple without being filed in them. A thing that can
/// start to cast without a change of its components may cast: one whose shadow is none or not wanted now but can change
/// in place, and a worship site or altar off the map, which can move onto it.
[[nodiscard]] Sorted SortThing(const ecs::Registry& registry, const ecs::LandShadowLookup& shadows,
                               LandShadowBake::Casters wanted, entt::entity entity)
{
	const auto& mesh = registry.Get<const MeshComponent>(entity);
	const auto& transform = registry.Get<const Transform>(entity);
	const auto shadow = shadows.Of(entity);
	const bool leaves = shadow == LandShadow::Leaves;
	if (shadow == LandShadow::None || (leaves ? !wanted.leaves : !wanted.shapes))
	{
		if (ecs::LandShadowChangesInPlace(registry, entity))
		{
			return MayCast {
			    .entity = entity, .shadow = shadow, .shadowChangesInPlace = true, .mesh = nullptr, .transform = nullptr};
		}
		return std::monostate {};
	}
	const auto* resident = registry.TryGet<const MapCellResident>(entity);
	uint32_t cellUnder = 0;
	if (resident == nullptr)
	{
		if (!registry.AnyOf<ecs::components::WorshipSite, ecs::components::WorshipAltar>(entity))
		{
			return std::monostate {};
		}
		const auto cell = map_coords::CellOf(transform.position);
		if (static_cast<uint32_t>(cell.x) >= map_coords::k_MapCells || static_cast<uint32_t>(cell.y) >= map_coords::k_MapCells)
		{
			return MayCast {.entity = entity,
			                .shadow = shadow,
			                .shadowChangesInPlace = ecs::LandShadowChangesInPlace(registry, entity),
			                .mesh = &mesh,
			                .transform = &transform};
		}
		cellUnder = static_cast<uint32_t>(cell.x) * map_coords::k_MapCells + static_cast<uint32_t>(cell.y);
	}
	return Caster {.entity = entity,
	               .shadow = shadow,
	               .mesh = &mesh,
	               .transform = &transform,
	               .resident = resident,
	               .cellUnder = cellUnder};
}

/// Gathers the casters of one kind of thing, and the things of the kind that may cast (see SortThing)
template <typename Kind>
void GatherCasters(const ecs::Registry& registry, LandShadowBake::Casters wanted, std::vector<Caster>& casters,
                   std::vector<MayCast>& mayCast)
{
	const ecs::LandShadowLookup shadows(registry);
	registry.Each<const Kind, const MeshComponent, const Transform>(
	    // Kinds without data aren't passed in
	    [&](entt::entity entity, const auto&... /*unused*/) {
		    const auto sorted = SortThing(registry, shadows, wanted, entity);
		    if (const auto* caster = std::get_if<Caster>(&sorted))
		    {
			    casters.push_back(*caster);
		    }
		    else if (const auto* thing = std::get_if<MayCast>(&sorted))
		    {
			    mayCast.push_back(*thing);
		    }
	    });
}

/// The kinds of things that a look at all the casters gathers
template <typename... Kinds>
struct KindList
{
	static void Gather(const ecs::Registry& registry, LandShadowBake::Casters wanted, std::vector<Caster>& casters,
	                   std::vector<MayCast>& mayCast)
	{
		(GatherCasters<Kinds>(registry, wanted, casters, mayCast), ...);
	}
	[[nodiscard]] static bool Has(const ecs::Registry& registry, entt::entity entity)
	{
		return registry.AnyOf<Kinds...>(entity);
	}
};
/// The kinds that cast through their leaves
using LeafKinds = KindList<ecs::components::Tree, ecs::components::BigForest>;
/// The kinds that cast by their shapes
using ShapeKinds = KindList<ecs::components::Abode, ecs::components::Temple, ecs::components::WorshipSite,
                            ecs::components::WorshipAltar, ecs::components::WorshipIcon, ecs::components::SpellDispenser,
                            ecs::components::Feature, ecs::components::MobileStatic, ecs::components::TeleportStone,
                            ecs::components::MobileObject, ecs::components::Ball, ecs::components::RewardOnLand>;

/// Sorts one thing again, as a look at all the casters sorts it. A thing that is gone, or that is of no kind that the
/// look gathers, is not a caster and cannot start to cast.
[[nodiscard]] Sorted SortAgain(const ecs::Registry& registry, const ecs::LandShadowLookup& shadows,
                               LandShadowBake::Casters wanted, entt::entity entity)
{
	if (!registry.Valid(entity) || !registry.AllOf<MeshComponent, Transform>(entity))
	{
		return std::monostate {};
	}
	if (!(wanted.leaves && LeafKinds::Has(registry, entity)) && !(wanted.shapes && ShapeKinds::Has(registry, entity)))
	{
		return std::monostate {};
	}
	return SortThing(registry, shadows, wanted, entity);
}

/// The place of an entity that has no kept caster
constexpr uint32_t k_NoPlace = std::numeric_limits<uint32_t>::max();

/// The fewest casters for each lane of the pool that shares the test of the watched things
constexpr size_t k_LeastCastersToShare = 4096;

glm::ivec2 CellOf(uint32_t index)
{
	return {static_cast<int>(index / map_coords::k_MapCells), static_cast<int>(index % map_coords::k_MapCells)};
}

/// How a caster stands: turned and scaled
glm::mat3 PlacingOf(const Transform& transform)
{
	return transform.rotation * glm::mat3(glm::vec3(transform.scale.x, 0.0f, 0.0f), glm::vec3(0.0f, transform.scale.y, 0.0f),
	                                      glm::vec3(0.0f, 0.0f, transform.scale.z));
}

/// Casts a tree's triangles into a block's texels through their textures' alpha
void CastLeaves(tree_shadows::Coverage& coverage, const Transform& transform,
                std::span<const LandShadowBake::LeafTriangle> leaves, glm::vec2 blockCorner, int side)
{
	const auto placing = PlacingOf(transform);
	const glm::vec2 foot {transform.position.x, transform.position.z};
	for (const auto& triangle : leaves)
	{
		std::array<tree_shadows::Corner, 3> corners;
		for (size_t k = 0; k < 3; ++k)
		{
			corners[k] = tree_shadows::Flatten(placing * triangle.positions[k], foot, blockCorner, triangle.uvs[k], side);
		}
		if (tree_shadows::Reaches(corners, side))
		{
			tree_shadows::Fill(coverage, corners, *triangle.mask, side);
		}
	}
}

/// Casts a solid thing's flattened triangles into a block's samples
void CastShape(tree_shadows::Coverage& coverage, std::span<const glm::vec2> cast,
               std::span<const LandShadowBake::ShapeTriangle> shape, bool allBothSides, glm::vec2 blockCorner, int side)
{
	const int rows = 2 * side;
	const int columns = 4 * side;
	for (size_t t = 0; t < shape.size(); ++t)
	{
		std::array<shape_shadows::Sample, 3> corners;
		float lowRow = 0.0f;
		float highRow = 0.0f;
		float lowColumn = 0.0f;
		float highColumn = 0.0f;
		for (size_t k = 0; k < 3; ++k)
		{
			corners[k] = shape_shadows::SampleOf(cast[t * 3 + k], blockCorner, side);
			lowRow = k == 0 ? corners[k].row : std::min(lowRow, corners[k].row);
			highRow = k == 0 ? corners[k].row : std::max(highRow, corners[k].row);
			lowColumn = k == 0 ? corners[k].column : std::min(lowColumn, corners[k].column);
			highColumn = k == 0 ? corners[k].column : std::max(highColumn, corners[k].column);
		}
		// Nothing of a triangle wholly before or past the block's samples is covered
		if (highRow < 1.0f || lowRow >= static_cast<float>(rows) || highColumn < -1.0f ||
		    lowColumn > static_cast<float>(columns + 1))
		{
			continue;
		}
		shape_shadows::Fill(coverage, corners, allBothSides || shape[t].bothSides, side);
	}
}

/// The flag of the thing's model that asks for a paint each time it goes into the map's cells or comes out of them
std::optional<ecs::land_repaint::Flag> FlagOf(const ecs::Registry& registry, entt::entity entity)
{
	const auto* flag = registry.TryGet<const ecs::components::RepaintsLandWhenFiled>(entity);
	if (flag == nullptr)
	{
		return std::nullopt;
	}
	return ecs::land_repaint::Flag {.afterFiling = flag->afterFiling};
}

} // namespace

LandShadowBake::LandShadowBake() = default;
LandShadowBake::~LandShadowBake()
{
	if (_bake.valid())
	{
		_bake.wait();
	}
}

void LandShadowBake::Reset()
{
	// The test of the watched things reads the watched things: it ends before they go
	std::ignore = TakeWatchedTest();
	// A bake that still runs reads the kept triangles of the models: it ends before they go
	if (_bake.valid())
	{
		_bake.wait();
		_bake = {};
	}
	_texture.reset();
	_texels = {0, 0};
	_baked.clear();
	_bakedPlaces.clear();
	_casterBlocks.Clear();
	_newlyBaked.clear();
	_bakingBlocks.clear();
	_mayCast.clear();
	_castersVersion.reset();
	_leaves.clear();
	_shapes.clear();
	_dirty.clear();
	_stale.clear();
	_paintedElsewhere.clear();
	_bakeAll = true;
}

void LandShadowBake::BakeAgain()
{
	if (_texture == nullptr)
	{
		return;
	}
	// The blocks with no land are not baked: they show no shadow
	const std::vector<uint8_t> unshaded(static_cast<size_t>(_texels.x) * _texels.y, 255);
	_texture->Update(unshaded.data(), static_cast<uint32_t>(unshaded.size()));
	_bakeAll = true;
}

std::span<const LandShadowBake::LeafTriangle> LandShadowBake::LeavesOf(entt::id_type meshId)
{
	if (const auto found = _leaves.find(meshId); found != _leaves.end())
	{
		return found->second;
	}
	// A model not loaded yet casts nothing until it is
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(meshId))
	{
		return {};
	}
	auto& leaves = _leaves[meshId];
	const auto mesh = meshes.Handle(meshId);
	const auto& textures = Locator::resources::value().GetTextures();
	// Every triangle of the nearest detail, not the parts moved by bones nor the physics' own parts, with its texture's
	// alpha: the model's own skin, or a texture of the pack its models share
	for (const auto& submesh : mesh->GetSubMeshes())
	{
		const auto flags = submesh->GetFlags();
		if ((flags.lodMask & 1u) == 0 || flags.hasBones != 0 || flags.isPhysics != 0)
		{
			continue;
		}
		const auto& surface = submesh->GetSurface();
		for (const auto& primitive : submesh->GetPrimitives())
		{
			const auto* mask = mesh->GetSkinAlpha(primitive.skinID);
			if (mask == nullptr)
			{
				if (const auto* shared = textures.Find(primitive.skinID); shared != nullptr)
				{
					mask = shared->GetAlphaMask();
				}
			}
			if (mask == nullptr)
			{
				continue;
			}
			const auto kept = std::make_shared<const tree_shadows::AlphaMask>(*mask);
			const auto end = std::min<size_t>(primitive.indicesOffset + primitive.indicesCount, surface.indices.size());
			for (size_t i = primitive.indicesOffset; i + 2 < end; i += 3)
			{
				LeafTriangle triangle {.positions = {}, .uvs = {}, .mask = kept};
				bool valid = true;
				for (size_t k = 0; k < 3; ++k)
				{
					const auto vertex = surface.indices[i + k];
					if (vertex >= surface.positions.size() || vertex >= surface.uvs.size())
					{
						valid = false;
						break;
					}
					triangle.positions[k] = surface.positions[vertex];
					triangle.uvs[k] = surface.uvs[vertex];
				}
				if (valid)
				{
					leaves.push_back(triangle);
				}
			}
		}
	}
	return leaves;
}

std::span<const LandShadowBake::ShapeTriangle> LandShadowBake::ShapeOf(entt::id_type meshId)
{
	if (const auto found = _shapes.find(meshId); found != _shapes.end())
	{
		return found->second;
	}
	// A model not loaded yet casts nothing until it is
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(meshId))
	{
		return {};
	}
	// The shapes of models that the cache does not hold now go first. A land makes models of its own, such as the meshes
	// of its temples, and the next land makes others, so the old shapes would stay for the whole run.
	std::erase_if(_shapes, [&meshes](const auto& kept) { return !meshes.Contains(kept.first); });
	auto& shape = _shapes[meshId];
	// Every triangle of every detail, not the physics' own parts nor the parts shown only in some states. The parts moved
	// by bones are left out: their triangles aren't kept apart from the drawn model.
	const auto mesh = meshes.Handle(meshId);
	for (const auto& submesh : mesh->GetSubMeshes())
	{
		const auto flags = submesh->GetFlags();
		if (flags.isPhysics != 0 || flags.status != 0 || flags.hasBones != 0)
		{
			continue;
		}
		const auto& surface = submesh->GetSurface();
		for (const auto& primitive : submesh->GetPrimitives())
		{
			const auto end = std::min<size_t>(primitive.indicesOffset + primitive.indicesCount, surface.indices.size());
			for (size_t i = primitive.indicesOffset; i + 2 < end; i += 3)
			{
				ShapeTriangle triangle {.positions = {}, .bothSides = primitive.twoSided};
				bool valid = true;
				for (size_t k = 0; k < 3; ++k)
				{
					const auto vertex = surface.indices[i + k];
					if (vertex >= surface.positions.size())
					{
						valid = false;
						break;
					}
					triangle.positions[k] = surface.positions[vertex];
				}
				if (valid)
				{
					shape.push_back(triangle);
				}
			}
		}
	}
	return shape;
}

void LandShadowBake::MarkBlocksOf(std::span<const uint32_t> cells, bool paint)
{
	for (const auto index : cells)
	{
		const auto cell = CellOf(index);
		const auto alongX = tree_shadows::BlocksTakingShadowFrom(cell.x);
		const auto alongZ = tree_shadows::BlocksTakingShadowFrom(cell.y);
		for (int x = alongX.x; x <= alongX.y; ++x)
		{
			for (int z = alongZ.x; z <= alongZ.y; ++z)
			{
				if (paint)
				{
					_dirty.emplace_back(x, z);
				}
				else
				{
					_stale.emplace(x, z);
				}
			}
		}
	}
}

void LandShadowBake::MarkCameOrWent(const Baked& caster, bool came)
{
	const auto repaints = ecs::land_repaint::RepaintsOf(caster.filing, caster.flag, caster.filings);
	MarkBlocksOf(caster.cells, came ? repaints.goingIn : repaints.comingOut);
}

void LandShadowBake::PaintedElsewhere(std::span<const glm::ivec2> blocks)
{
	for (const auto block : blocks)
	{
		_paintedElsewhere.emplace(block.x, block.y);
	}
}

void LandShadowBake::TakePaintedElsewhere()
{
	for (const auto& [x, z] : std::exchange(_paintedElsewhere, {}))
	{
		if (_stale.contains(std::pair(x, z)))
		{
			_dirty.emplace_back(x, z);
		}
	}
}

void LandShadowBake::Update(bool rowsFromFarEnd, Casters casters)
{
	// The answer of the test that started before this update, when there is one. Nothing changed the world or the
	// watched things after the test started.
	auto testedAhead = TakeWatchedTest();
	if (testedAhead.has_value())
	{
		// The marks that the test took are lost until the update uses its answer
		_testAllCasters = true;
	}
	if (!Locator::terrainSystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	const auto& island = Locator::terrainSystem::value();
	const auto& blocks = island.GetBlocks();
	if (blocks.empty())
	{
		return;
	}

	// A texel for every texel of the land blocks' textures, over the whole island
	const auto extent = island.GetExtent();
	const auto blocksAcross = glm::round((extent.maximum - extent.minimum) / tree_shadows::k_BlockSize);
	const auto side = island.GetBlockTextureSide();
	const auto texels = glm::u16vec2(blocksAcross * static_cast<float>(side));
	if (_texture == nullptr || texels != _texels || side != _side || extent.minimum != _extentMin ||
	    rowsFromFarEnd != _rowsFromFarEnd || casters != _casters)
	{
		if (_texture != nullptr && casters != _casters)
		{
			++_castersChanges;
		}
		Reset();
		testedAhead.reset();
		_texels = texels;
		_side = side;
		_extentMin = extent.minimum;
		_rowsFromFarEnd = rowsFromFarEnd;
		_casters = casters;
		_texture = std::make_unique<Texture2D>("Land Shadows");
		_texture->CreateWithinFrame(texels.x, texels.y, 1, TextureFormat::R8, Wrapping::ClampEdge, Filter::Nearest, nullptr);
		// The bake gives the texels again after a restore of the graphics device, thus the device keeps no copy
		_texture->SetTexelsGivenAgain();
		const std::vector<uint8_t> unshaded(static_cast<size_t>(texels.x) * texels.y, 255);
		_texture->Update(unshaded.data(), static_cast<uint32_t>(unshaded.size()));
	}

	ApplyFinishedBake();

	// While the casters stay the same, only the watched things that changed can change what is baked. Each thing that
	// got or lost a component that its shadow depends on, and each caster whose model became ready or went, changes only
	// its own shadow. While nothing changed and no block waits for its bake, there is nothing to bake.
	auto& registry = Locator::entitiesRegistry::value();
	const auto castersVersion = ecs::LandShadowCastersVersion(registry);
	const auto meshCount = Locator::resources::value().GetMeshes().Size();
	const bool castersSame = _castersVersion == castersVersion;
	// The registry keeps the things that came or went only until the next take. None: it did not keep all of them.
	std::optional<std::vector<entt::entity>> cameOrWent;
	if (!castersSame)
	{
		cameOrWent = ecs::TakeLandShadowCasterChanges(registry);
	}
	if (!_bakeAll && _castersVersion.has_value() && (castersSame || cameOrWent.has_value()))
	{
		const auto watched = testedAhead.has_value()
		                         ? std::move(*testedAhead)
		                         : FindWatchedChanges(WatchedStores(registry), TakeMarkedThings(), !castersSame);
		auto changed = cameOrWent.has_value() ? std::move(*cameOrWent) : std::vector<entt::entity> {};
		std::ranges::transform(watched, std::back_inserter(changed), [this](const WatchedChange& change) {
			return change.caster ? _baked.at(change.index).entity : _mayCast.at(change.index).entity;
		});
		if (_meshCount != meshCount)
		{
			const auto& meshes = Locator::resources::value().GetMeshes();
			for (const auto& kept : _baked)
			{
				if ((kept.from.meshReady != 0) != meshes.Contains(kept.from.mesh))
				{
					changed.push_back(kept.entity);
				}
			}
		}
		_castersVersion = castersVersion;
		_meshCount = meshCount;
		std::ranges::sort(changed);
		const auto [repeatsFirst, repeatsLast] = std::ranges::unique(changed);
		changed.erase(repeatsFirst, repeatsLast);
		_testAllCasters = false;
		TakePaintedElsewhere();
		if (changed.empty() && _dirty.empty())
		{
			return;
		}
		UpdateChanged(changed, casters);
	}
	else
	{
		// The look at all the casters keeps each caster as it is now. A later write marks the thing again.
		LookAtAllCasters(casters);
		_castersVersion = castersVersion;
		_meshCount = meshCount;
		_testAllCasters = false;
		TakePaintedElsewhere();
	}

	// A bake on the other threads that is still running holds back the next one: the blocks changed since then wait
	if (_bake.valid())
	{
		return;
	}
	std::vector<size_t> toBake;
	const bool wholeIsland = _bakeAll;
	if (_bakeAll)
	{
		toBake.resize(blocks.size());
		std::ranges::generate(toBake, [i = size_t {0}]() mutable { return i++; });
		_bakeAll = false;
		_stale.clear();
	}
	else if (!_dirty.empty())
	{
		std::ranges::sort(_dirty, [](glm::ivec2 a, glm::ivec2 b) { return a.x != b.x ? a.x < b.x : a.y < b.y; });
		const auto [first, last] = std::ranges::unique(_dirty);
		_dirty.erase(first, last);
		for (size_t i = 0; i < blocks.size(); ++i)
		{
			const auto position = blocks[i].GetBlockPosition();
			if (std::ranges::binary_search(_dirty, position,
			                               [](glm::ivec2 a, glm::ivec2 b) { return a.x != b.x ? a.x < b.x : a.y < b.y; }))
			{
				toBake.push_back(i);
				// The bake gives the block the shadows of its casters as they are now
				_stale.erase(std::pair(position.x, position.y));
			}
		}
	}
	_dirty.clear();
	if (toBake.empty())
	{
		return;
	}
	std::ranges::transform(toBake, std::back_inserter(_painted), [&blocks](size_t i) { return blocks[i].GetBlockPosition(); });

	// The triangles of every caster's model, read on this thread where the models and textures are found. A solid thing's
	// are flattened once, where they fall in the world, for every block it casts into. The bake gets its own copy of all
	// that it reads, so that the game can change its things while the bake runs.
	BakeJob job;
	job.rowsFromFarEnd = rowsFromFarEnd;
	job.side = _side;
	job.blocks.reserve(toBake.size());
	std::ranges::transform(toBake, std::back_inserter(job.blocks), [&blocks](size_t i) {
		return BakeBlock {.position = blocks[i].GetBlockPosition(), .corner = blocks[i].GetMapPosition()};
	});
	std::vector<glm::ivec2> baking;
	baking.reserve(job.blocks.size());
	std::ranges::transform(job.blocks, std::back_inserter(baking), &BakeBlock::position);
	std::ranges::sort(baking, [](glm::ivec2 a, glm::ivec2 b) { return a.x != b.x ? a.x < b.x : a.y < b.y; });
	// Only the casters whose shadows fall into the blocks to bake go into the job, in the order of the casters
	const auto castingInto = _casterBlocks.CastingInto(baking);
	job.casters.reserve(castingInto.size());
	job.cells.reserve(castingInto.size());
	for (const auto entity : castingInto)
	{
		auto* found = FindBaked(entity);
		assert(found != nullptr);
		auto& kept = *found;
		const auto shadow = static_cast<LandShadow>(kept.from.shadow);
		auto& baked = job.casters.emplace_back(BakeCaster {
		    .shadow = shadow,
		    .firstCell = static_cast<uint32_t>(job.cells.size()),
		    .cellCount = static_cast<uint32_t>(kept.cells.size()),
		    .transform = kept.transform,
		});
		job.cells.insert(job.cells.end(), kept.cells.begin(), kept.cells.end());
		if (shadow == LandShadow::Leaves)
		{
			baked.leaves = LeavesOf(kept.from.mesh);
			continue;
		}
		baked.shape = ShapeOf(kept.from.mesh);
		if (!kept.cast && !baked.shape.empty())
		{
			const auto& transform = kept.transform;
			const auto placing = PlacingOf(transform);
			const glm::vec2 origin {transform.position.x, transform.position.z};
			auto cast = std::make_shared<std::vector<glm::vec2>>();
			cast->reserve(baked.shape.size() * 3);
			for (const auto& triangle : baked.shape)
			{
				for (const auto& position : triangle.positions)
				{
					cast->push_back(shape_shadows::Cast(placing * position, origin));
				}
			}
			const auto [lowX, highX] = std::ranges::minmax(*cast | std::views::transform(&glm::vec2::x));
			const auto [lowZ, highZ] = std::ranges::minmax(*cast | std::views::transform(&glm::vec2::y));
			kept.castLow = {lowX, lowZ};
			kept.castHigh = {highX, highZ};
			kept.cast = std::move(cast);
		}
		baked.cast = kept.cast;
		baked.castLow = kept.castLow;
		baked.castHigh = kept.castHigh;
	}

	// This thread bakes the whole island before the first frame that shows it. Other threads bake the later changes
	// while the game continues. Their shadows show from the first frame after the bake ends.
	if (wholeIsland)
	{
		Apply(Bake(job));
		return;
	}
	_bakingBlocks = std::move(baking);
	_bake = std::async(std::launch::async, [job = std::move(job)] { return Bake(job); });
}

LandShadowBake::Baked LandShadowBake::BakedOf(const land_shadow_bake::Caster& caster, const ecs::Registry& registry)
{
	const auto cells = caster.Cells();
	const auto& meshes = Locator::resources::value().GetMeshes();
	return Baked {
	    .entity = caster.entity,
	    .from =
	        {
	            .shadow = static_cast<uint32_t>(caster.shadow),
	            .mesh = caster.mesh->id,
	            .meshReady = meshes.Contains(caster.mesh->id) ? 1u : 0u,
	            .position = caster.transform->position,
	            .rotation = caster.transform->rotation,
	            .scale = caster.transform->scale,
	        },
	    .cells = std::vector<uint32_t>(cells.begin(), cells.end()),
	    .transform = *caster.transform,
	    .filed = caster.resident != nullptr,
	    .shadowChangesInPlace = ecs::LandShadowChangesInPlace(registry, caster.entity),
	    .filings = caster.resident != nullptr ? caster.resident->filings : 0,
	    // A worship site and its altar stand at the temple without being filed, as things that stay put
	    .filing =
	        caster.resident != nullptr ? ecs::land_repaint::FilingOf(*caster.resident) : ecs::land_repaint::Filing::Outline,
	    .flag = FlagOf(registry, caster.entity),
	};
}

void LandShadowBake::Rebake(Baked& kept, Baked&& now)
{
	// A change of any bit, of a cell or of the filing changes the shadow. The game paints the blocks of the old and the
	// new place only for some of these changes. The other blocks keep the old shadow until a paint for another cause.
	const bool otherCells = !same_bits::SameWords(kept.cells, now.cells);
	if (!same_bits::SameBits(kept.from, now.from) || otherCells || kept.filings != now.filings)
	{
		const auto& before = kept.from;
		const auto& after = now.from;
		const ecs::land_repaint::Change change {
		    .filedAgain = otherCells || kept.filings != now.filings,
		    .moved =
		        !same_bits::SameBits(before.position, after.position) || !same_bits::SameBits(before.rotation, after.rotation),
		    .resized = !same_bits::SameBits(before.scale, after.scale),
		    .otherLook = before.shadow != after.shadow || before.mesh != after.mesh,
		    .modelReady = before.meshReady != after.meshReady,
		};
		const auto paints =
		    ecs::land_repaint::PaintsOf(ecs::land_repaint::RepaintsOf(now.filing, now.flag, now.filings), change);
		MarkBlocksOf(kept.cells, paints.oldPlace);
		MarkBlocksOf(now.cells, paints.newPlace);
		_casterBlocks.Remove(kept.entity, kept.cells);
		_casterBlocks.Add(kept.entity, now.cells);
		kept.from = now.from;
		kept.cells = std::move(now.cells);
		kept.cast.reset();
	}
	kept.transform = now.transform;
	kept.filed = now.filed;
	kept.shadowChangesInPlace = now.shadowChangesInPlace;
	kept.filings = now.filings;
	kept.filing = now.filing;
	kept.flag = now.flag;
}

LandShadowBake::Watched LandShadowBake::WatchedOf(const land_shadow_bake::MayCast& thing)
{
	return Watched {
	    .entity = thing.entity,
	    .shadow = thing.shadow,
	    .shadowChangesInPlace = thing.shadowChangesInPlace,
	    .watchesPlace = thing.transform != nullptr,
	    .transform = thing.transform != nullptr ? *thing.transform : Transform {},
	    .mesh = thing.mesh != nullptr ? thing.mesh->id : entt::id_type {0},
	};
}

void LandShadowBake::LookAtAllCasters(Casters casters)
{
	const auto& registry = Locator::entitiesRegistry::value();
	std::vector<Caster> found;
	std::vector<MayCast> mayCast;
	if (casters.leaves)
	{
		LeafKinds::Gather(registry, casters, found, mayCast);
	}
	if (casters.shapes)
	{
		ShapeKinds::Gather(registry, casters, found, mayCast);
	}
	// A thing of more than one kind is gathered once
	std::ranges::sort(found, {}, &Caster::entity);
	const auto [repeatsFirst, repeatsLast] = std::ranges::unique(found, {}, &Caster::entity);
	found.erase(repeatsFirst, repeatsLast);

	// What changed since the last bake: every block that took or takes the changed thing's shadow is baked again. A kept
	// caster that is found again loses its entity, thus the kept casters that keep their entities cast no more.
	std::vector<Baked> next;
	next.reserve(found.size());
	for (const auto& caster : found)
	{
		auto now = BakedOf(caster, registry);
		if (auto* kept = FindBaked(caster.entity); kept != nullptr)
		{
			Rebake(*kept, std::move(now));
			next.push_back(std::move(*kept));
			kept->entity = entt::null;
			continue;
		}
		MarkCameOrWent(now, true);
		_casterBlocks.Add(now.entity, now.cells);
		next.push_back(std::move(now));
	}
	for (const auto& kept : _baked)
	{
		if (kept.entity != entt::null)
		{
			MarkCameOrWent(kept, false);
			_casterBlocks.Remove(kept.entity, kept.cells);
		}
	}
	_baked = std::move(next);
	FindBakedPlaces();

	// The other things that the next frames check before they look at all the casters again
	_mayCast.clear();
	_mayCast.reserve(mayCast.size());
	std::ranges::transform(mayCast, std::back_inserter(_mayCast), &WatchedOf);
}

void LandShadowBake::UpdateChanged(std::span<const entt::entity> changed, Casters casters)
{
	// Each other thing is sorted as before. Each changed thing is sorted again as a look at all the casters sorts it.
	const auto& registry = Locator::entitiesRegistry::value();
	const ecs::LandShadowLookup shadows(registry);
	std::vector<Sorted> sorted;
	sorted.reserve(changed.size());
	std::ranges::transform(changed, std::back_inserter(sorted),
	                       [&](entt::entity entity) { return SortAgain(registry, shadows, casters, entity); });
	std::erase_if(_mayCast, [changed](const Watched& thing) { return std::ranges::binary_search(changed, thing.entity); });

	// The casters that cast no more go first. A new caster can then have the index of an entity that went.
	for (size_t i = 0; i < changed.size(); ++i)
	{
		if (std::holds_alternative<Caster>(sorted[i]))
		{
			continue;
		}
		if (const auto* kept = FindBaked(changed[i]); kept != nullptr)
		{
			MarkCameOrWent(*kept, false);
			_casterBlocks.Remove(kept->entity, kept->cells);
			RemoveBaked(changed[i]);
		}
		if (const auto* thing = std::get_if<MayCast>(&sorted[i]))
		{
			_mayCast.push_back(WatchedOf(*thing));
		}
	}
	for (size_t i = 0; i < changed.size(); ++i)
	{
		const auto* caster = std::get_if<Caster>(&sorted[i]);
		if (caster == nullptr)
		{
			continue;
		}
		auto now = BakedOf(*caster, registry);
		if (auto* kept = FindBaked(changed[i]); kept != nullptr)
		{
			Rebake(*kept, std::move(now));
			continue;
		}
		MarkCameOrWent(now, true);
		_casterBlocks.Add(now.entity, now.cells);
		AddBaked(std::move(now));
	}
}

LandShadowBake::Baked* LandShadowBake::FindBaked(entt::entity entity)
{
	const auto index = static_cast<size_t>(entt::to_entity(entity));
	if (index >= _bakedPlaces.size())
	{
		return nullptr;
	}
	const auto place = _bakedPlaces[index];
	if (place >= _baked.size() || _baked[place].entity != entity)
	{
		return nullptr;
	}
	return &_baked[place];
}

void LandShadowBake::AddBaked(Baked&& baked)
{
	const auto index = static_cast<size_t>(entt::to_entity(baked.entity));
	if (index >= _bakedPlaces.size())
	{
		_bakedPlaces.resize(index + 1, k_NoPlace);
	}
	_bakedPlaces[index] = static_cast<uint32_t>(_baked.size());
	_baked.push_back(std::move(baked));
}

void LandShadowBake::RemoveBaked(entt::entity entity)
{
	const auto index = static_cast<size_t>(entt::to_entity(entity));
	const auto place = _bakedPlaces.at(index);
	_bakedPlaces[index] = k_NoPlace;
	// The last caster moves into the place of the caster that goes
	if (place + 1 != _baked.size())
	{
		_baked[place] = std::move(_baked.back());
		_bakedPlaces.at(static_cast<size_t>(entt::to_entity(_baked[place].entity))) = place;
	}
	_baked.pop_back();
}

void LandShadowBake::FindBakedPlaces()
{
	_bakedPlaces.assign(_bakedPlaces.size(), k_NoPlace);
	for (size_t place = 0; place < _baked.size(); ++place)
	{
		const auto index = static_cast<size_t>(entt::to_entity(_baked[place].entity));
		if (index >= _bakedPlaces.size())
		{
			_bakedPlaces.resize(index + 1, k_NoPlace);
		}
		_bakedPlaces[index] = static_cast<uint32_t>(place);
	}
}

std::vector<LandShadowBake::WatchedChange>
LandShadowBake::FindWatchedChanges(const WatchedStores& stores, const std::optional<std::vector<uint32_t>>& marked,
                                   bool castersCanBeGone) const
{
	std::vector<WatchedChange> changes;
	const auto* transforms = stores.transforms;
	const auto* meshes = stores.meshes;
	const auto* residents = stores.residents;
	if (transforms == nullptr || meshes == nullptr || residents == nullptr)
	{
		// With no store, no watched thing is there
		return changes;
	}
	const auto& shadows = stores.shadows;
	const auto casterChanged = [&](const Baked& caster) {
		if (castersCanBeGone && (!transforms->contains(caster.entity) || !meshes->contains(caster.entity) ||
		                         (caster.filed && !residents->contains(caster.entity))))
		{
			return true;
		}
		return (caster.shadowChangesInPlace && static_cast<uint32_t>(shadows.Of(caster.entity)) != caster.from.shadow) ||
		       !same_bits::SameBits(transforms->get(caster.entity), caster.transform) ||
		       meshes->get(caster.entity).id != caster.from.mesh ||
		       (caster.filed && (residents->get(caster.entity).filings != caster.filings ||
		                         !same_bits::SameWords(residents->get(caster.entity).cells, caster.cells)));
	};
	if (marked.has_value())
	{
		// The registry marks each write of a caster's transform, mesh and map cells, and each loss of them. Thus only the
		// marked things and the things whose shadows can change in place can be changed casters.
		const auto testNumber = [&](uint32_t number) {
			if (number >= _bakedPlaces.size())
			{
				return;
			}
			const auto place = _bakedPlaces[number];
			if (place >= _baked.size() || entt::to_entity(_baked[place].entity) != number)
			{
				return;
			}
			if (casterChanged(_baked[place]))
			{
				changes.push_back({.caster = true, .index = place});
			}
		};
		std::ranges::for_each(*marked, testNumber);
		for (const auto* store : stores.inPlace)
		{
			if (store == nullptr)
			{
				continue;
			}
			for (const auto entity : *store)
			{
				testNumber(static_cast<uint32_t>(entt::to_entity(entity)));
			}
		}
	}
	else
	{
		const auto findChangedCasters = [&](size_t from, size_t to, std::vector<WatchedChange>& found) {
			for (size_t i = from; i < to; ++i)
			{
				if (casterChanged(_baked[i]))
				{
					found.push_back({.caster = true, .index = i});
				}
			}
		};
		// The lanes of the pool share many casters. Each share keeps its changes, and the changes then follow the order of
		// the casters, as the changes of one walk do.
		const size_t shares = _testWorkers != nullptr
		                          ? std::clamp<size_t>(_baked.size() / k_LeastCastersToShare, 1, _testWorkers->GetLaneCount())
		                          : 1;
		if (shares == 1)
		{
			findChangedCasters(0, _baked.size(), changes);
		}
		else
		{
			std::vector<std::vector<WatchedChange>> found(shares);
			const auto count = _baked.size();
			_testWorkers->ForEach(shares, [&](size_t share, size_t /*lane*/) {
				findChangedCasters(count * share / shares, count * (share + 1) / shares, found[share]);
			});
			for (const auto& share : found)
			{
				changes.insert(changes.end(), share.begin(), share.end());
			}
		}
	}
	for (size_t i = 0; i < _mayCast.size(); ++i)
	{
		const auto& thing = _mayCast[i];
		if (castersCanBeGone && thing.watchesPlace && (!transforms->contains(thing.entity) || !meshes->contains(thing.entity)))
		{
			changes.push_back({.caster = false, .index = i});
			continue;
		}
		const bool changed = (thing.shadowChangesInPlace && shadows.Of(thing.entity) != thing.shadow) ||
		                     (thing.watchesPlace && (!same_bits::SameBits(transforms->get(thing.entity), thing.transform) ||
		                                             meshes->get(thing.entity).id != thing.mesh));
		if (changed)
		{
			changes.push_back({.caster = false, .index = i});
		}
	}
	return changes;
}

std::optional<std::vector<uint32_t>> LandShadowBake::TakeMarkedThings()
{
	auto& registry = Locator::entitiesRegistry::value();
	auto marked = registry.ThingChanges().Take(ecs::ThingWrites::Reader::LandShadowBake);
	// The marks belong to one registry. Until an update uses the answer of a test, the marks that the test took are not
	// in the kept casters: then the next test looks at all the casters.
	const bool all = std::exchange(_testAllCasters, true);
	if (all || std::exchange(_markedRegistry, &registry) != &registry)
	{
		return std::nullopt;
	}
	return marked;
}

void LandShadowBake::StartWatchedTest()
{
	ForgetWatchedTest();
	if (!Locator::entitiesRegistry::has_value() || !Locator::resources::has_value())
	{
		return;
	}
	// The update tests the watched things only after a look at all the casters, and only when it does not bake the
	// whole island. The test does not start in other cases.
	auto& registry = Locator::entitiesRegistry::value();
	if (_bakeAll || !_castersVersion.has_value())
	{
		return;
	}
	// After a caster came or went, a watched thing can be gone from the stores
	const bool castersCanBeGone = _castersVersion != ecs::LandShadowCastersVersion(registry);
	if (_watchedTest == nullptr)
	{
		_watchedTest = std::make_unique<SideThread>();
	}
	if (_testWorkers == nullptr)
	{
		_testWorkers = MakeWorkerPool();
	}
	_watchedTestStarted = true;
	// The side thread reads only the stores that this thread finds now
	_watchedTest->Start([this, stores = WatchedStores(std::as_const(registry)), marked = TakeMarkedThings(), castersCanBeGone] {
		_watchedAnswer = FindWatchedChanges(stores, marked, castersCanBeGone);
	});
}

void LandShadowBake::ForgetWatchedTest()
{
	std::ignore = TakeWatchedTest();
}

std::optional<std::vector<LandShadowBake::WatchedChange>> LandShadowBake::TakeWatchedTest()
{
	if (!std::exchange(_watchedTestStarted, false))
	{
		return std::nullopt;
	}
	_watchedTest->WaitOrRun();
	return std::exchange(_watchedAnswer, std::nullopt);
}

void LandShadowBake::ApplyFinishedBake()
{
	if (!_bake.valid() || _bake.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
	{
		return;
	}
	Apply(_bake.get());
	_bakingBlocks.clear();
}

bool LandShadowBake::Bakes(glm::ivec2 block) const
{
	return _bake.valid() && std::ranges::binary_search(_bakingBlocks, block, [](glm::ivec2 a, glm::ivec2 b) {
		       return a.x != b.x ? a.x < b.x : a.y < b.y;
	       });
}

void LandShadowBake::Apply(const std::vector<BlockShades>& results)
{
	for (const auto& result : results)
	{
		const auto origin = glm::ivec2(glm::round((result.corner - _extentMin) / tree_shadows::k_BlockSize)) * _side;
		if (origin.x < 0 || origin.y < 0 || origin.x + _side > _texels.x || origin.y + _side > _texels.y ||
		    result.shades.size() != static_cast<size_t>(_side) * static_cast<size_t>(_side))
		{
			continue;
		}
		const auto originRow = _rowsFromFarEnd ? _texels.y - _side - origin.y : origin.y;
		_texture->UpdateLayerRegion(0, glm::u16vec2(origin.x, originRow), glm::u16vec2(static_cast<uint16_t>(_side)),
		                            result.shades.data(), static_cast<uint32_t>(result.shades.size()));
		_newlyBaked.push_back(result.position);
	}
}

std::vector<LandShadowBake::BlockShades> LandShadowBake::Bake(const BakeJob& job)
{
	// The blocks are baked side by side, each on its own
	std::vector<BlockShades> results(job.blocks.size());
	const auto bakeBlock = [&job, &results](size_t i, tree_shadows::Coverage& coverage) {
		const auto& block = job.blocks[i];
		coverage.fill(0);
		for (const auto& caster : job.casters)
		{
			const auto cells = std::span(job.cells).subspan(caster.firstCell, caster.cellCount);
			const bool takes = std::ranges::any_of(
			    cells, [&block](uint32_t index) { return tree_shadows::TakesShadowFrom(block.position, CellOf(index)); });
			if (!takes)
			{
				continue;
			}
			if (caster.shadow == LandShadow::Leaves)
			{
				CastLeaves(coverage, caster.transform, caster.leaves, block.corner, job.side);
			}
			else if (caster.cast && !caster.cast->empty() && caster.castHigh.x >= block.corner.x &&
			         caster.castLow.x <= block.corner.x + tree_shadows::k_BlockSize && caster.castHigh.y >= block.corner.y &&
			         caster.castLow.y <= block.corner.y + tree_shadows::k_BlockSize)
			{
				CastShape(coverage, *caster.cast, caster.shape, caster.shadow == LandShadow::ShapeBothSides, block.corner,
				          job.side);
			}
		}
		// The block's texels into the island's texture: x across it and z along its rows
		auto& shades = results[i].shades;
		const int side = job.side;
		const bool lowResolution = side < tree_shadows::k_BlockTexels;
		shades.resize(static_cast<size_t>(side) * static_cast<size_t>(side));
		for (int x = 0; x < side; ++x)
		{
			for (int z = 0; z < side; ++z)
			{
				const auto covered = coverage[ToIndex(x * side + z)];
				const int row = job.rowsFromFarEnd ? side - 1 - z : z;
				shades[ToIndex(row * side + x)] = covered != 0 ? tree_shadows::Shade(covered, lowResolution) : 255;
			}
		}
		results[i].position = block.position;
		results[i].corner = block.corner;
	};
#if defined(__EMSCRIPTEN__)
	// In the browser, the shared worker threads bake the blocks. A new thread would go past the workers that the page
	// makes at the start. Each lane has its own coverage.
	const auto pool = MakeWorkerPool();
	std::vector<std::unique_ptr<tree_shadows::Coverage>> coverages(pool->GetLaneCount());
	pool->ForEach(job.blocks.size(), [&bakeBlock, &coverages](size_t i, size_t lane) {
		auto& coverage = coverages[lane];
		if (coverage == nullptr)
		{
			coverage = std::make_unique<tree_shadows::Coverage>();
		}
		bakeBlock(i, *coverage);
	});
#else
	const auto bake = [&job, &bakeBlock](size_t first, size_t step) {
		auto coverage = std::make_unique<tree_shadows::Coverage>();
		for (size_t i = first; i < job.blocks.size(); i += step)
		{
			bakeBlock(i, *coverage);
		}
	};
	const auto workers = std::min(job.blocks.size(), static_cast<size_t>(std::max(1u, std::thread::hardware_concurrency())));
	{
		std::vector<std::jthread> threads;
		for (size_t w = 1; w < workers; ++w)
		{
			threads.emplace_back(bake, w, workers);
		}
		bake(0, workers);
	}
#endif
	return results;
}

} // namespace openblack::graphics
