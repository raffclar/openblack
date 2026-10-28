/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "HighDetailSystem.h"

#include <algorithm>
#include <array>
#include <bit>
#include <string>
#include <vector>

#include <glm/gtx/transform.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/PhysicsDrawMatrix.h"
#include "Common/GameRandom.h"
#include "ECS/Components/DetailMeshes.h"
#include "ECS/Components/HighDetail.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillagerPose.h"
#include "ECS/Registry.h"
#include "ECS/SwimmerRules.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "ECS/Systems/WaterRingSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/MeshDetail.h"
#include "Graphics/ViewFrustum.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
namespace rules = openblack::ecs::high_detail_rules;

namespace
{
/// The ordinary models that have a detailed one
constexpr std::array k_ModelsWithDetail = {MeshId::PersonNorseMaleA1, MeshId::PersonNorseFemaleA1, MeshId::PersonBoyWhite1,
                                           MeshId::PersonAnimalTrainer};

/// A model's id in the mesh cache, loaded from the misc folder the first time it is needed
std::optional<entt::id_type> LoadMiscModel(std::string_view file)
{
	if (!Locator::resources::has_value() || !Locator::filesystem::has_value())
	{
		return std::nullopt;
	}
	const auto name = "misc/" + std::string(file);
	const auto id = resources::HashIdentifier(name);
	auto& meshes = Locator::resources::value().GetMeshes();
	if (meshes.Contains(id))
	{
		return id;
	}
	auto& fileSystem = Locator::filesystem::value();
	const auto path = fileSystem.GetPath<filesystem::Path::Misc>() / file;
	if (!fileSystem.Exists(path))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Model {} not found", path.generic_string());
		return std::nullopt;
	}
	meshes.Load(id, resources::L3DLoader::FromDiskTag {}, path);
	return id;
}
} // namespace

void HighDetailSystem::LoadModels()
{
	for (const auto model : k_ModelsWithDetail)
	{
		const auto detailed = rules::DetailedModelFor(model);
		if (!detailed.has_value() || !LoadMiscModel(detailed->file).has_value())
		{
			continue;
		}
		// The pieces load in order until one is not there, as at the first use
		for (size_t piece = 0; piece < villager_eyes::k_PieceCount; ++piece)
		{
			if (!LoadMiscModel(villager_eyes::PieceFile(detailed->face, static_cast<villager_eyes::Piece>(piece))).has_value())
			{
				break;
			}
		}
	}
}

void HighDetailSystem::Make(entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<HighDetail>(thing))
	{
		return;
	}
	auto& detail = registry.Assign<HighDetail>(thing);
	auto* mesh = registry.TryGet<Mesh>(thing);
	if (mesh == nullptr)
	{
		return;
	}
	// Known by the model it wears close up
	auto* detailMeshes = registry.TryGet<DetailMeshes>(thing);
	const auto worn = detailMeshes != nullptr ? detailMeshes->meshes[0] : mesh->id;
	const auto model =
	    std::ranges::find_if(k_ModelsWithDetail, [worn](MeshId id) { return resources::HashIdentifier(id) == worn; });
	if (model == k_ModelsWithDetail.end())
	{
		return;
	}
	const auto detailed = rules::DetailedModelFor(*model);
	const auto id = LoadMiscModel(detailed->file);
	if (!id.has_value())
	{
		return;
	}
	detail.usualModel = mesh->id;
	detail.face = detailed->face;
	mesh->id = *id;
	// It wears the detailed model at every distance
	if (detailMeshes != nullptr)
	{
		detail.usualDetailModels = detailMeshes->meshes;
		detailMeshes->meshes = {*id, *id, *id};
	}
	// Only a detailed model with places on its head for both eyes has eyes, and only once all their pieces load
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto loaded = [face = detailed->face]() {
		for (size_t piece = 0; piece < villager_eyes::k_PieceCount; ++piece)
		{
			if (!LoadMiscModel(villager_eyes::PieceFile(face, static_cast<villager_eyes::Piece>(piece))).has_value())
			{
				return false;
			}
		}
		return true;
	};
	if (const auto detailedModel = meshes.Handle(*id);
	    detailedModel->IsContainsEBone() && detailedModel->GetBoneFrames().size() >= 2 && loaded())
	{
		detail.eyes = villager_eyes::Eyes {};
	}
	registry.SetDirty();
}

void HighDetailSystem::Release(entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* detail = registry.TryGet<const HighDetail>(thing);
	if (detail == nullptr)
	{
		return;
	}
	if (auto* mesh = registry.TryGet<Mesh>(thing); mesh != nullptr && detail->usualModel.has_value())
	{
		mesh->id = *detail->usualModel;
	}
	if (auto* detailMeshes = registry.TryGet<DetailMeshes>(thing);
	    detailMeshes != nullptr && detail->usualDetailModels.has_value())
	{
		detailMeshes->meshes = *detail->usualDetailModels;
	}
	registry.Remove<HighDetail>(thing);
	registry.SetDirty();
}

void HighDetailSystem::RemakeModelsAfterLoad()
{
	// Each thing in high detail goes back to its usual model and takes its detailed model again
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> things;
	registry.Each<const HighDetail>([&things](entt::entity thing, const HighDetail&) { things.push_back(thing); });
	for (const auto thing : things)
	{
		Release(thing);
		Make(thing);
	}
}

