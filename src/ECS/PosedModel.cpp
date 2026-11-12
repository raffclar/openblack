/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PosedModel.h"

#include <cstddef>

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "Creature/CreatureMorph.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/VillagerPose.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
float HeightAt(glm::vec2 point, MapReach* reach = nullptr)
{
	if (!Locator::terrainSystem::has_value())
	{
		return 0.0f;
	}
	if (reach != nullptr)
	{
		reach->Add(LandIslandInterface::ToMapCoords(point.x), LandIslandInterface::ToMapCoords(point.y));
	}
	return Locator::terrainSystem::value().GetHeightAt(point);
}
} // namespace

std::span<const glm::mat4> posed_model::BonesOf(const ecs::Registry& registry, entt::entity entity,
                                                const graphics::L3DMesh& mesh)
{
	const auto& rest = mesh.GetBoneMatrices();
	if (const auto* pose = registry.TryGet<const AnimalPose>(entity); pose != nullptr && pose->bones.size() == rest.size())
	{
		return pose->bones;
	}
	if (const auto* pose = registry.TryGet<const VillagerPose>(entity); pose != nullptr && pose->bones.size() == rest.size())
	{
		return pose->bones;
	}
	if (const auto* pose = registry.TryGet<const AnimatedStaticPose>(entity);
	    pose != nullptr && pose->bones.size() == rest.size())
	{
		return pose->bones;
	}
	if (const auto* animation = registry.TryGet<const CreatureAnimation>(entity);
	    animation != nullptr && animation->boneMatrices.size() == rest.size())
	{
		return animation->boneMatrices;
	}
	return rest;
}

namespace
{
/// Places a submesh's vertices one at a time as the model is drawn this frame
class VertexPlacer
{
public:
	/// With a reach, the placer adds to it each map point where it reads the land height. With a shape, a creature's
	/// body is placed with that shape in place of its own.
	VertexPlacer(const ecs::Registry& registry, entt::entity entity, const graphics::L3DMesh& mesh, size_t index,
	             const glm::mat4& model, std::span<const glm::mat4> bones, MapReach* reach = nullptr,
	             posed_model::LandFollow landFollow = posed_model::LandFollow::AsDrawn,
	             const creature_morph::Morph* shape = nullptr)
	    : _geometry(mesh.GetSubMeshes()[index]->GetBodyGeometry())
	    , _model(model)
	    , _bones(bones)
	    , _followsLand(posed_model::MovesWithLand(landFollow, registry.AllOf<MorphWithTerrain>(entity)))
	    , _reach(reach)
	{
		// The land under the model's origin, its up and its scale are the same for each corner
		if (_followsLand)
		{
			const glm::vec3 origin(_model[3]);
			_originHeight = HeightAt({origin.x, origin.z}, _reach);
			_up = glm::vec3(_model[1]);
			_scale = glm::length(glm::vec3(_model[0]));
			if (landFollow == posed_model::LandFollow::AsPicked)
			{
				_mesh = &mesh;
				_subMeshCorners.reserve(mesh.GetSubMeshes().size());
				for (const auto& subMesh : mesh.GetSubMeshes())
				{
					_subMeshCorners.push_back(
					    {.corners = subMesh->GetBodyGeometry().positions.size(), .tested = posed_model::IsDrawn(*subMesh)});
				}
				_riseOffset = posed_model::PickedRiseOffset(_subMeshCorners, index);
			}
		}
		// A creature's body is its base mesh moved towards the meshes of how evil or good, thin or fat and weak or strong
		// it is drawn
		if (const auto* creature = registry.TryGet<const Creature>(entity))
		{
			const auto* own = registry.TryGet<const CreatureMorph>(entity);
			if (const auto* drawn = shape != nullptr ? shape : (own != nullptr ? &own->drawn : nullptr))
			{
				auto& meshes = Locator::resources::value().GetMeshes();
				const auto ids = creature_morph::MeshesOf(creature->species, *drawn,
				                                          [&meshes](entt::id_type id) { return meshes.Contains(id); });
				const auto subMeshOf = [&meshes, index](entt::id_type id) -> const graphics::L3DSubMesh* {
					const auto& subs = meshes.Handle(id)->GetSubMeshes();
					return index < subs.size() ? subs[index].get() : nullptr;
				};
				_morphs = {subMeshOf(ids.evilGood), subMeshOf(ids.thinFat), subMeshOf(ids.weakStrong)};
				_morph = drawn;
			}
		}
	}

