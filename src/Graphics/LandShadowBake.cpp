/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandShadowBake.h"

#include <algorithm>
#include <iterator>
#include <ranges>
#include <span>
#include <thread>

#include <glm/mat3x3.hpp>

#include "3D/L3DMesh.h"
#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/SpellDispenser.h"
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

namespace
{

constexpr int k_BlockTexels = tree_shadows::k_BlockTexels;

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

/// Gathers the casters of one kind of thing. A thing has to be in the map's cells to cast (one carried by the hand
/// casts none), except a worship site and its altar, which stand at the temple without being filed in them.
template <typename Kind>
void GatherCasters(ecs::Registry& registry, LandShadowBake::Casters wanted, std::vector<Caster>& casters)
{
	registry.Each<const Kind, const MeshComponent, const Transform>(
	    // Kinds without data aren't passed in
	    [&](entt::entity entity, const auto&... /*unused*/) {
		    const auto& mesh = registry.Get<const MeshComponent>(entity);
		    const auto& transform = registry.Get<const Transform>(entity);
		    const auto shadow = ecs::LandShadowOf(registry, entity);
		    const bool leaves = shadow == LandShadow::Leaves;
		    if (shadow == LandShadow::None || (leaves ? !wanted.leaves : !wanted.shapes))
		    {
			    return;
		    }
		    const auto* resident = registry.TryGet<const MapCellResident>(entity);
		    uint32_t cellUnder = 0;
		    if (resident == nullptr)
		    {
			    if (!registry.AnyOf<ecs::components::WorshipSite, ecs::components::WorshipAltar>(entity))
			    {
				    return;
			    }
			    const auto cell = map_coords::CellOf(transform.position);
			    if (static_cast<uint32_t>(cell.x) >= map_coords::k_MapCells ||
			        static_cast<uint32_t>(cell.y) >= map_coords::k_MapCells)
			    {
				    return;
			    }
			    cellUnder = static_cast<uint32_t>(cell.x) * map_coords::k_MapCells + static_cast<uint32_t>(cell.y);
		    }
		    casters.push_back({.entity = entity,
		                       .shadow = shadow,
		                       .mesh = &mesh,
		                       .transform = &transform,
		                       .resident = resident,
		                       .cellUnder = cellUnder});
	    });
}

/// Folds bytes into a running FNV-1a hash
void Fold(uint64_t& hash, std::span<const std::byte> bytes)
{
	for (const auto byte : bytes)
	{
		hash ^= static_cast<uint64_t>(byte);
		hash *= 0x100000001B3ull;
	}
}

uint64_t SignatureOf(const Caster& caster, bool meshReady)
{
	uint64_t hash = 0xCBF29CE484222325ull;
	Fold(hash, std::as_bytes(std::span(&caster.shadow, 1)));
	Fold(hash, std::as_bytes(std::span(&caster.mesh->id, 1)));
	Fold(hash, std::as_bytes(std::span(&meshReady, 1)));
	Fold(hash, std::as_bytes(std::span(&caster.transform->position, 1)));
	Fold(hash, std::as_bytes(std::span(&caster.transform->rotation, 1)));
	Fold(hash, std::as_bytes(std::span(&caster.transform->scale, 1)));
	Fold(hash, std::as_bytes(caster.Cells()));
	return hash;
}

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
                std::span<const LandShadowBake::LeafTriangle> leaves, glm::vec2 blockCorner)
{
	const auto placing = PlacingOf(transform);
	const glm::vec2 foot {transform.position.x, transform.position.z};
	for (const auto& triangle : leaves)
	{
		std::array<tree_shadows::Corner, 3> corners;
		for (size_t k = 0; k < 3; ++k)
		{
			corners[k] = tree_shadows::Flatten(placing * triangle.positions[k], foot, blockCorner, triangle.uvs[k]);
		}
		if (tree_shadows::Reaches(corners))
		{
			tree_shadows::Fill(coverage, corners, *triangle.mask);
		}
	}
}

