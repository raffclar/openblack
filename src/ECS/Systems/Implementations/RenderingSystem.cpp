/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "RenderingSystem.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <tuple>
#include <unordered_map>

#include <glm/gtx/transform.hpp>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "3D/PhysicsDrawMatrix.h"
#include "Camera/Camera.h"
#include "ECS/BuildingConstruction.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/DetailMeshes.h"
#include "ECS/Components/FallingRoots.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Graveyard.h"
#include "ECS/Components/GroundMark.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/HiddenByState.h"
#include "ECS/Components/HighDetail.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/NeverFades.h"
#include "ECS/Components/ObjectGlow.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ResourcePile.h"
#include "ECS/Components/RisingSoul.h"
#include "ECS/Components/SeeThrough.h"
#include "ECS/Components/SkinOverride.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Stream.h"
#include "ECS/Components/Swayable.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Translucent.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Unlit.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerPose.h"
#include "ECS/Graveyards.h"
#include "ECS/ObjectDetail.h"
#include "ECS/Registry.h"
#include "ECS/Systems/BuildingDamageSystemInterface.h"
#include "ECS/Systems/FieldSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/VegetationInterface.h"
#include "ECS/TownDesireFlags.h"
#include "ECS/TreeRoots.h"
#include "ECS/VillagerHeldObject.h"
#include "Game.h"
#include "Graphics/BonePalette.h"
#include "Graphics/DebugLines.h"
#include "Graphics/DetailLevel.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/GroundBlobs.h"
#include "Graphics/MeshDetail.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/Texture2D.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Physics/DamageMesh.h"
#include "Physics/ObjectRules.h"
#include "Profiler.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
namespace object_detail = openblack::ecs::object_detail;
using openblack::physics::objects::TreePlace;

namespace
{
/// Whether an object is drawn whole with the others: a building drawn as far up as it stands is drawn by itself
bool DrawnWhole(entt::entity entity)
{
	return !openblack::Locator::buildingDamageSystem::has_value() ||
	       openblack::Locator::buildingDamageSystem::value().DrawsWhole(entity);
}

/// The model an object is drawn with: a broken building's broken model in place of its own
entt::id_type DrawnMeshOf(entt::entity entity, const Mesh& mesh)
{
	return openblack::Locator::buildingDamageSystem::has_value()
	           ? openblack::Locator::buildingDamageSystem::value().DrawnMesh(entity, mesh.id)
	           : mesh.id;
}

/// How a thing is drawn this frame by how deep into the view the middle of `box` is, drawn by `model`: the box of its
/// own mesh, or of a villager's standard mesh. Whether it fades out is only asked of one that has come that far.
template <typename Disappears>
openblack::graphics::mesh_detail::Choice ChooseDetail(const openblack::AxisAlignedBoundingBox& box, float importance,
                                                      const glm::mat4& model, float scale, Disappears&& disappears,
                                                      const openblack::Camera& camera, float modelDetail)
{
	namespace mesh_detail = openblack::graphics::mesh_detail;
	const auto centre = glm::vec3(model * glm::vec4(box.Center(), 1.0f));
	const float depth = mesh_detail::ViewDepth(centre, camera.GetOrigin(), camera.GetForward());
	const float reach = mesh_detail::Reach(importance, mesh_detail::ScaledRadius(box.Size(), scale), modelDetail);
	const auto choice = mesh_detail::Choose(depth, reach, true);
	if ((choice.alpha.has_value() || !choice.mesh.has_value()) && !disappears())
	{
		return mesh_detail::Choose(depth, reach, false);
	}
	return choice;
}

/// Whether a model's parts differ by the detail it is drawn in, of those ever drawn
bool PartsDifferByLevel(entt::id_type meshId)
{
	const auto& meshes = entt::locator<openblack::resources::ResourcesInterface>::value().GetMeshes();
	if (!meshes.Contains(meshId))
	{
		return false;
	}
	return std::ranges::any_of(meshes.Handle(meshId)->GetSubMeshes(), [](const auto& subMesh) {
		const auto flags = subMesh->GetFlags();
		return !subMesh->IsPhysics() && flags.isWindow == 0 &&
		       openblack::graphics::mesh_detail::PartsDifferByLevel(static_cast<uint8_t>(flags.lodMask));
	});
}

/// Whether a thing is a village centre
bool IsTownCentre(const openblack::ecs::Registry& registry, entt::entity entity)
{
	const auto* abode = registry.TryGet<const Abode>(entity);
	if (abode == nullptr || !openblack::Locator::infoConstants::has_value())
	{
		return false;
	}
	const auto& infos = openblack::Locator::infoConstants::value().abode;
	const auto type = static_cast<size_t>(abode->type);
	return type < infos.size() && infos.at(type).abodeType == openblack::AbodeType::TownCentre;
}

/// Whether a thing is drawn as a paper cut-out turned to the camera when far off: a field's crop, unless it moves in
/// the physics
bool IsPaper(const openblack::ecs::Registry& registry, entt::entity entity)
{
	return registry.AllOf<Field>(entity) && !registry.AllOf<InPhysics>(entity);
}

/// The model of the roots drawn under the trees out of the land
entt::id_type RootsMesh()
{
	return openblack::resources::HashIdentifier(openblack::MeshId::TreeRoots);
}
} // namespace

RenderingSystem::~RenderingSystem() = default;