	[[nodiscard]] size_t Count() const { return _geometry.positions.size(); }

	/// Adds the bits of each input of the placement to the key, except the land heights. The geometry of a loaded
	/// mesh does not change, so its address stands for it.
	void AppendKey(std::vector<std::byte>& key) const
	{
		placed_memo::Append(key, _geometry.positions.data());
		placed_memo::Append(key, _geometry.positions.size());
		placed_memo::Append(key, _model);
		placed_memo::Append(key, _bones.size());
		placed_memo::AppendAll(key, _bones);
		placed_memo::Append(key, _followsLand);
		placed_memo::Append(key, _mesh != nullptr);
		placed_memo::Append(key, _riseOffset);
		placed_memo::Append(key, Locator::terrainSystem::has_value());
		placed_memo::Append(key, _morphs);
		placed_memo::Append(key, _morph != nullptr);
		if (_morph != nullptr)
		{
			placed_memo::Append(key, *_morph);
		}
	}

	/// Whether the placement reads the land height
	[[nodiscard]] bool ReadsLand() const { return _followsLand && Locator::terrainSystem::has_value(); }

	[[nodiscard]] glm::vec3 operator()(size_t i) const
	{
		const auto& positions = _geometry.positions;
		auto position = positions[i];
		if (_morph != nullptr)
		{
			const auto partner = [&](const graphics::L3DSubMesh* other) {
				const auto& others = other != nullptr ? other->GetBodyGeometry().positions : positions;
				return i < others.size() ? others[i] : positions[i];
			};
			position = creature_morph::Blend(position, partner(_morphs[0]), partner(_morphs[1]), partner(_morphs[2]), *_morph);
		}
		glm::vec4 local(position, 1.0f);
		const auto& bones = _geometry.bones;
		if (i < bones.size() && bones[i] != graphics::L3DSubMesh::k_NoBone && bones[i] < _bones.size())
		{
			local = _bones[bones[i]] * local;
		}
		auto world = glm::vec3(_model * local);
		if (_followsLand)
		{
			// Each corner moves along the model's up by how much higher the land is under it than under its origin. For
			// the cursor pick, the rise can come from a different corner of the model.
			const auto riseAt = RisePoint(i, world);
			const float rise = HeightAt({riseAt.x, riseAt.z}, _reach) - _originHeight;
			if (_up.x == 0.0f && _up.z == 0.0f)
			{
				world.y += rise;
			}
			else
			{
				world += _up * (rise / _scale);
			}
		}
		return world;
	}

private:
	/// The point where the land height gives the rise of the corner `i`, which is at `world`
	[[nodiscard]] glm::vec3 RisePoint(size_t i, glm::vec3 world) const
	{
		if (_mesh == nullptr)
		{
			return world;
		}
		const auto corner = posed_model::CornerAt(_subMeshCorners, _riseOffset + i);
		if (!corner.has_value())
		{
			return world;
		}
		// The model's matrix alone places the corner, as a model that follows the land has no bones
		const auto& position = _mesh->GetSubMeshes()[corner->first]->GetBodyGeometry().positions[corner->second];
		return glm::vec3(_model * glm::vec4(position, 1.0f));
	}

	const graphics::L3DSubMesh::BodyGeometry& _geometry;
	glm::mat4 _model;
	std::span<const glm::mat4> _bones;
	bool _followsLand;
	MapReach* _reach;
	float _originHeight {0.0f};
	glm::vec3 _up {0.0f};
	float _scale {1.0f};
	std::array<const graphics::L3DSubMesh*, 3> _morphs {};
	const creature_morph::Morph* _morph {nullptr};
	/// The model, only when the rises of its corners are read as the cursor pick reads them
	const graphics::L3DMesh* _mesh {nullptr};
	std::vector<posed_model::SubMeshCorners> _subMeshCorners;
	size_t _riseOffset {0};
};
} // namespace