/// Casts a solid thing's flattened triangles into a block's samples
void CastShape(tree_shadows::Coverage& coverage, std::span<const glm::vec2> cast,
               std::span<const LandShadowBake::ShapeTriangle> shape, bool allBothSides, glm::vec2 blockCorner)
{
	for (size_t t = 0; t < shape.size(); ++t)
	{
		std::array<shape_shadows::Sample, 3> corners;
		float lowRow = 0.0f;
		float highRow = 0.0f;
		float lowColumn = 0.0f;
		float highColumn = 0.0f;
		for (size_t k = 0; k < 3; ++k)
		{
			corners[k] = shape_shadows::SampleOf(cast[t * 3 + k], blockCorner);
			lowRow = k == 0 ? corners[k].row : std::min(lowRow, corners[k].row);
			highRow = k == 0 ? corners[k].row : std::max(highRow, corners[k].row);
			lowColumn = k == 0 ? corners[k].column : std::min(lowColumn, corners[k].column);
			highColumn = k == 0 ? corners[k].column : std::max(highColumn, corners[k].column);
		}
		// Nothing of a triangle wholly before or past the block's samples is covered
		if (highRow < 1.0f || lowRow >= static_cast<float>(shape_shadows::k_Rows) || highColumn < -1.0f ||
		    lowColumn > static_cast<float>(shape_shadows::k_Columns + 1))
		{
			continue;
		}
		shape_shadows::Fill(coverage, corners, allBothSides || shape[t].bothSides);
	}
}

} // namespace

LandShadowBake::LandShadowBake() = default;
LandShadowBake::~LandShadowBake() = default;

void LandShadowBake::Reset()
{
	_texture.reset();
	_texels = {0, 0};
	_baked.clear();
	_leaves.clear();
	_shapes.clear();
	_dirty.clear();
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
			const auto end = std::min<size_t>(primitive.indicesOffset + primitive.indicesCount, surface.indices.size());
			for (size_t i = primitive.indicesOffset; i + 2 < end; i += 3)
			{
				LeafTriangle triangle {.positions = {}, .uvs = {}, .mask = mask};
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

void LandShadowBake::MarkBlocksOf(std::span<const uint32_t> cells)
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
				_dirty.emplace_back(x, z);
			}
		}
	}
}