void RenderingSystem::PrepareDrawDescs(bool drawBoundingBox)
{
	auto section = Locator::profiler::value().BeginScoped(Profiler::Stage::UpdateEntitiesDescs);
	auto& registry = Locator::entitiesRegistry::value();

	// Count number of instances. The draw lists are made from the components listed in k_ChangesDrawLayout, and made
	// again only when an entity gains or loses one of them: a component they come to depend on belongs in that list.
	uint32_t instanceCount = 0;
	struct MeshInstances
	{
		uint32_t count;
		bool morphWithTerrain;
		bool unlit;
		bool perEntity;
		bool translucent;
		std::optional<float> additiveShare;
		bool instanceAlpha;
		/// Every instance is a posed villager
		bool villagers;
	};
	std::unordered_map<entt::id_type, MeshInstances> meshIds;
	std::unordered_map<entt::id_type, uint32_t> fadingCounts;
	_meshBoxes.clear();

	auto prepMesh = [&registry, &meshIds, &instanceCount](entt::entity entity, entt::id_type drawnMesh, const Mesh& mesh,
	                                                      bool morphWithTerrain) {
		auto count = meshIds.insert(std::make_pair(drawnMesh, MeshInstances {.count = static_cast<uint32_t>(mesh.submeshId),
		                                                                     .morphWithTerrain = morphWithTerrain,
		                                                                     .unlit = false,
		                                                                     .perEntity = false,
		                                                                     .translucent = false,
		                                                                     .additiveShare = std::nullopt,
		                                                                     .instanceAlpha = false,
		                                                                     .villagers = true}));
		count.first->second.count++;
		count.first->second.unlit |= registry.AnyOf<Unlit>(entity);
		// The creatures and the animals are each posed as they are
		count.first->second.perEntity |= registry.AnyOf<CreatureMorph, AnimalPose, VillagerPose, AnimatedStaticPose>(entity);
		count.first->second.villagers &= registry.AllOf<VillagerPose>(entity) && !morphWithTerrain;
		if (const auto* translucent = registry.TryGet<const Translucent>(entity))
		{
			count.first->second.translucent = true;
			count.first->second.additiveShare = translucent->share;
		}
		if (const auto* mark = registry.TryGet<const GroundMark>(entity);
		    (mark != nullptr && mark->alpha.has_value()) || registry.AllOf<SeeThrough>(entity))
		{
			count.first->second.translucent = true;
			count.first->second.instanceAlpha = true;
		}
		instanceCount++;
	};
	auto prep = [&registry, &prepMesh, &fadingCounts, &instanceCount](entt::entity entity, const Mesh& mesh,
	                                                                  bool morphWithTerrain) {
		const auto* detail = registry.TryGet<const DetailMeshes>(entity);
		if (detail == nullptr || morphWithTerrain)
		{
			const auto drawnMesh = DrawnMeshOf(entity, mesh);
			prepMesh(entity, drawnMesh, mesh, morphWithTerrain);
			// A thing drawn in less detail further off has room among those fading out too
			if (detail == nullptr && openblack::ecs::object_detail::Banded(registry, entity))
			{
				++fadingCounts[drawnMesh];
				++instanceCount;
			}
			return;
		}
		// One drawn in less detail further off has room in each of its meshes, and among those fading out
		for (auto it = detail->meshes.begin(); it != detail->meshes.end(); ++it)
		{
			if (std::find(detail->meshes.begin(), it, *it) == it)
			{
				prepMesh(entity, *it, mesh, morphWithTerrain);
			}
		}
		++fadingCounts[detail->meshes.back()];
		++instanceCount;
	};

	registry.Each<const Mesh, const Transform>(
	    [&prep](entt::entity entity, const Mesh& mesh, const Transform& /*unused*/) { prep(entity, mesh, false); },
	    entt::exclude<MorphWithTerrain, Tree, TempleInteriorPart, AtHome, HiddenByState>);
	registry.Each<const Mesh, const Transform, const MorphWithTerrain>(
	    [&prep](entt::entity entity, const Mesh& mesh, const Transform& /*unused*/, const MorphWithTerrain& /*unused*/) {
		    prep(entity, mesh, true);
	    },
	    entt::exclude<Tree>);
	// A tree out of the land has its roots drawn under it: once, or twice while it moves in the physics
	const Mesh roots {.id = RootsMesh(), .submeshId = 0, .bbSubmeshId = 0};
	registry.Each<const Tree, const ShownRoots>(
	    [&prepMesh, &roots](entt::entity tree, const Tree& /*unused*/, const ShownRoots& shown) {
		    for (size_t copy = 0; copy < openblack::physics::objects::ShownRootsScales(shown.place).size(); ++copy)
		    {
			    prepMesh(tree, roots.id, roots, false);
		    }
	    });

	if (drawBoundingBox)
	{
		instanceCount *= 2;
	}

	// Recreate instancing uniform buffer if it is too small
	if (_renderContext.instanceUniforms.size() < instanceCount)
	{
		if (bgfx::isValid(toBgfx(_renderContext.instanceUniformBuffer)))
		{
			bgfx::destroy(toBgfx(_renderContext.instanceUniformBuffer));
		}
		bgfx::VertexLayout layout;
		layout.begin()
		    .add(bgfx::Attrib::TexCoord7, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord6, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord5, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord4, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord3, 4, bgfx::AttribType::Float)
		    .end();
		_renderContext.instanceUniformBuffer = graphics::fromBgfx(bgfx::createDynamicVertexBuffer(instanceCount, layout));
		_renderContext.instanceUniforms.resize(instanceCount);
		_renderContext.instanceLevels.resize(instanceCount);
	}

	// Determine uniform buffer offsets and instance count for draw
	uint32_t offset = 0;
	_renderContext.instancedDrawDescs.clear();
	_instanceSlots.clear();
	for (const auto& [meshId, desc] : meshIds)
	{
		const auto [drawDesc, inserted] =
		    _renderContext.instancedDrawDescs.emplace(std::piecewise_construct, std::forward_as_tuple(meshId),
		                                              std::forward_as_tuple(offset, desc.count, desc.morphWithTerrain));
		drawDesc->second.unlit = desc.unlit;
		drawDesc->second.perEntity = desc.perEntity;
		// The villagers of a mesh are drawn together, their bones read from the bone palette
		drawDesc->second.bonePalette = desc.villagers;
		// The sea doesn't reflect villagers
		drawDesc->second.hiddenFromReflection = desc.villagers;
		// Blended by its materials, over the opaque things
		drawDesc->second.materialBlending = desc.translucent;
		drawDesc->second.translucent = desc.translucent;
		drawDesc->second.additiveShare = desc.additiveShare;
		drawDesc->second.instanceAlpha = desc.instanceAlpha;
		drawDesc->second.partsByLevel = PartsDifferByLevel(meshId);
		_instanceSlots.emplace(meshId, InstanceSlots {.offset = offset,
		                                              .count = desc.count,
		                                              .filled = 0,
		                                              .perEntity = desc.perEntity,
		                                              .height = 0.0f,
		                                              .bonePalette = desc.villagers});
		offset += desc.count;
	}
	// The things fading out in the distance, blended by their own alpha, drawn as the others of their mesh are
	_renderContext.fadingDrawDescs.clear();
	_fadingSlots.clear();
	for (const auto& [meshId, count] : fadingCounts)
	{
		const auto found = meshIds.find(meshId);
		const auto desc = found != meshIds.end() ? std::optional(found->second) : std::nullopt;
		const bool villagers = !desc.has_value() || desc->villagers;
		const bool perEntity = !desc.has_value() || desc->perEntity;
		const auto [drawDesc, inserted] = _renderContext.fadingDrawDescs.emplace(
		    std::piecewise_construct, std::forward_as_tuple(meshId),
		    std::forward_as_tuple(offset, count, desc.has_value() && desc->morphWithTerrain));
		drawDesc->second.perEntity = perEntity;
		drawDesc->second.bonePalette = villagers;
		drawDesc->second.unlit = desc.has_value() && desc->unlit;
		drawDesc->second.instanceAlpha = true;
		drawDesc->second.hiddenFromReflection = villagers;
		drawDesc->second.partsByLevel = PartsDifferByLevel(meshId);
		_fadingSlots.emplace(meshId, InstanceSlots {.offset = offset,
		                                            .count = count,
		                                            .filled = 0,
		                                            .perEntity = perEntity,
		                                            .height = 0.0f,
		                                            .bonePalette = villagers});
		offset += count;
	}

	// Prepare tree instances separately
	PrepareTreeDrawDescs(drawBoundingBox);
}

