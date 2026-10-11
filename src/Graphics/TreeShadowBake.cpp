/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TreeShadowBake.h"

#include <algorithm>
#include <span>
#include <thread>

#include <glm/mat3x3.hpp>

#include "3D/L3DMesh.h"
#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Registry.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::graphics
{

using ecs::components::BigForest;
using ecs::components::MapCellResident;
using ecs::components::Transform;
using ecs::components::Tree;
using MeshComponent = ecs::components::Mesh;

namespace
{

constexpr int k_BlockTexels = tree_shadows::k_BlockTexels;

/// A caster of a baked shadow: a tree or a big forest standing in the map's cells
struct Caster
{
	entt::entity entity;
	const MeshComponent* mesh;
	const Transform* transform;
	const MapCellResident* resident;
};

template <typename Kind>
void GatherCasters(std::vector<Caster>& casters)
{
	Locator::entitiesRegistry::value().Each<const Kind, const MeshComponent, const Transform, const MapCellResident>(
	    [&casters](entt::entity entity, const Kind& /*unused*/, const MeshComponent& mesh, const Transform& transform,
	               const MapCellResident& resident) {
		    casters.push_back({.entity = entity, .mesh = &mesh, .transform = &transform, .resident = &resident});
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
	Fold(hash, std::as_bytes(std::span(&caster.mesh->id, 1)));
	Fold(hash, std::as_bytes(std::span(&meshReady, 1)));
	Fold(hash, std::as_bytes(std::span(&caster.transform->position, 1)));
	Fold(hash, std::as_bytes(std::span(&caster.transform->rotation, 1)));
	Fold(hash, std::as_bytes(std::span(&caster.transform->scale, 1)));
	Fold(hash, std::as_bytes(std::span(caster.resident->cells)));
	return hash;
}

glm::ivec2 CellOf(uint32_t index)
{
	return {static_cast<int>(index / map_coords::k_MapCells), static_cast<int>(index % map_coords::k_MapCells)};
}

/// Casts a caster's triangles into a block's texels
void CastInto(tree_shadows::Coverage& coverage, const Caster& caster, std::span<const TreeShadowBake::Triangle> shape,
              glm::vec2 blockCorner)
{
	const auto& transform = *caster.transform;
	const glm::mat3 placing =
	    transform.rotation * glm::mat3(glm::vec3(transform.scale.x, 0.0f, 0.0f), glm::vec3(0.0f, transform.scale.y, 0.0f),
	                                   glm::vec3(0.0f, 0.0f, transform.scale.z));
	const glm::vec2 foot {transform.position.x, transform.position.z};
	for (const auto& triangle : shape)
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

} // namespace

TreeShadowBake::TreeShadowBake() = default;
TreeShadowBake::~TreeShadowBake() = default;

void TreeShadowBake::Reset()
{
	_texture.reset();
	_texels = {0, 0};
	_baked.clear();
	_shapes.clear();
	_dirty.clear();
	_bakeAll = true;
}

std::span<const TreeShadowBake::Triangle> TreeShadowBake::ShapeOf(entt::id_type meshId)
{
	if (const auto found = _shapes.find(meshId); found != _shapes.end())
	{
		return found->second;
	}
	auto& shape = _shapes[meshId];
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(meshId))
	{
		return shape;
	}
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
				Triangle triangle {.positions = {}, .uvs = {}, .mask = mask};
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
					shape.push_back(triangle);
				}
			}
		}
	}
	return shape;
}

void TreeShadowBake::MarkBlocksOf(const std::vector<uint32_t>& cells)
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

void TreeShadowBake::Update(bool rowsFromFarEnd)
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
	if (_texture == nullptr || texels != _texels || extent.minimum != _extentMin || rowsFromFarEnd != _rowsFromFarEnd)
	{
		Reset();
		_texels = texels;
		_extentMin = extent.minimum;
		_rowsFromFarEnd = rowsFromFarEnd;
		_texture = std::make_unique<Texture2D>("Tree Shadows");
		_texture->CreateWithinFrame(texels.x, texels.y, 1, TextureFormat::R8, Wrapping::ClampEdge, Filter::Nearest, nullptr);
		const std::vector<uint8_t> unshaded(static_cast<size_t>(texels.x) * texels.y, 255);
		_texture->Update(unshaded.data(), static_cast<uint32_t>(unshaded.size()));
	}

	std::vector<Caster> casters;
	GatherCasters<Tree>(casters);
	GatherCasters<BigForest>(casters);
	const auto& meshes = Locator::resources::value().GetMeshes();

	// What changed since the last bake: every block that took or takes the changed tree's shadow is baked again
	for (auto& [entity, baked] : _baked)
	{
		baked.seen = false;
	}
	for (const auto& caster : casters)
	{
		const auto signature = SignatureOf(caster, meshes.Contains(caster.mesh->id));
		auto found = _baked.find(caster.entity);
		if (found == _baked.end())
		{
			MarkBlocksOf(caster.resident->cells);
			_baked.emplace(caster.entity, Baked {.signature = signature, .cells = caster.resident->cells, .seen = true});
			continue;
		}
		found->second.seen = true;
		if (found->second.signature != signature)
		{
			MarkBlocksOf(found->second.cells);
			MarkBlocksOf(caster.resident->cells);
			found->second.signature = signature;
			found->second.cells = caster.resident->cells;
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

	// The triangles of every caster's model, read on this thread where the models and textures are found
	std::vector<std::span<const Triangle>> shapes;
	shapes.reserve(casters.size());
	for (const auto& caster : casters)
	{
		shapes.emplace_back(ShapeOf(caster.mesh->id));
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
			for (size_t c = 0; c < casters.size(); ++c)
			{
				const bool takes = std::ranges::any_of(casters[c].resident->cells, [blockPosition](uint32_t index) {
					return tree_shadows::TakesShadowFrom(blockPosition, CellOf(index));
				});
				if (takes)
				{
					CastInto(*coverage, casters[c], shapes[c], blockCorner);
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