size_t posed_model::PickedRiseOffset(std::span<const SubMeshCorners> subMeshes, size_t index)
{
	size_t offset = 0;
	for (const auto& subMesh : subMeshes.first(std::min(index, subMeshes.size())))
	{
		if (subMesh.tested)
		{
			offset += subMesh.corners;
		}
	}
	return offset;
}

std::optional<std::pair<size_t, size_t>> posed_model::CornerAt(std::span<const SubMeshCorners> subMeshes, size_t place)
{
	for (size_t s = 0; s < subMeshes.size(); ++s)
	{
		if (place < subMeshes[s].corners)
		{
			return std::pair {s, place};
		}
		place -= subMeshes[s].corners;
	}
	return std::nullopt;
}

void posed_model::Place(const ecs::Registry& registry, entt::entity entity, const graphics::L3DMesh& mesh, size_t index,
                        const glm::mat4& model, std::vector<glm::vec3>& corners)
{
	Place(registry, entity, mesh, index, model, posed_model::BonesOf(registry, entity, mesh), corners);
}

void posed_model::Place(const ecs::Registry& registry, entt::entity entity, const graphics::L3DMesh& mesh, size_t index,
                        const glm::mat4& model, std::span<const glm::mat4> bones, std::vector<glm::vec3>& corners,
                        const creature_morph::Morph* shape)
{
	const VertexPlacer place(registry, entity, mesh, index, model, bones, nullptr, LandFollow::AsDrawn, shape);
	corners.clear();
	corners.reserve(place.Count());
	for (size_t i = 0; i < place.Count(); ++i)
	{
		corners.push_back(place(i));
	}
}

placed_memo::Placed posed_model::Place(placed_memo::Memo& memo, const ecs::Registry& registry, entt::entity entity,
                                       const graphics::L3DMesh& mesh, size_t index, const glm::mat4& model,
                                       LandFollow landFollow)
{
	// The reach holds the map points where the placer reads the land height
	MapReach reach;
	const VertexPlacer placer(registry, entity, mesh, index, model, posed_model::BonesOf(registry, entity, mesh), &reach,
	                          landFollow);
	auto& key = memo.KeyScratch();
	key.clear();
	placer.AppendKey(key);
	const auto place = [&](std::vector<glm::vec3>& corners) -> std::optional<MapReach> {
		corners.clear();
		corners.reserve(placer.Count());
		for (size_t i = 0; i < placer.Count(); ++i)
		{
			corners.push_back(placer(i));
		}
		return placer.ReadsLand() ? std::optional(reach) : std::nullopt;
	};
	const auto readLand = [](const MapReach& within, std::vector<std::byte>& bytes) {
		return Locator::terrainSystem::value().HeightInputs(within, bytes);
	};
	return memo.Get({.entity = entity, .geometry = &mesh.GetSubMeshes()[index]->GetBodyGeometry()}, key, place, readLand);
}

std::optional<glm::vec3> posed_model::PointOnSkin(const ecs::Registry& registry, entt::entity entity,
                                                  const graphics::L3DMesh& mesh, const glm::mat4& model, const SkinPoint& point)
{
	const auto corners = CornersOnSkin(registry, entity, mesh, model, BonesOf(registry, entity, mesh), point);
	if (!corners.has_value())
	{
		return std::nullopt;
	}
	return SkinPointPosition(*corners, point.s, point.t);
}

std::optional<std::array<glm::vec3, 3>> posed_model::CornersOnSkin(const ecs::Registry& registry, entt::entity entity,
                                                                   const graphics::L3DMesh& mesh, const glm::mat4& model,
                                                                   std::span<const glm::mat4> bones, const SkinPoint& point,
                                                                   const creature_morph::Morph* shape)
{
	if (point.subMesh >= mesh.GetSubMeshes().size())
	{
		return std::nullopt;
	}
	const auto& indices = mesh.GetSubMeshes()[point.subMesh]->GetBodyGeometry().indices;
	if (static_cast<size_t>(point.firstIndex) + 2 >= indices.size())
	{
		return std::nullopt;
	}
	const VertexPlacer place(registry, entity, mesh, point.subMesh, model, bones, nullptr, LandFollow::AsDrawn, shape);
	std::array<glm::vec3, 3> corners {};
	for (size_t c = 0; c < corners.size(); ++c)
	{
		const auto vertex = indices[point.firstIndex + c];
		if (vertex >= place.Count())
		{
			return std::nullopt;
		}
		corners.at(c) = place(vertex);
	}
	return corners;
}