void RenderingSystem::PrepareTreeDrawDescs(bool drawBoundingBox)
{
	auto& registry = Locator::entitiesRegistry::value();

	// Count number of tree instances
	uint32_t treeInstanceCount = 0;
	std::unordered_map<entt::id_type, uint32_t> treeMeshIds;

	auto prepTree = [&treeMeshIds, &treeInstanceCount](const Mesh& mesh) {
		auto count = treeMeshIds.insert(std::make_pair(mesh.id, 1));
		if (!count.second)
		{
			count.first->second++;
		}
		treeInstanceCount++;
	};

	// Process Tree entities
	registry.Each<const Mesh, const Transform, const Tree>(
	    [&prepTree](const Mesh& mesh, const Transform& /*unused*/, const Tree& /*unused*/) { prepTree(mesh); });

	_renderContext.treeInstanceCount = treeInstanceCount;
	if (drawBoundingBox)
	{
		treeInstanceCount *= 2;
	}

	// Create tree instance buffer if needed
	if (_renderContext.treeInstanceData.size() < treeInstanceCount)
	{
		if (bgfx::isValid(toBgfx(_renderContext.treeInstanceUniformBuffer)))
		{
			bgfx::destroy(toBgfx(_renderContext.treeInstanceUniformBuffer));
		}
		bgfx::VertexLayout layout;
		layout.begin()
		    .add(bgfx::Attrib::TexCoord7, 4, bgfx::AttribType::Float) // i_data0 (matrix row 0)
		    .add(bgfx::Attrib::TexCoord6, 4, bgfx::AttribType::Float) // i_data1 (matrix row 1)
		    .add(bgfx::Attrib::TexCoord5, 4, bgfx::AttribType::Float) // i_data2 (matrix row 2)
		    .add(bgfx::Attrib::TexCoord4, 4, bgfx::AttribType::Float) // i_data3 (matrix row 3)
		    .add(bgfx::Attrib::TexCoord3, 4, bgfx::AttribType::Float) // i_data4 (how it burns)
		    .end();
		_renderContext.treeInstanceUniformBuffer =
		    graphics::fromBgfx(bgfx::createDynamicVertexBuffer(treeInstanceCount, layout));
		_renderContext.treeInstanceData.resize(treeInstanceCount);
	}

	// Determine tree uniform buffer offsets and instance count for draw
	uint32_t treeOffset = 0;
	_renderContext.treeInstancedDrawDescs.clear();
	_treeSlots.clear();
	auto& meshes = entt::locator<resources::ResourcesInterface>::value().GetMeshes();
	for (const auto& [meshId, count] : treeMeshIds)
	{
		_renderContext.treeInstancedDrawDescs.emplace(std::piecewise_construct, std::forward_as_tuple(meshId),
		                                              std::forward_as_tuple(treeOffset, count, false));
		const auto height = meshes.Contains(meshId) ? meshes.Handle(meshId)->GetBoundingBox().Size().y : 0.0f;
		_treeSlots.emplace(
		    meshId, InstanceSlots {.offset = treeOffset, .count = count, .filled = 0, .perEntity = false, .height = height});
		treeOffset += count;
	}
}

void RenderingSystem::PrepareDrawUploadUniforms(bool drawBoundingBox)
{
	// Fresh draw lists have room for every instance
	UploadInstances(drawBoundingBox);
	UploadTreeInstances(drawBoundingBox);
}

bool RenderingSystem::UploadUniformsKeepingDescs(bool drawBoundingBox)
{
	return UploadInstances(drawBoundingBox) && UploadTreeInstances(drawBoundingBox);
}