bool HighDetailSystem::Order(entt::entity thing, rules::ThingSpecial special, bool on)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* detail = registry.TryGet<HighDetail>(thing);
	if (detail == nullptr)
	{
		return false;
	}
	auto& living = Locator::livingActionSystem::value();
	const auto result = rules::ApplyThingSpecial(special, on, detail->orders, living.VillagerYAngle(thing).value_or(0.0f));
	detail->orders = result.orders;
	if (result.yAngle.has_value())
	{
		living.VillagerSetYAngle(thing, *result.yAngle);
	}
	return true;
}

void HighDetailSystem::Update()
{
	const auto& director = Locator::cinematicDirectorSystem::value();
	if (rules::KeepsHighDetail(director.IsWideScreenOn(), director.GetWideScreenOwner()))
	{
		return;
	}
	if (ReleaseAll() && Locator::objectDrawListSystem::has_value())
	{
		Locator::objectDrawListSystem::value().RequestRebuild(1);
	}
}

bool HighDetailSystem::ReleaseAll()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> drawn;
	registry.Each<const HighDetail>([&drawn](entt::entity entity, const HighDetail&) { drawn.push_back(entity); });
	for (const auto entity : drawn)
	{
		Release(entity);
	}
	return !drawn.empty();
}

void HighDetailSystem::PlaceEyes(uint32_t frameGameMs, const glm::mat4& viewProjection)
{
	if (!Locator::resources::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto view = graphics::view_frustum::FromViewProjection(viewProjection);
	auto& random = Locator::gameRandom::value();
	registry.Each<HighDetail, const Mesh, const Transform>(
	    [&](entt::entity entity, HighDetail& detail, const Mesh& mesh, const Transform& transform) {
		    detail.drawnEyes.reset();
		    if (!detail.eyes.has_value() || !detail.face.has_value() || !meshes.Contains(mesh.id))
		    {
			    return;
		    }
		    // Placed as its body is drawn, by its bones as posed or as its model rests, in the opening hand's grip while held
		    const auto* pose = registry.TryGet<const VillagerPose>(entity);
		    const auto drawnAt = rules::DrawnAt(detail.heldAt, DrawnPosition(transform.position, pose));
		    const auto rotation = DrawnRotation(transform.rotation, pose);
		    const auto standing = PlacedMatrix(drawnAt, rotation, transform.scale);
		    const auto model = physics_draw::ModelMatrix(standing, registry.TryGet<const PhysicsDrawPose>(entity));
		    const auto detailed = meshes.Handle(mesh.id);
		    const auto& frames = detailed->GetBoneFrames();
		    if (!model.has_value() || frames.size() < 2)
		    {
			    return;
		    }
		    // Its eyes blink and look about only while it is in view
		    const auto box = detailed->GetBoundingBox();
		    const auto centre = glm::vec3(*model * glm::vec4(box.Center(), 1.0f));
		    if (!graphics::view_frustum::SeesSphere(view, centre,
		                                            graphics::mesh_detail::ScaledRadius(box.Size(), transform.scale.x)))
		    {
			    return;
		    }
		    const auto& rest = detailed->GetBoneMatrices();
		    const auto& bones = pose != nullptr && pose->bones.size() == rest.size() ? pose->bones : rest;
		    if (frames[0].bone >= bones.size() || frames[1].bone >= bones.size())
		    {
			    return;
		    }
		    const float closed = villager_eyes::Step(*detail.eyes, _eyes, frameGameMs,
		                                             [&random](float a, float b) { return random.CrtRandom(a, b); });
		    detail.drawnEyes = villager_eyes::Place(*detail.face, *model, {bones[frames[0].bone], bones[frames[1].bone]},
		                                            {frames[0].frame, frames[1].frame}, detail.eyes->roll, closed);
	    });
}

void HighDetailSystem::LeaveSwimmingRings(uint32_t frameGameMs)
{
	if (!Locator::resources::has_value() || !Locator::waterRingSystem::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& meshes = Locator::resources::value().GetMeshes();
	// The ring turns by a random angle up to a full turn, from the random numbers of the C library
	constexpr float k_FullTurn = std::bit_cast<float>(0x40C90FDBu);
	const auto angle = [] { return Locator::gameRandom::value().CrtRandom(0.0f, k_FullTurn); };
	registry.Each<const HighDetail, const VillagerPose, const Mesh>(
	    [&](entt::entity, const HighDetail&, const VillagerPose& pose, const Mesh& mesh) {
		    if (!ecs::swimmer_rules::Swims(true, pose.clip) || !meshes.Contains(mesh.id))
		    {
			    return;
		    }
		    // The place of the ring bone in the pose, or in the model's own pose before the villager has one
		    const auto& rest = meshes.Handle(mesh.id)->GetBoneMatrices();
		    const auto& bones = pose.bones.size() == rest.size() ? pose.bones : rest;
		    if (ecs::swimmer_rules::k_RingBone >= bones.size())
		    {
			    return;
		    }
		    const auto ring = ecs::swimmer_rules::Ring(_ringTimer, static_cast<int32_t>(frameGameMs),
		                                               glm::vec3(bones[ecs::swimmer_rules::k_RingBone][3]), angle);
		    if (ring.has_value())
		    {
			    // A ring that does not fit among the many already on the water does not come
			    static_cast<void>(Locator::waterRingSystem::value().Add(*ring));
		    }
	    });
}