glm::vec3 posed_model::SkinPointPosition(const std::array<glm::vec3, 3>& corners, float s, float t)
{
	return corners[0] + ((corners[1] - corners[0]) * s) + ((corners[2] - corners[0]) * t);
}

bool posed_model::IsDrawn(const graphics::L3DSubMesh& subMesh)
{
	const auto flags = subMesh.GetFlags();
	return (flags.lodMask & 1) != 0 && flags.status == 0;
}

std::optional<posed_model::SkinHit> posed_model::NearestSkinHit(const ecs::Registry& registry, entt::entity entity,
                                                                const graphics::L3DMesh& mesh, const glm::mat4& model,
                                                                glm::vec3 origin, glm::vec3 direction, bool behindAllowed)
{
	return NearestSkinHit(registry, entity, mesh, model, BonesOf(registry, entity, mesh), origin, direction, behindAllowed);
}

std::optional<posed_model::SkinHit> posed_model::NearestSkinHit(const ecs::Registry& registry, entt::entity entity,
                                                                const graphics::L3DMesh& mesh, const glm::mat4& model,
                                                                std::span<const glm::mat4> bones, glm::vec3 origin,
                                                                glm::vec3 direction, bool behindAllowed)
{
	std::optional<SkinHit> nearest;
	std::vector<glm::vec3> placed;
	const auto& skinOrder = mesh.GetSkinOrder();
	for (size_t s = 0; s < mesh.GetSubMeshes().size(); ++s)
	{
		const auto& subMesh = *mesh.GetSubMeshes()[s];
		// The submeshes of the nearest level of detail
		if ((subMesh.GetFlags().lodMask & 1) == 0)
		{
			continue;
		}
		Place(registry, entity, mesh, s, model, bones, placed);
		const auto& geometry = subMesh.GetBodyGeometry();
		const auto hit = screen_pick::NearestSkinIntersection(placed, geometry.indices, origin, direction, behindAllowed);
		if (!hit.has_value() || (nearest.has_value() && !(hit->distance < nearest->hit.distance)))
		{
			continue;
		}
		const auto uvAt = [&geometry, &hit](uint32_t corner) {
			const auto index = geometry.indices[hit->firstIndex + corner];
			return index < geometry.uvs.size() ? geometry.uvs[index] : glm::vec2(0.0f);
		};
		// The skin is looked up by the place of the triangle's primitive in its submesh, but among the first submesh's
		// primitives (the same for a creature, whose nearest body is its first submesh), counted among the model's skins
		// from the last that matches
		uint32_t skin = 0;
		const auto& primitives = subMesh.GetPrimitives();
		const auto within = std::ranges::find_if(primitives, [&hit](const auto& primitive) {
			return hit->firstIndex >= primitive.indicesOffset &&
			       hit->firstIndex < primitive.indicesOffset + primitive.indicesCount;
		});
		const auto& firstPrimitives = mesh.GetSubMeshes().front()->GetPrimitives();
		if (const auto place = static_cast<size_t>(std::distance(primitives.begin(), within));
		    within != primitives.end() && place < firstPrimitives.size())
		{
			for (size_t k = 0; k < skinOrder.size(); ++k)
			{
				if (skinOrder[k] == firstPrimitives[place].skinID)
				{
					skin = static_cast<uint32_t>(k);
				}
			}
		}
		nearest = SkinHit {.hit = *hit, .uvs = {uvAt(0), uvAt(1), uvAt(2)}, .skin = skin, .subMesh = static_cast<uint32_t>(s)};
	}
	return nearest;
}