bool RenderingSystem::UploadInstances(bool drawBoundingBox)
{
	auto section = Locator::profiler::value().BeginScoped(Profiler::Stage::UpdateEntitiesUniforms);
	auto& registry = Locator::entitiesRegistry::value();

	for (auto& [meshId, slots] : _instanceSlots)
	{
		slots.filled = 0;
		slots.gone = 0;
	}
	for (auto& [meshId, slots] : _fadingSlots)
	{
		slots.filled = 0;
	}
	_renderContext.farVillagers.clear();
	_fadingThings.clear();
	_goneThings.clear();
	const auto* camera = Locator::camera::has_value() ? &Locator::camera::value() : nullptr;
	const float modelDetail = graphics::detail_level::ModelDetail(Locator::config::value().detailLevel);

	const auto& vegetation = Locator::vegetation::value();

	// Set transforms for instanced draw at offsets
	_renderContext.entityDraws.clear();
	_renderContext.drawnObjects.clear();
	_renderContext.highDetailDraws.clear();
	_renderContext.inHandDraws.clear();
	_renderContext.bonePalette.clear();
	_heldThings.clear();
	const auto& meshes = entt::locator<resources::ResourcesInterface>::value().GetMeshes();
	bool fits = true;
	registry.Each<const Mesh, const Transform>(
	    [this, &registry, &vegetation, &meshes, &fits, drawBoundingBox, camera,
	     modelDetail](entt::entity entity, const Mesh& mesh, const Transform& transform) {
		    // A villager is drawn gliding between its turns and turning after the way it faces; one held by the opening's
		    // hand is drawn in its grip
		    const auto* villagerPose = registry.TryGet<const VillagerPose>(entity);
		    const auto standsAt = DrawnPosition(transform.position, villagerPose);
		    const auto rotation = DrawnRotation(transform.rotation, villagerPose);
		    const auto* highDetail = registry.TryGet<const HighDetail>(entity);
		    const auto drawnAt =
		        highDetail != nullptr ? openblack::ecs::high_detail_rules::DrawnAt(highDetail->heldAt, standsAt) : standsAt;
		    auto modelMatrix = glm::mat4(rotation);
		    modelMatrix = glm::translate(modelMatrix, drawnAt * rotation);
		    modelMatrix = glm::scale(modelMatrix, transform.scale);
		    // A body moving in the physics is drawn between its last two turns, and not at all once sunk under the sea
		    const auto* drawn = registry.TryGet<const PhysicsDrawPose>(entity);
		    const auto placed = physics_draw::ModelMatrix(modelMatrix, drawn);
		    modelMatrix = placed.value_or(glm::mat4(0.0f));

		    // A thing on the land is drawn in less detail the further off it is, fades out, then isn't drawn; a villager
		    // then shows only as a smudge on a blob while it stands above the sea. One flying in the physics or one that
		    // is kept in view never fades, and one a script draws in high detail is drawn in full however far off.
		    auto drawnMesh = DrawnMeshOf(entity, mesh);
		    auto* slotMap = &_instanceSlots;
		    std::optional<uint8_t> fade;
		    auto level = graphics::mesh_detail::Mesh::High;
		    bool gone = false;
		    const auto* detail = registry.TryGet<const DetailMeshes>(entity);
		    if (detail != nullptr && registry.AllOf<HighDetail>(entity) && !registry.AllOf<MorphWithTerrain>(entity))
		    {
			    drawnMesh = detail->meshes.at(static_cast<size_t>(graphics::mesh_detail::Mesh::High));
		    }
		    else if (camera != nullptr && (detail == nullptr || !registry.AllOf<MorphWithTerrain>(entity)) &&
		             object_detail::Banded(registry, entity) &&
		             (detail != nullptr || object_detail::BandedNow(registry, entity)))
		    {
			    // A villager's bands are by its standard mesh's box, any other thing's by its own
			    const auto bounding = detail != nullptr
			                              ? detail->meshes.at(static_cast<size_t>(graphics::mesh_detail::Mesh::Standard))
			                              : drawnMesh;
			    if (const auto& box = MeshBox(bounding); box.has_value())
			    {
				    const auto choice = ChooseDetail(
				        *box, detail != nullptr ? detail->importance : 0.0f, modelMatrix, transform.scale.x,
				        [&registry, entity] {
					        return object_detail::Disappears(registry, entity, IsTownCentre(registry, entity));
				        },
				        *camera, modelDetail);
				    if (!choice.mesh.has_value())
				    {
					    if (detail == nullptr)
					    {
						    gone = true;
						    _goneThings.insert(entity);
					    }
					    else if (!registry.AllOf<Villager>(entity))
					    {
						    // Any other thing of several meshes, an animal, leaves nothing behind
						    return;
					    }
					    else
					    {
						    const auto position = glm::vec3(modelMatrix[3]);
						    if (placed.has_value() && position.y > graphics::ground_blobs::k_LowestHeight)
						    {
							    _renderContext.farVillagers.push_back(
							        {.entity = entity, .position = position, .scale = transform.scale.x});
							    _farSmudgeScale = _farSmudgeScale.value_or(transform.scale.x);
						    }
						    return;
					    }
				    }
				    else
				    {
					    level = *choice.mesh;
					    if (detail != nullptr)
					    {
						    drawnMesh = detail->meshes.at(static_cast<size_t>(level));
					    }
					    if (choice.alpha.has_value())
					    {
						    slotMap = &_fadingSlots;
						    fade = choice.alpha;
						    _fadingThings.emplace(entity, *choice.alpha);
					    }
				    }
			    }
		    }

		    // A thing too far off to be drawn keeps a place at the back of its mesh's instances, only for what it leaves on
		    // the land
		    if (gone)
		    {
			    const auto slots = _instanceSlots.find(drawnMesh);
			    if (!fits || slots == _instanceSlots.end() || slots->second.filled + slots->second.gone >= slots->second.count)
			    {
				    fits = false;
				    return;
			    }
			    const uint32_t idx = slots->second.offset + slots->second.count - 1 - slots->second.gone;
			    _renderContext.instanceUniforms[idx] = {.model = modelMatrix, .look = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)};
			    if (drawBoundingBox)
			    {
				    _renderContext.instanceUniforms[idx + (_renderContext.instanceUniforms.size() / 2)] = {.model =
				                                                                                               glm::mat4(0.0f)};
			    }
			    ++slots->second.gone;
			    return;
		    }

		    // A mesh the draw lists don't have room for, which has changed since they were made
		    const auto slots = slotMap->find(drawnMesh);
		    if (!fits || slots == slotMap->end() || slots->second.filled + slots->second.gone >= slots->second.count)
		    {
			    fits = false;
			    return;
		    }
		    // A building drawn only as far up as it stands keeps its place in the lists, not drawn there, so the lists
		    // needn't be made again as it comes to stand whole
		    if (!DrawnWhole(entity))
		    {
			    const uint32_t idx = slots->second.offset + slots->second.filled;
			    _renderContext.instanceUniforms[idx] = {.model = glm::mat4(0.0f), .look = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)};
			    if (drawBoundingBox)
			    {
				    _renderContext.instanceUniforms[idx + (_renderContext.instanceUniforms.size() / 2)] = {.model =
				                                                                                               glm::mat4(0.0f)};
			    }
			    ++slots->second.filled;
			    return;
		    }
		    // A mesh drawn with another texture, slid across it: the temple's leashes
		    if (const auto* skin = registry.TryGet<const SkinOverride>(entity))
		    {
			    if (const auto desc = _renderContext.instancedDrawDescs.find(slots->first);
			        desc != _renderContext.instancedDrawDescs.end())
			    {
				    desc->second.uvOffset = skin->uvOffset;
				    if (desc->second.subMeshTextures.empty())
				    {
					    const auto skinned =
					        entt::locator<resources::ResourcesInterface>::value().GetMeshes().Handle(slots->first);
					    for (uint32_t i = 0; i < static_cast<uint32_t>(skinned->GetSubMeshes().size()); ++i)
					    {
						    desc->second.subMeshTextures.emplace_back(i, skin->texture);
					    }
				    }
			    }
		    }

		    // A home with someone in lights its windows at night
		    const auto* abode = registry.TryGet<const Abode>(entity);
		    glm::vec4 look {abode != nullptr && abode->presentAtHome > 0 ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f};
		    // A field's crop shows once it has grown a little, in its colour for how ripe it is, sunk into the ground by
		    // how empty it is, and sways once ripe
		    if (const auto* field = registry.TryGet<const Field>(entity); field != nullptr)
		    {
			    const auto crop = Locator::fieldSystem::value().GetLook(*field);
			    if (!crop.has_value())
			    {
				    look.z = 1.0f;
			    }
			    else
			    {
				    look.y = static_cast<float>(crop->tint);
				    look.w = field_crop::k_SnowCap;
				    const auto cropMesh = entt::locator<resources::ResourcesInterface>::value().GetMeshes().Handle(mesh.id);
				    const float cropHeight = cropMesh->GetBoundingBox().Size().y * transform.scale.y;
				    modelMatrix[3].y += field_crop::Sink(field->height.position) * cropHeight;
				    // Far off, drawn in its least detail, a crop is a paper cut-out standing turned to the camera, which
				    // doesn't sway
				    if (level == graphics::mesh_detail::Mesh::Low && IsPaper(registry, entity) && camera != nullptr)
				    {
					    modelMatrix = graphics::mesh_detail::PaperMatrix(glm::vec3(modelMatrix[3]), transform.scale.x,
					                                                     camera->GetOrigin(), camera->GetFocus());
				    }
				    else if (const auto* swayable = registry.TryGet<const Swayable>(entity); swayable != nullptr && crop->sways)
				    {
					    modelMatrix = vegetation.GetFieldMatrix(modelMatrix, transform.scale.y, swayable->swaySlot);
				    }
			    }
		    }

		    // A pile of food or wood is drawn as far under the ground as it has sunk, not at all once wholly under, and the
		    // grain of a pile of food flows down it as it rises. A handful in the hand is drawn whole where it is held.
		    if (const auto* pile = registry.TryGet<const ResourcePile>(entity); pile != nullptr && pile->height > 0.0f)
		    {
			    const auto sink = magic::piles::DrawnSink(pile->rise, pile->height, registry.AllOf<InHand>(entity));
			    if (!sink.has_value())
			    {
				    look.z = 1.0f;
			    }
			    else
			    {
				    modelMatrix[3].y += *sink;
				    const auto* pot = registry.TryGet<const Pot>(entity);
				    if (pot != nullptr && magic::piles::GrainFlows(pot->type))
				    {
					    look.x = -magic::piles::GrainFlow(*sink, pile->height);
				    }
			    }
		    }

		    // A frozen creature takes an icy look, tinted dark blue and sheened with ice as it freezes (one fizzing out of
		    // sight is drawn through the static by the renderer)
		    if (const auto* spells = registry.TryGet<const CreatureSpells>(entity))
		    {
			    if (spells->freeze > 0.0f)
			    {
				    look.y = static_cast<float>(creature_spells::FrozenTint(spells->freeze));
				    look.w = -spells->freeze;
			    }
		    }

		    // A glow added over it, as the heal lights the people it heals, as a negative x: a house's positive x is its
		    // windows' light
		    if (const auto* glow = registry.TryGet<const ObjectGlow>(entity); glow != nullptr && abode == nullptr)
		    {
			    look.x = -static_cast<float>(glow->Packed());
		    }
		    // A blast's rubble fades by its alpha in its last second
		    if (const auto* mark = registry.TryGet<const GroundMark>(entity); mark != nullptr && mark->alpha.has_value())
		    {
			    look.z = -(1.0f - static_cast<float>(*mark->alpha) / 255.0f);
		    }
		    // A see-through copy is blended by its own alpha
		    if (const auto* seeThrough = registry.TryGet<const SeeThrough>(entity))
		    {
			    look.z = -(1.0f - static_cast<float>(seeThrough->alpha) / 255.0f);
		    }
		    // Something charred by fire is drawn grey
		    if (look.y == 0.0f && Locator::fireSystem::has_value())
		    {
			    if (const auto charred = Locator::fireSystem::value().GetCharredColour(entity))
			    {
				    look.y = static_cast<float>(*charred);
			    }
		    }
		    // Something hot glows red-orange, added as the heal's glow is (a pile or pot doesn't). A house's glow goes
		    // above its windows' light: 1 plus the glow while someone is home
		    if (Locator::fireSystem::has_value() && !registry.AllOf<Pot>(entity))
		    {
			    if (const auto heat = Locator::fireSystem::value().GetGlowColour(entity); heat.has_value() && *heat != 0)
			    {
				    if (abode != nullptr)
				    {
					    look.x = look.x > 0.5f ? 1.0f + static_cast<float>(*heat) : -static_cast<float>(*heat);
				    }
				    else if (look.x <= 0.0f && look.x >= -0.5f)
				    {
					    look.x = -static_cast<float>(*heat);
				    }
				    else if (look.x < -0.5f)
				    {
					    const auto glow = static_cast<uint32_t>(-look.x);
					    uint32_t sum = 0;
					    for (const uint32_t shift : {16u, 8u, 0u})
					    {
						    sum |= std::min(((glow >> shift) & 0xFFu) + ((*heat >> shift) & 0xFFu), 0xFFu) << shift;
					    }
					    look.x = -static_cast<float>(sum);
				    }
			    }
		    }

		    // A flag over a town's store carries little snow, and isn't there at all while it doesn't show
		    if (const auto* flag = registry.TryGet<const TownDesireFlag>(entity); flag != nullptr)
		    {
			    look.w = openblack::ecs::town_desire_flags::k_SnowCap;
			    if (!flag->shown)
			    {
				    look.z = 1.0f;
				    modelMatrix = glm::mat4(0.0f);
			    }
		    }

		    // A body sunk wholly under the sea isn't drawn in any pass, its shadow included
		    if (!placed.has_value())
		    {
			    look.z = 1.0f;
			    modelMatrix = glm::mat4(0.0f);
		    }

		    // A building going up is drawn only as far up as it stands, with the broken and the unfinished ones, though
		    // what of it stands can still be pointed at
		    const auto* progress = registry.TryGet<const BuildProgress>(entity);
		    const bool goingUp = progress != nullptr && DrawnMeshOf(entity, mesh) == mesh.id;
		    if (goingUp)
		    {
			    look.z = 1.0f;
		    }

		    // Fading out in the distance
		    if (fade.has_value() && look.z != 1.0f)
		    {
			    look.z = -(1.0f - (static_cast<float>(*fade) / 255.0f));
		    }

		    const uint32_t idx = slots->second.offset + slots->second.filled;
		    // An animal finds its own bones by its place among its model's instances, kept in its first column's w, which
		    // the model's affine matrix leaves at 0
		    auto instanceModel = modelMatrix;
		    if (registry.AllOf<AnimalPose>(entity))
		    {
			    instanceModel[0].w = static_cast<float>(slots->second.filled);
		    }
		    _renderContext.instanceUniforms[idx] = {.model = instanceModel, .look = look};
		    _renderContext.instanceLevels[idx] = graphics::mesh_detail::LevelBit(level);
		    // A villager's bones go into the palette, as its clip poses it or as its model rests, and its instance says
		    // where they start
		    if (slots->second.bonePalette)
		    {
			    const auto* pose = registry.TryGet<const VillagerPose>(entity);
			    const auto& rest = meshes.Handle(slots->first)->GetBoneMatrices();
			    const auto bones = pose != nullptr && pose->bones.size() == rest.size()
			                           ? std::span<const glm::mat4>(pose->bones)
			                           : std::span<const glm::mat4>(rest);
			    graphics::bone_palette::SetFirstBone(_renderContext.instanceUniforms[idx].model,
			                                         graphics::bone_palette::Append(_renderContext.bonePalette, bones));
		    }
		    // What a villager holds is drawn on the bone at the end of its arm, as its clip poses it, with the villager's
		    // look; not while a hand holds it
		    if (const auto* person = registry.TryGet<const Villager>(entity); person != nullptr && villagerPose != nullptr &&
		                                                                      look.z != 1.0f && placed.has_value() &&
		                                                                      !registry.AllOf<InHand>(entity))
		    {
			    const auto heldMesh = openblack::ecs::villager_held::MeshOf(person->carried);
			    const auto& rest = meshes.Handle(slots->first)->GetBoneMatrices();
			    const auto& bones =
			        villagerPose->bones.size() > openblack::ecs::villager_held::k_GripBone ? villagerPose->bones : rest;
			    if (heldMesh.has_value() && bones.size() > openblack::ecs::villager_held::k_GripBone)
			    {
				    const auto heldId = resources::HashIdentifier(*heldMesh);
				    if (meshes.Contains(heldId))
				    {
					    const auto held = openblack::ecs::villager_held::HeldMatrix(
					        modelMatrix, bones[openblack::ecs::villager_held::k_GripBone]);
					    // It fades out in the distance as any small thing does, by its own size
					    std::optional<uint8_t> ownFade;
					    if (camera != nullptr)
					    {
						    const auto box = meshes.Handle(heldId)->GetBoundingBox();
						    const auto centre = glm::vec3(held * glm::vec4(box.Center(), 1.0f));
						    const float depth =
						        graphics::mesh_detail::ViewDepth(centre, camera->GetOrigin(), camera->GetForward());
						    const float reach = graphics::mesh_detail::Reach(
						        0.0f, graphics::mesh_detail::ScaledRadius(box.Size(), glm::length(glm::vec3(held[0]))),
						        modelDetail);
						    const auto choice = graphics::mesh_detail::Choose(depth, reach, true);
						    ownFade = choice.alpha;
						    if (!choice.mesh.has_value())
						    {
							    ownFade = 0;
						    }
					    }
					    const auto alpha = openblack::ecs::villager_held::HeldAlpha(ownFade, fade);
					    if (alpha != std::optional<uint8_t>(0))
					    {
						    auto heldLook = look;
						    heldLook.z = alpha.has_value() ? -(1.0f - (static_cast<float>(*alpha) / 255.0f)) : 0.0f;
						    _heldThings.push_back({.meshId = heldId,
						                           .fading = alpha.has_value(),
						                           .reflected = registry.AllOf<InPhysics>(entity),
						                           .instance = {.model = held, .look = heldLook}});
					    }
				    }
			    }
		    }
		    if (look.z != 1.0f || (goingUp && progress->built > 0.0f))
		    {
			    _renderContext.drawnObjects.push_back({.entity = entity, .model = modelMatrix});
		    }
		    if (openblack::ecs::high_detail_rules::ShadowOf(highDetail != nullptr) ==
		            openblack::ecs::high_detail_rules::Shadow::Projected &&
		        look.z != 1.0f)
		    {
			    // Its shadow is cast by its model as the villager itself turns, before the body's own easing
			    auto castModel = modelMatrix;
			    if (villagerPose != nullptr && villagerPose->drawnHeading.has_value() &&
			        villagerPose->easedHeading.has_value() && drawn == nullptr)
			    {
				    const auto turned = glm::mat4(openblack::ecs::villager_draw::RotationOf(*villagerPose->easedHeading));
				    castModel = glm::scale(glm::translate(turned, drawnAt * glm::mat3(turned)), transform.scale);
			    }
			    _renderContext.highDetailDraws.push_back(
			        {.entity = entity, .mesh = drawnMesh, .model = modelMatrix, .castModel = castModel});
		    }
		    if (registry.AllOf<InHand>(entity) && look.z != 1.0f)
		    {
			    // What a hand holds joins the hand's shadow
			    _renderContext.inHandDraws.push_back(
			        {.entity = entity, .mesh = drawnMesh, .model = modelMatrix, .castModel = modelMatrix});
		    }
		    if (slots->second.perEntity && placed.has_value())
		    {
			    _renderContext.entityDraws.push_back({.entity = entity, .instance = idx});
		    }
		    if (drawBoundingBox)
		    {
			    auto l3dMesh =
			        entt::locator<resources::ResourcesInterface>::value().GetMeshes().Handle(DrawnMeshOf(entity, mesh));
			    auto box = l3dMesh->GetBoundingBox();
			    auto boxMatrix = modelMatrix * glm::translate(box.Center()) * glm::scale(box.Size());
			    _renderContext.instanceUniforms[idx + (_renderContext.instanceUniforms.size() / 2)] = {.model = boxMatrix};
		    }
		    ++slots->second.filled;
	    },
	    entt::exclude<TempleInteriorPart, Tree, AtHome, HiddenByState>);

	// The roots under a tree out of the land, at its base, turned and stretched as the tree is placed
	const auto rootsSlots = _instanceSlots.find(RootsMesh());
	registry.Each<const Mesh, const Transform, const Tree, const ShownRoots>(
	    [this, &registry, &meshes, &fits, &rootsSlots, drawBoundingBox](
	        entt::entity tree, const Mesh& mesh, const Transform& transform, const Tree& /*unused*/, const ShownRoots& shown) {
		    const auto scales = openblack::physics::objects::ShownRootsScales(shown.place);
		    if (!fits || rootsSlots == _instanceSlots.end() ||
		        rootsSlots->second.filled + scales.size() > rootsSlots->second.count)
		    {
			    fits = false;
			    return;
		    }
		    auto standing = glm::mat4(transform.rotation);
		    standing = glm::translate(standing, transform.position * transform.rotation);
		    standing = glm::scale(standing, transform.scale);
		    const auto placed = physics_draw::ModelMatrix(standing, registry.TryGet<const PhysicsDrawPose>(tree));
		    const float halfWidth = meshes.Contains(mesh.id) ? meshes.Handle(mesh.id)->GetBoundingBox().Size().x * 0.5f : 0.0f;
		    for (const float scale : scales)
		    {
			    const uint32_t idx = rootsSlots->second.offset + rootsSlots->second.filled;
			    _renderContext.instanceUniforms[idx] =
			        placed.has_value()
			            ? RenderContext::ObjectInstance {.model = openblack::physics::objects::ShownRootsModel(*placed, scale,
			                                                                                                   halfWidth),
			                                             .look = glm::vec4(0.0f)}
			            : RenderContext::ObjectInstance {.model = glm::mat4(0.0f), .look = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)};
			    if (drawBoundingBox)
			    {
				    _renderContext.instanceUniforms[idx + (_renderContext.instanceUniforms.size() / 2)] = {.model =
				                                                                                               glm::mat4(0.0f)};
			    }
			    ++rootsSlots->second.filled;
		    }
	    });

	if (fits && !_renderContext.instanceUniforms.empty())
	{
		const auto size = static_cast<uint32_t>(_renderContext.instanceUniforms.size() * sizeof(RenderContext::ObjectInstance));
		// Copied, never referenced: bgfx's render thread reads the update while the next frame refills (and may grow,
		// freeing the old storage) these instances
		bgfx::update(toBgfx(_renderContext.instanceUniformBuffer), 0, bgfx::copy(_renderContext.instanceUniforms.data(), size));
	}
	for (auto& [meshId, desc] : _renderContext.instancedDrawDescs)
	{
		const auto slots = _instanceSlots.find(meshId);
		desc.filled = slots != _instanceSlots.end() ? slots->second.filled : 0;
		desc.gone = slots != _instanceSlots.end() ? slots->second.gone : 0;
	}
	for (auto& [meshId, desc] : _renderContext.fadingDrawDescs)
	{
		const auto slots = _fadingSlots.find(meshId);
		desc.filled = slots != _fadingSlots.end() ? slots->second.filled : 0;
	}
	std::ranges::sort(_renderContext.farVillagers, {}, &RenderContext::FarVillager::entity);
	_renderContext.farSmudgeScale = _farSmudgeScale.value_or(1.0f);
	UploadBonePalette();
	UploadPartialBuilds();
	UploadHeldObjects();
	UploadSouls();
	return fits;
}