void LandShadowBake::Update(bool rowsFromFarEnd, Casters casters)
{
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
	const auto texels = glm::u16vec2(blocksAcross * static_cast<float>(k_BlockTexels));
	if (_texture == nullptr || texels != _texels || extent.minimum != _extentMin || rowsFromFarEnd != _rowsFromFarEnd ||
	    casters != _casters)
	{
		Reset();
		_texels = texels;
		_extentMin = extent.minimum;
		_rowsFromFarEnd = rowsFromFarEnd;
		_casters = casters;
		_texture = std::make_unique<Texture2D>("Land Shadows");
		_texture->CreateWithinFrame(texels.x, texels.y, 1, TextureFormat::R8, Wrapping::ClampEdge, Filter::Nearest, nullptr);
		const std::vector<uint8_t> unshaded(static_cast<size_t>(texels.x) * texels.y, 255);
		_texture->Update(unshaded.data(), static_cast<uint32_t>(unshaded.size()));
	}

	auto& registry = Locator::entitiesRegistry::value();
	std::vector<Caster> found;
	if (casters.leaves)
	{
		GatherCasters<ecs::components::Tree>(registry, casters, found);
		GatherCasters<ecs::components::BigForest>(registry, casters, found);
	}
	if (casters.shapes)
	{
		using namespace ecs::components;
		GatherCasters<Abode>(registry, casters, found);
		GatherCasters<Temple>(registry, casters, found);
		GatherCasters<WorshipSite>(registry, casters, found);
		GatherCasters<WorshipAltar>(registry, casters, found);
		GatherCasters<SpellDispenser>(registry, casters, found);
		GatherCasters<Feature>(registry, casters, found);
		GatherCasters<MobileStatic>(registry, casters, found);
		GatherCasters<TeleportStone>(registry, casters, found);
		GatherCasters<MobileObject>(registry, casters, found);
		GatherCasters<Ball>(registry, casters, found);
		GatherCasters<RewardOnLand>(registry, casters, found);
	}
	// A thing of more than one kind is gathered once
	std::ranges::sort(found, {}, &Caster::entity);
	const auto [repeatsFirst, repeatsLast] = std::ranges::unique(found, {}, &Caster::entity);
	found.erase(repeatsFirst, repeatsLast);
	const auto& meshes = Locator::resources::value().GetMeshes();

	// What changed since the last bake: every block that took or takes the changed thing's shadow is baked again
	for (auto& [entity, baked] : _baked)
	{
		baked.seen = false;
	}
	for (const auto& caster : found)
	{
		const auto signature = SignatureOf(caster, meshes.Contains(caster.mesh->id));
		const auto cells = caster.Cells();
		auto previous = _baked.find(caster.entity);
		if (previous == _baked.end())
		{
			MarkBlocksOf(cells);
			_baked.emplace(
			    caster.entity,
			    Baked {.signature = signature, .cells = std::vector<uint32_t>(cells.begin(), cells.end()), .seen = true});
			continue;
		}
		previous->second.seen = true;
		if (previous->second.signature != signature)
		{
			MarkBlocksOf(previous->second.cells);
			MarkBlocksOf(cells);
			previous->second.signature = signature;
			previous->second.cells.assign(cells.begin(), cells.end());
			previous->second.cast.clear();
		}
	}
	for (auto it = _baked.begin(); it != _baked.end();)
	{
		if (!it->second.seen)
		{
			MarkBlocksOf(it->second.cells);
			it = _baked.erase(it);
		}
		else
		{
			++it;
		}
	}

	std::vector<size_t> toBake;
	if (_bakeAll)
	{
		toBake.resize(blocks.size());
		std::ranges::generate(toBake, [i = size_t {0}]() mutable { return i++; });
		_bakeAll = false;
	}
	else if (!_dirty.empty())
	{
		std::ranges::sort(_dirty, [](glm::ivec2 a, glm::ivec2 b) { return a.x != b.x ? a.x < b.x : a.y < b.y; });
		const auto [first, last] = std::ranges::unique(_dirty);
		_dirty.erase(first, last);
		for (size_t i = 0; i < blocks.size(); ++i)
		{
			if (std::ranges::binary_search(_dirty, blocks[i].GetBlockPosition(),
			                               [](glm::ivec2 a, glm::ivec2 b) { return a.x != b.x ? a.x < b.x : a.y < b.y; }))
			{
				toBake.push_back(i);
			}
		}
	}
	_dirty.clear();
	if (toBake.empty())
	{
		return;
	}

	// The triangles of every caster's model, read on this thread where the models and textures are found. A solid thing's
	// are flattened once, where they fall in the world, for every block it casts into.
	struct Prepared
	{
		std::span<const LeafTriangle> leaves;
		std::span<const ShapeTriangle> shape;
		const Baked* baked {nullptr};
	};
	std::vector<glm::ivec2> baking;
	baking.reserve(toBake.size());
	std::ranges::transform(toBake, std::back_inserter(baking), [&blocks](size_t i) { return blocks[i].GetBlockPosition(); });
	std::ranges::sort(baking, [](glm::ivec2 a, glm::ivec2 b) { return a.x != b.x ? a.x < b.x : a.y < b.y; });
	const auto castsIntoBaking = [&baking](const Caster& caster) {
		return std::ranges::any_of(caster.Cells(), [&baking](uint32_t index) {
			const auto cell = CellOf(index);
			const auto alongX = tree_shadows::BlocksTakingShadowFrom(cell.x);
			const auto alongZ = tree_shadows::BlocksTakingShadowFrom(cell.y);
			for (int x = alongX.x; x <= alongX.y; ++x)
			{
				for (int z = alongZ.x; z <= alongZ.y; ++z)
				{
					if (std::ranges::binary_search(baking, glm::ivec2(x, z), [](glm::ivec2 a, glm::ivec2 b) {
						    return a.x != b.x ? a.x < b.x : a.y < b.y;
					    }))
					{
						return true;
					}
				}
			}
			return false;
		});
	};
	std::vector<Prepared> prepared(found.size());
	for (size_t c = 0; c < found.size(); ++c)
	{
		const auto& caster = found[c];
		if (!castsIntoBaking(caster))
		{
			continue;
		}
		if (caster.shadow == LandShadow::Leaves)
		{
			prepared[c].leaves = LeavesOf(caster.mesh->id);
			continue;
		}
		prepared[c].shape = ShapeOf(caster.mesh->id);
		auto& baked = _baked.at(caster.entity);
		prepared[c].baked = &baked;
		if (!baked.cast.empty() || prepared[c].shape.empty())
		{
			continue;
		}
		const auto& transform = *caster.transform;
		const auto placing = PlacingOf(transform);
		const glm::vec2 origin {transform.position.x, transform.position.z};
		baked.cast.reserve(prepared[c].shape.size() * 3);
		for (const auto& triangle : prepared[c].shape)
		{
			for (const auto& position : triangle.positions)
			{
				baked.cast.push_back(shape_shadows::Cast(placing * position, origin));
			}
		}
		const auto [lowX, highX] = std::ranges::minmax(baked.cast | std::views::transform(&glm::vec2::x));
		const auto [lowZ, highZ] = std::ranges::minmax(baked.cast | std::views::transform(&glm::vec2::y));
		baked.castLow = {lowX, lowZ};
		baked.castHigh = {highX, highZ};
	}

	// The blocks are baked side by side, each on its own
	struct Result
	{
		size_t block {0};
		std::vector<uint8_t> shades;
	};
	std::vector<Result> results(toBake.size());
	const auto bake = [&](size_t first, size_t step) {
		auto coverage = std::make_unique<tree_shadows::Coverage>();
		for (size_t i = first; i < toBake.size(); i += step)
		{
			const auto& block = blocks[toBake[i]];
			const auto blockPosition = block.GetBlockPosition();
			const auto blockCorner = block.GetMapPosition();
			coverage->fill(0);
			for (size_t c = 0; c < found.size(); ++c)
			{
				const auto& caster = found[c];
				const bool takes = std::ranges::any_of(caster.Cells(), [blockPosition](uint32_t index) {
					return tree_shadows::TakesShadowFrom(blockPosition, CellOf(index));
				});
				if (!takes)
				{
					continue;
				}
				if (caster.shadow == LandShadow::Leaves)
				{
					CastLeaves(*coverage, *caster.transform, prepared[c].leaves, blockCorner);
				}
				else if (const auto* baked = prepared[c].baked;
				         baked != nullptr && !baked->cast.empty() && baked->castHigh.x >= blockCorner.x &&
				         baked->castLow.x <= blockCorner.x + tree_shadows::k_BlockSize && baked->castHigh.y >= blockCorner.y &&
				         baked->castLow.y <= blockCorner.y + tree_shadows::k_BlockSize)
				{
					CastShape(*coverage, baked->cast, prepared[c].shape, caster.shadow == LandShadow::ShapeBothSides,
					          blockCorner);
				}
			}
			// The block's texels into the island's texture: x across it and z along its rows
			auto& shades = results[i].shades;
			shades.resize(static_cast<size_t>(k_BlockTexels) * k_BlockTexels);
			for (int x = 0; x < k_BlockTexels; ++x)
			{
				for (int z = 0; z < k_BlockTexels; ++z)
				{
					const auto covered = (*coverage)[static_cast<size_t>(x * k_BlockTexels + z)];
					const int row = rowsFromFarEnd ? k_BlockTexels - 1 - z : z;
					shades[static_cast<size_t>(row * k_BlockTexels + x)] = covered != 0 ? tree_shadows::Shade(covered) : 255;
				}
			}
			results[i].block = toBake[i];
		}
	};
	const auto workers = std::min<size_t>(toBake.size(), std::max(1u, std::thread::hardware_concurrency()));
	{
		std::vector<std::jthread> threads;
		for (size_t w = 1; w < workers; ++w)
		{
			threads.emplace_back(bake, w, workers);
		}
		bake(0, workers);
	}

	for (const auto& result : results)
	{
		const auto blockCorner = blocks[result.block].GetMapPosition();
		const auto origin = glm::ivec2(glm::round((blockCorner - _extentMin) / tree_shadows::k_BlockSize)) * k_BlockTexels;
		if (origin.x < 0 || origin.y < 0 || origin.x + k_BlockTexels > _texels.x || origin.y + k_BlockTexels > _texels.y)
		{
			continue;
		}
		const auto originRow = rowsFromFarEnd ? _texels.y - k_BlockTexels - origin.y : origin.y;
		_texture->UpdateLayerRegion(0, glm::u16vec2(origin.x, originRow), glm::u16vec2(k_BlockTexels), result.shades.data(),
		                            static_cast<uint32_t>(result.shades.size()));
	}
}

} // namespace openblack::graphics
