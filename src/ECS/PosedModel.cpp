/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PosedModel.h"

#include <algorithm>
#include <array>

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
float HeightAt(glm::vec2 point)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
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
	VertexPlacer(const ecs::Registry& registry, entt::entity entity, const graphics::L3DMesh& mesh, size_t index,
	             const glm::mat4& model)
	    : _geometry(mesh.GetSubMeshes()[index]->GetBodyGeometry())
	    , _model(model)
	    , _bones(posed_model::BonesOf(registry, entity, mesh))
	    , _followsLand(registry.AllOf<MorphWithTerrain>(entity))
	{
		// A creature's body is its base mesh moved towards the meshes of how evil or good, thin or fat and weak or strong
		// it is drawn
		if (const auto* creature = registry.TryGet<const Creature>(entity))
		{
			if (const auto* shape = registry.TryGet<const CreatureMorph>(entity))
			{
				auto& meshes = Locator::resources::value().GetMeshes();
				const auto ids = creature_morph::MeshesOf(creature->species, shape->drawn,
				                                          [&meshes](entt::id_type id) { return meshes.Contains(id); });
				const auto subMeshOf = [&meshes, index](entt::id_type id) -> const graphics::L3DSubMesh* {
					const auto& subs = meshes.Handle(id)->GetSubMeshes();
					return index < subs.size() ? subs[index].get() : nullptr;
				};
				_morphs = {subMeshOf(ids.evilGood), subMeshOf(ids.thinFat), subMeshOf(ids.weakStrong)};
				_morph = &shape->drawn;
			}
		}
	}

	[[nodiscard]] size_t Count() const { return _geometry.positions.size(); }

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
			// Each corner moves along the model's up by how much higher the land is under it than under its origin
			const glm::vec3 origin(_model[3]);
			const glm::vec3 up(_model[1]);
			const float scale = glm::length(glm::vec3(_model[0]));
			const float rise = HeightAt({world.x, world.z}) - HeightAt({origin.x, origin.z});
			if (up.x == 0.0f && up.z == 0.0f)
			{
				world.y += rise;
			}
			else
			{
				world += up * (rise / scale);
			}
		}
		return world;
	}

private:
	const graphics::L3DSubMesh::BodyGeometry& _geometry;
	glm::mat4 _model;
	std::span<const glm::mat4> _bones;
	bool _followsLand;
	std::array<const graphics::L3DSubMesh*, 3> _morphs {};
	const creature_morph::Morph* _morph {nullptr};
};
} // namespace

void posed_model::Place(const ecs::Registry& registry, entt::entity entity, const graphics::L3DMesh& mesh, size_t index,
                        const glm::mat4& model, std::vector<glm::vec3>& corners)
{
	const VertexPlacer place(registry, entity, mesh, index, model);
	corners.clear();
	corners.reserve(place.Count());
	for (size_t i = 0; i < place.Count(); ++i)
	{
		corners.push_back(place(i));
	}
}

std::optional<glm::vec3> posed_model::PointOnSkin(const ecs::Registry& registry, entt::entity entity,
                                                  const graphics::L3DMesh& mesh, const glm::mat4& model, const SkinPoint& point)
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
	const VertexPlacer place(registry, entity, mesh, point.subMesh, model);
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
	return SkinPointPosition(corners, point.s, point.t);
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
                                                                glm::vec3 origin, glm::vec3 direction)
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
		Place(registry, entity, mesh, s, model, placed);
		const auto& geometry = subMesh.GetBodyGeometry();
		const auto hit = screen_pick::NearestSkinIntersection(placed, geometry.indices, origin, direction);
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