void RenderingSystem::UploadSouls()
{
	auto& registry = Locator::entitiesRegistry::value();
	_renderContext.soulDraws.clear();
	_renderContext.soulInstances.clear();
	registry.Each<const RisingSoul, const Transform>([this](const RisingSoul& soul, const Transform& transform) {
		if (soul.bones.empty())
		{
			return;
		}
		_renderContext.soulDraws.push_back({.meshId = soul.mesh,
		                                    .instance = static_cast<uint32_t>(_renderContext.soulInstances.size()),
		                                    .bones = soul.bones,
		                                    .alpha = static_cast<float>(soul.alpha) / 255.0f});
		const auto model = glm::translate(transform.position) * glm::mat4(transform.rotation) * glm::scale(transform.scale);
		_renderContext.soulInstances.push_back({.model = model, .look = glm::vec4(0.0f)});
	});
	const auto count = static_cast<uint32_t>(_renderContext.soulInstances.size());
	if (count == 0)
	{
		return;
	}
	if (_renderContext.soulCapacity < count)
	{
		if (bgfx::isValid(toBgfx(_renderContext.soulInstanceBuffer)))
		{
			bgfx::destroy(toBgfx(_renderContext.soulInstanceBuffer));
		}
		bgfx::VertexLayout layout;
		layout.begin()
		    .add(bgfx::Attrib::TexCoord7, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord6, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord5, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord4, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord3, 4, bgfx::AttribType::Float)
		    .end();
		// Room for a few more dying at once without making it again
		const uint32_t capacity = count + 8;
		_renderContext.soulInstanceBuffer = graphics::fromBgfx(bgfx::createDynamicVertexBuffer(capacity, layout));
		_renderContext.soulCapacity = capacity;
	}
	bgfx::update(
	    toBgfx(_renderContext.soulInstanceBuffer), 0,
	    bgfx::copy(_renderContext.soulInstances.data(), static_cast<uint32_t>(count * sizeof(RenderContext::ObjectInstance))));
}

void RenderingSystem::UploadHeldObjects()
{
	_renderContext.heldDraws.clear();
	_renderContext.heldInstances.clear();
	// Those of a model together, so that each model is drawn at once: the opaque ones, then those fading out; and of
	// each the ones the sea reflects together
	std::ranges::sort(_heldThings, {},
	                  [](const HeldThing& thing) { return std::tuple(thing.fading, thing.reflected, thing.meshId); });
	for (const auto& thing : _heldThings)
	{
		auto& draws = _renderContext.heldDraws;
		if (draws.empty() || draws.back().meshId != thing.meshId || draws.back().fading != thing.fading ||
		    draws.back().reflected != thing.reflected)
		{
			draws.push_back({.meshId = thing.meshId,
			                 .first = static_cast<uint32_t>(_renderContext.heldInstances.size()),
			                 .count = 0,
			                 .fading = thing.fading,
			                 .reflected = thing.reflected});
		}
		++draws.back().count;
		_renderContext.heldInstances.push_back(thing.instance);
	}
	const auto count = static_cast<uint32_t>(_renderContext.heldInstances.size());
	if (count == 0)
	{
		return;
	}
	if (_renderContext.heldCapacity < count)
	{
		if (bgfx::isValid(toBgfx(_renderContext.heldInstanceBuffer)))
		{
			bgfx::destroy(toBgfx(_renderContext.heldInstanceBuffer));
		}
		bgfx::VertexLayout layout;
		layout.begin()
		    .add(bgfx::Attrib::TexCoord7, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord6, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord5, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord4, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord3, 4, bgfx::AttribType::Float)
		    .end();
		// Room to grow, so that a few more villagers picking things up don't make it again
		const uint32_t capacity = count + (count / 2) + 16;
		_renderContext.heldInstanceBuffer = graphics::fromBgfx(bgfx::createDynamicVertexBuffer(capacity, layout));
		_renderContext.heldCapacity = capacity;
	}
	bgfx::update(
	    toBgfx(_renderContext.heldInstanceBuffer), 0,
	    bgfx::copy(_renderContext.heldInstances.data(), static_cast<uint32_t>(count * sizeof(RenderContext::ObjectInstance))));
}

void RenderingSystem::UploadPartialBuilds()
{
	auto& registry = Locator::entitiesRegistry::value();
	_renderContext.partialBuilds.clear();
	_renderContext.partialBuildInstances.clear();
	if (!Locator::buildingDamageSystem::has_value())
	{
		return;
	}
	const auto& buildings = Locator::buildingDamageSystem::value();
	const auto& meshes = Locator::resources::value().GetMeshes();
	registry.Each<const Mesh, const Transform>([&](entt::entity entity, const Mesh& mesh, const Transform& transform) {
		const auto share = buildings.PartialShare(entity);
		if (!share.has_value() || !meshes.Contains(mesh.id) || _goneThings.contains(entity))
		{
			return;
		}
		const auto fading = _fadingThings.find(entity);
		const auto look = fading != _fadingThings.end()
		                      ? glm::vec4(0.0f, 0.0f, -(1.0f - (static_cast<float>(fading->second) / 255.0f)), 0.0f)
		                      : glm::vec4(0.0f);
		const auto model = meshes.Handle(mesh.id);
		const float halfHeight = 0.5f * model->GetBoundingBox().Size().y;
		const auto build = physics::damage::PartialBuildOf(*share, transform.position.y, halfHeight, transform.scale.y);
		// The scaffold is the submeshes of the highest status there is
		std::optional<uint32_t> scaffold;
		for (const auto& subMesh : model->GetSubMeshes())
		{
			const auto status = subMesh->GetFlags().status;
			if (status >= 1 && (!scaffold.has_value() || status > *scaffold))
			{
				scaffold = status;
			}
		}
		auto matrix = glm::translate(glm::mat4(1.0f), transform.position) * glm::mat4(transform.rotation) *
		              glm::scale(glm::mat4(1.0f), transform.scale);
		RenderContext::PartialBuildDraw draw {
		    .meshId = mesh.id,
		    .morphWithTerrain = registry.AllOf<MorphWithTerrain>(entity),
		    .instance = static_cast<uint32_t>(_renderContext.partialBuildInstances.size()),
		    .modelCut = build.modelCut,
		    .capHeight = build.modelCut.has_value() && transform.scale.y != 0.0f
		                     ? std::optional((*build.modelCut - transform.position.y) / transform.scale.y)
		                     : std::nullopt,
		    .scaffoldStatus = build.scaffoldShown ? scaffold : std::nullopt,
		    .scaffoldCut = build.scaffoldCut,
		    // A temple's inner walls stand in further than other buildings', whatever their material
		    .innerWallInset =
		        registry.AllOf<Temple>(entity) ? std::optional(building_construction::k_TempleInnerWallInset) : std::nullopt,
		    .fading = fading != _fadingThings.end(),
		};
		_renderContext.partialBuildInstances.push_back({.model = matrix, .look = look});
		// The scaffold sinks along its up axis while the building rises out of the land
		const glm::vec3 up = transform.rotation[1];
		draw.scaffoldInstance = static_cast<uint32_t>(_renderContext.partialBuildInstances.size());
		_renderContext.partialBuildInstances.push_back(
		    {.model = glm::translate(glm::mat4(1.0f), -up * build.scaffoldSink) * matrix, .look = look});
		_renderContext.partialBuilds.push_back(draw);
	});
	// A standing building shows its model's numbered parts up to a number over its model drawn with the others; one going
	// up or broken shows none
	const auto showParts = [&](entt::entity entity, const Mesh& mesh, const Transform& transform, uint32_t parts) {
		if (parts == 0 || buildings.PartialShare(entity).has_value() || !DrawnWhole(entity) || !meshes.Contains(mesh.id) ||
		    _goneThings.contains(entity))
		{
			return;
		}
		const auto fading = _fadingThings.find(entity);
		_renderContext.partialBuilds.push_back({
		    .meshId = mesh.id,
		    .morphWithTerrain = registry.AllOf<MorphWithTerrain>(entity),
		    .instance = static_cast<uint32_t>(_renderContext.partialBuildInstances.size()),
		    .gravesShown = parts,
		    .fading = fading != _fadingThings.end(),
		});
		_renderContext.partialBuildInstances.push_back(
		    {.model = glm::translate(glm::mat4(1.0f), transform.position) * glm::mat4(transform.rotation) *
		              glm::scale(glm::mat4(1.0f), transform.scale),
		     .look = fading != _fadingThings.end()
		                 ? glm::vec4(0.0f, 0.0f, -(1.0f - (static_cast<float>(fading->second) / 255.0f)), 0.0f)
		                 : glm::vec4(0.0f)});
	};
	// A graveyard shows the graves of its burials
	registry.Each<const Graveyard, const Mesh, const Transform>(
	    [&](entt::entity entity, const Graveyard&, const Mesh& mesh, const Transform& transform) {
		    showParts(entity, mesh, transform, graveyards::GravesShown(entity));
	    });
	// A building standing in as its town's graveyard shows the parts its burials' graves would, as in the original game:
	// its scaffold
	registry.Each<const StandInGraves, const Mesh, const Transform>(
	    [&](entt::entity entity, const StandInGraves& graves, const Mesh& mesh, const Transform& transform) {
		    showParts(entity, mesh, transform, graves.partsShown);
	    });
	const auto count = static_cast<uint32_t>(_renderContext.partialBuildInstances.size());
	if (count == 0)
	{
		return;
	}
	if (_renderContext.partialBuildCapacity < count)
	{
		if (bgfx::isValid(toBgfx(_renderContext.partialBuildInstanceBuffer)))
		{
			bgfx::destroy(toBgfx(_renderContext.partialBuildInstanceBuffer));
		}
		bgfx::VertexLayout layout;
		layout.begin()
		    .add(bgfx::Attrib::TexCoord7, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord6, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord5, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord4, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord3, 4, bgfx::AttribType::Float)
		    .end();
		_renderContext.partialBuildInstanceBuffer = graphics::fromBgfx(bgfx::createDynamicVertexBuffer(count, layout));
		_renderContext.partialBuildCapacity = count;
	}
	bgfx::update(toBgfx(_renderContext.partialBuildInstanceBuffer), 0,
	             bgfx::copy(_renderContext.partialBuildInstances.data(),
	                        static_cast<uint32_t>(count * sizeof(RenderContext::ObjectInstance))));
}

bool RenderingSystem::UploadTreeInstances(bool drawBoundingBox)
{
	auto section = Locator::profiler::value().BeginScoped(Profiler::Stage::UpdateEntitiesTrees);
	auto& registry = Locator::entitiesRegistry::value();
	const auto& vegetation = Locator::vegetation::value();
	const auto* camera = Locator::camera::has_value() ? &Locator::camera::value() : nullptr;
	const float modelDetail = graphics::detail_level::ModelDetail(Locator::config::value().detailLevel);
	const auto& meshes = entt::locator<resources::ResourcesInterface>::value().GetMeshes();
	using Group = PlacedTree::Group;
	// Every far tree turns the same way, by the camera's heading
	const auto paperTurn =
	    camera != nullptr ? graphics::mesh_detail::PaperTurn(camera->GetOrigin(), camera->GetFocus()) : glm::mat3(1.0f);

	// Each tree as it is drawn this frame, gathered first, then laid out by mesh and detail
	_placedTrees.clear();
	registry.Each<const Mesh, const Transform, const Tree, const Swayable>(
	    [&](entt::entity entity, const Mesh& mesh, const Transform& transform, const Tree& /*unused*/,
	        const Swayable& swayable) {
		    const auto slots = _treeSlots.find(mesh.id);
		    if (slots == _treeSlots.end())
		    {
			    return;
		    }
		    auto modelMatrix = glm::mat4(transform.rotation);
		    modelMatrix = glm::translate(modelMatrix, transform.position * transform.rotation);
		    modelMatrix = glm::scale(modelMatrix, transform.scale);
		    // A body moving in the physics is drawn between its last two turns. Sunk wholly under the sea it isn't drawn at
		    // all: every vertex lands on one point, which draws nothing, and it neither sways nor bends, which would take its
		    // vertices out to the horizon
		    const auto placed = physics_draw::ModelMatrix(modelMatrix, registry.TryGet<const PhysicsDrawPose>(entity));
		    if (!placed.has_value())
		    {
			    _placedTrees.push_back({.mesh = mesh.id,
			                            .slots = &slots->second,
			                            .group = Group::Gone,
			                            .data = {.modelMatrix = glm::mat4(0.0f), .burning = glm::vec4(0.0f)},
			                            .box = glm::mat4(0.0f)});
			    return;
		    }
		    modelMatrix = *placed;

		    // Drawn in less detail the further off it is, fading out, then not at all, unless something holds it
		    auto group = Group::High;
		    std::optional<uint8_t> fade;
		    if (const auto& box = MeshBox(mesh.id);
		        camera != nullptr && box.has_value() && object_detail::BandedNow(registry, entity))
		    {
			    const auto choice = ChooseDetail(
			        *box, 0.0f, modelMatrix, transform.scale.x,
			        [&registry, entity] { return object_detail::Disappears(registry, entity, false); }, *camera, modelDetail);
			    if (!choice.mesh.has_value())
			    {
				    // Too far off to be drawn: kept only for what the land keeps of it
				    _placedTrees.push_back({.mesh = mesh.id,
				                            .slots = &slots->second,
				                            .group = Group::Gone,
				                            .data = {.modelMatrix = modelMatrix, .burning = glm::vec4(0.0f)},
				                            .box = glm::mat4(0.0f)});
				    return;
			    }
			    group = static_cast<Group>(*choice.mesh);
			    fade = choice.alpha;
		    }

		    // A tree with a fire on it is drawn darker, its foliage thinning as it burns, and narrows away at the last,
		    // keeping its height
		    glm::vec4 burning(0.0f);
		    if (Locator::fireSystem::has_value())
		    {
			    if (const auto look = Locator::fireSystem::value().GetBurningTreeLook(entity))
			    {
				    burning = {static_cast<float>(look->grey) / 256.0f, look->alphaReference / 255.0f, 0.0f, 0.0f};
				    modelMatrix = glm::scale(modelMatrix, glm::vec3(look->scale, 1.0f, look->scale));
			    }
		    }
		    if (fade.has_value())
		    {
			    burning.z = 1.0f - (static_cast<float>(*fade) / 255.0f);
			    group = Group::Fading;
		    }
		    // In its least detail a tree is a paper cut-out standing turned to the camera, which neither sways nor bends,
		    // unless it moves in the physics. Otherwise out of the land it neither sways nor bends: it is drawn as it is held
		    // or as the physics places it.
		    glm::mat4 drawnMatrix;
		    if (camera != nullptr && (group == Group::Low || group == Group::Fading) && !registry.AllOf<InPhysics>(entity))
		    {
			    drawnMatrix = graphics::mesh_detail::PaperMatrix(glm::vec3(modelMatrix[3]), transform.scale.x, paperTurn);
		    }
		    else
		    {
			    const auto place = registry.AnyOf<ShownRoots, InPhysics, InHand, HeldByCreature, CarriedByTornado>(entity)
			                           ? openblack::ecs::tree_roots::PlaceOf(registry, entity)
			                           : TreePlace::InTheLand;
			    drawnMatrix = openblack::physics::objects::SwaysAndBends(place)
			                      ? vegetation.GetTreeMatrix(modelMatrix, transform.position, transform.scale.y,
			                                                 slots->second.height, swayable.swaySlot)
			                      : modelMatrix;
		    }
		    _renderContext.drawnObjects.push_back({.entity = entity, .model = drawnMatrix});
		    glm::mat4 boxMatrix(0.0f);
		    if (drawBoundingBox && meshes.Contains(mesh.id))
		    {
			    const auto box = meshes.Handle(mesh.id)->GetBoundingBox();
			    boxMatrix = modelMatrix * glm::translate(box.Center()) * glm::scale(box.Size());
		    }
		    _placedTrees.push_back({.mesh = mesh.id,
		                            .slots = &slots->second,
		                            .group = group,
		                            .data = {.modelMatrix = drawnMatrix, .burning = burning},
		                            .box = boxMatrix});
	    });

	// Laid out within each mesh's instances by detail, each run drawn in its own: counted by mesh and detail first, then
	// each tree put in its place
	constexpr size_t k_Groups = static_cast<size_t>(Group::Gone) + 1;
	for (auto& [meshId, slots] : _treeSlots)
	{
		slots.treeGroups = {};
	}
	for (const auto& tree : _placedTrees)
	{
		++tree.slots->treeGroups.at(static_cast<size_t>(tree.group));
	}
	_renderContext.treeRuns.clear();
	bool fits = true;
	const auto low = graphics::mesh_detail::LevelBit(graphics::mesh_detail::Mesh::Low);
	for (auto& [meshId, slots] : _treeSlots)
	{
		slots.filled = 0;
		uint32_t first = slots.offset;
		for (size_t group = 0; group < k_Groups; ++group)
		{
			const auto count = slots.treeGroups.at(group);
			slots.treeGroups.at(group) = first;
			if (count == 0)
			{
				continue;
			}
			const auto drawn = static_cast<Group>(group);
			const bool fading = drawn == Group::Fading;
			const bool gone = drawn == Group::Gone;
			const auto level =
			    fading || gone ? low : graphics::mesh_detail::LevelBit(static_cast<graphics::mesh_detail::Mesh>(group));
			_renderContext.treeRuns.push_back(
			    {.mesh = meshId, .first = first, .count = count, .level = level, .fading = fading, .gone = gone});
			first += count;
			slots.filled += count;
		}
		fits = fits && slots.filled <= slots.count;
	}
	for (const auto& tree : _placedTrees)
	{
		if (!fits)
		{
			break;
		}
		const uint32_t idx = tree.slots->treeGroups.at(static_cast<size_t>(tree.group))++;
		_renderContext.treeInstanceData[idx] = tree.data;
		if (drawBoundingBox && idx + _renderContext.treeInstanceData.size() / 2 < _renderContext.treeInstanceData.size())
		{
			// Bounding box matrices in the second half of the instance data
			_renderContext.treeInstanceData[idx + (_renderContext.treeInstanceData.size() / 2)].modelMatrix = tree.box;
		}
	}

	if (fits && !_renderContext.treeInstanceData.empty())
	{
		const auto size =
		    static_cast<uint32_t>(_renderContext.treeInstanceData.size() * sizeof(RenderContext::TreeInstanceData));

		// Update the buffer with the combined data, copied: bgfx's render thread reads it while the next frame refills
		// (and may grow) the trees' instances
		bgfx::update(toBgfx(_renderContext.treeInstanceUniformBuffer), 0,
		             bgfx::copy(_renderContext.treeInstanceData.data(), size));
	}
	return fits;
}

const std::optional<openblack::AxisAlignedBoundingBox>& RenderingSystem::MeshBox(entt::id_type mesh)
{
	const auto found = _meshBoxes.find(mesh);
	if (found != _meshBoxes.end())
	{
		return found->second;
	}
	const auto& meshes = entt::locator<resources::ResourcesInterface>::value().GetMeshes();
	return _meshBoxes.emplace(mesh, meshes.Contains(mesh) ? std::optional(meshes.Handle(mesh)->GetBoundingBox()) : std::nullopt)
	    .first->second;
}
