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
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <unordered_map>

#include <glm/gtx/transform.hpp>

#include "3D/L3DMesh.h"
#include "3D/PhysicsDrawMatrix.h"
#include "Camera/Camera.h"
#include "ECS/BuildingConstruction.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/DetailMeshes.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/GroundMark.h"
#include "ECS/Components/HiddenByState.h"
#include "ECS/Components/HighDetail.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/ObjectGlow.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ResourcePile.h"
#include "ECS/Components/SeeThrough.h"
#include "ECS/Components/SkinOverride.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Stream.h"
#include "ECS/Components/Swayable.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Translucent.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Unlit.h"
#include "ECS/Components/VillagerPose.h"
#include "ECS/Registry.h"
#include "ECS/Systems/BuildingDamageSystemInterface.h"
#include "ECS/Systems/FieldSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/VegetationInterface.h"
#include "Game.h"
#include "Graphics/BonePalette.h"
#include "Graphics/DebugLines.h"
#include "Graphics/DetailLevel.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/GroundBlobs.h"
#include "Graphics/MeshDetail.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Physics/DamageMesh.h"
#include "Profiler.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

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

/// Which of a villager's meshes it is drawn as this frame, by how deep into the view the middle of its standard mesh's
/// bounding sphere is, drawn by `model`
openblack::graphics::mesh_detail::Choice ChooseDetail(const DetailMeshes& detail, const glm::mat4& model, float scale,
                                                      bool disappears, const openblack::Camera& camera, float modelDetail)
{
	namespace mesh_detail = openblack::graphics::mesh_detail;
	const auto& meshes = entt::locator<openblack::resources::ResourcesInterface>::value().GetMeshes();
	const auto bounding = detail.meshes.at(static_cast<size_t>(mesh_detail::Mesh::Standard));
	if (!meshes.Contains(bounding))
	{
		return {.mesh = mesh_detail::Mesh::High, .alpha = std::nullopt};
	}
	const auto box = meshes.Handle(bounding)->GetBoundingBox();
	const auto centre = glm::vec3(model * glm::vec4(box.Center(), 1.0f));
	const float depth = mesh_detail::ViewDepth(centre, camera.GetOrigin(), camera.GetForward());
	const float reach = mesh_detail::Reach(detail.importance, mesh_detail::ScaledRadius(box.Size(), scale), modelDetail);
	return mesh_detail::Choose(depth, reach, disappears);
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
		bool castsShadow;
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

	auto prepMesh = [&registry, &meshIds, &instanceCount](entt::entity entity, entt::id_type drawnMesh, const Mesh& mesh,
	                                                      bool morphWithTerrain) {
		auto count = meshIds.insert(std::make_pair(drawnMesh, MeshInstances {.count = static_cast<uint32_t>(mesh.submeshId),
		                                                                     .morphWithTerrain = morphWithTerrain,
		                                                                     .castsShadow = false,
		                                                                     .unlit = false,
		                                                                     .perEntity = false,
		                                                                     .translucent = false,
		                                                                     .additiveShare = std::nullopt,
		                                                                     .instanceAlpha = false,
		                                                                     .villagers = true}));
		count.first->second.count++;
		// The things whose shadows Black & White bakes into the land (IsCastShadowAtNight), and its features
		count.first->second.castsShadow |= registry.AnyOf<Abode, Feature, MobileStatic, StoragePit>(entity);
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
			prepMesh(entity, DrawnMeshOf(entity, mesh), mesh, morphWithTerrain);
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
	}

	// Determine uniform buffer offsets and instance count for draw
	uint32_t offset = 0;
	_renderContext.instancedDrawDescs.clear();
	_instanceSlots.clear();
	for (const auto& [meshId, desc] : meshIds)
	{
		const auto [drawDesc, inserted] = _renderContext.instancedDrawDescs.emplace(
		    std::piecewise_construct, std::forward_as_tuple(meshId),
		    std::forward_as_tuple(offset, desc.count, desc.morphWithTerrain, desc.castsShadow));
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
		_instanceSlots.emplace(meshId, InstanceSlots {.offset = offset,
		                                              .count = desc.count,
		                                              .filled = 0,
		                                              .perEntity = desc.perEntity,
		                                              .height = 0.0f,
		                                              .bonePalette = desc.villagers});
		offset += desc.count;
	}
	// The villagers fading out in the distance, blended by their own alpha
	_renderContext.fadingDrawDescs.clear();
	_fadingSlots.clear();
	for (const auto& [meshId, count] : fadingCounts)
	{
		const auto [drawDesc, inserted] = _renderContext.fadingDrawDescs.emplace(
		    std::piecewise_construct, std::forward_as_tuple(meshId), std::forward_as_tuple(offset, count, false, false));
		drawDesc->second.perEntity = true;
		drawDesc->second.bonePalette = true;
		drawDesc->second.instanceAlpha = true;
		drawDesc->second.hiddenFromReflection = true;
		_fadingSlots.emplace(
		    meshId, InstanceSlots {
		                .offset = offset, .count = count, .filled = 0, .perEntity = true, .height = 0.0f, .bonePalette = true});
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
		                                              std::forward_as_tuple(treeOffset, count, false, true));
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
	}
	for (auto& [meshId, slots] : _fadingSlots)
	{
		slots.filled = 0;
	}
	_renderContext.farVillagers.clear();
	const auto* camera = Locator::camera::has_value() ? &Locator::camera::value() : nullptr;
	const float modelDetail = graphics::detail_level::ModelDetail(Locator::config::value().detailLevel);

	const auto& vegetation = Locator::vegetation::value();

	// Set transforms for instanced draw at offsets
	_renderContext.entityDraws.clear();
	_renderContext.drawnObjects.clear();
	_renderContext.bonePalette.clear();
	const auto& meshes = entt::locator<resources::ResourcesInterface>::value().GetMeshes();
	bool fits = true;
	registry.Each<const Mesh, const Transform>(
	    [this, &registry, &vegetation, &meshes, &fits, drawBoundingBox, camera,
	     modelDetail](entt::entity entity, const Mesh& mesh, const Transform& transform) {
		    // One held by the opening's hand is drawn in its grip, facing as it does
		    const auto* highDetail = registry.TryGet<const HighDetail>(entity);
		    const auto drawnAt =
		        highDetail != nullptr && highDetail->heldAt.has_value() ? *highDetail->heldAt : transform.position;
		    auto modelMatrix = glm::mat4(transform.rotation);
		    modelMatrix = glm::translate(modelMatrix, drawnAt * transform.rotation);
		    modelMatrix = glm::scale(modelMatrix, transform.scale);
		    // A body moving in the physics is drawn between its last two turns, and not at all once sunk under the sea
		    const auto* drawn = registry.TryGet<const PhysicsDrawPose>(entity);
		    const auto placed = physics_draw::ModelMatrix(modelMatrix, drawn);
		    modelMatrix = placed.value_or(glm::mat4(0.0f));

		    // A villager is drawn in less detail the further off it is, fades out, then shows only as a smudge on a blob
		    // while it stands above the sea. One flying in the physics never fades.
		    auto drawnMesh = DrawnMeshOf(entity, mesh);
		    auto* slotMap = &_instanceSlots;
		    std::optional<uint8_t> fade;
		    if (const auto* detail = registry.TryGet<const DetailMeshes>(entity);
		        detail != nullptr && camera != nullptr && !registry.AllOf<MorphWithTerrain>(entity))
		    {
			    const auto choice = ChooseDetail(*detail, modelMatrix, transform.scale.x, !registry.AllOf<InPhysics>(entity),
			                                     *camera, modelDetail);
			    if (!choice.mesh.has_value())
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
			    drawnMesh = detail->meshes.at(static_cast<size_t>(*choice.mesh));
			    if (choice.alpha.has_value())
			    {
				    slotMap = &_fadingSlots;
				    fade = choice.alpha;
			    }
		    }

		    // A mesh the draw lists don't have room for, which has changed since they were made
		    const auto slots = slotMap->find(drawnMesh);
		    if (!fits || slots == slotMap->end() || slots->second.filled >= slots->second.count)
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
				    if (const auto* swayable = registry.TryGet<const Swayable>(entity); swayable != nullptr && crop->sways)
				    {
					    modelMatrix = vegetation.GetFieldMatrix(modelMatrix, transform.scale.y, swayable->swaySlot);
				    }
			    }
		    }

		    // A pile of food or wood is drawn as far under the ground as it has sunk, not at all once wholly under, and the
		    // grain of a pile of food flows down it as it rises
		    if (const auto* pile = registry.TryGet<const ResourcePile>(entity); pile != nullptr && pile->height > 0.0f)
		    {
			    if (!magic::piles::Shown(pile->rise, pile->height))
			    {
				    look.z = 1.0f;
			    }
			    else
			    {
				    modelMatrix[3].y += pile->rise.offset;
				    const auto* pot = registry.TryGet<const Pot>(entity);
				    if (pot != nullptr && magic::piles::GrainFlows(pot->type))
				    {
					    look.x = -magic::piles::GrainFlow(pile->rise.offset, pile->height);
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
		    if (look.z != 1.0f || (goingUp && progress->built > 0.0f))
		    {
			    _renderContext.drawnObjects.push_back({.entity = entity, .model = modelMatrix});
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

	if (fits && !_renderContext.instanceUniforms.empty())
	{
		const auto size = static_cast<uint32_t>(_renderContext.instanceUniforms.size() * sizeof(RenderContext::ObjectInstance));
		bgfx::update(toBgfx(_renderContext.instanceUniformBuffer), 0,
		             bgfx::makeRef(_renderContext.instanceUniforms.data(), size));
	}
	for (auto& [meshId, desc] : _renderContext.instancedDrawDescs)
	{
		const auto slots = _instanceSlots.find(meshId);
		desc.filled = slots != _instanceSlots.end() ? slots->second.filled : 0;
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
	return fits;
}

void RenderingSystem::UploadBonePalette()
{
	auto& texels = _renderContext.bonePalette;
	if (texels.empty())
	{
		return;
	}
	// Whole rows are sent, the last one filled out
	const auto rows = graphics::bone_palette::RowsFor(texels.size());
	texels.resize(static_cast<size_t>(rows) * graphics::bone_palette::k_Width, glm::vec4(0.0f));
	auto& texture = _renderContext.bonePaletteTexture;
	// The texture grows to twice the rows it needs, so that a growing crowd doesn't make it again every frame
	if (!texture || texture->GetResolution().y < rows)
	{
		texture = std::make_unique<graphics::Texture2D>("BonePalette");
		texture->CreateWithinFrame(graphics::bone_palette::k_Width, static_cast<uint16_t>(rows * 2), 1,
		                           graphics::TextureFormat::RGBA32F, graphics::Wrapping::ClampEdge, graphics::Filter::Nearest,
		                           nullptr);
	}
	const auto bytes = static_cast<uint32_t>(texels.size() * sizeof(glm::vec4));
	bgfx::updateTexture2D(toBgfx(texture->GetNativeHandle()), 0, 0, 0, 0, graphics::bone_palette::k_Width, rows,
	                      bgfx::copy(texels.data(), bytes));
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
		if (!share.has_value() || !meshes.Contains(mesh.id))
		{
			return;
		}
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
		};
		_renderContext.partialBuildInstances.push_back({.model = matrix});
		// The scaffold sinks along its up axis while the building rises out of the land
		const glm::vec3 up = transform.rotation[1];
		draw.scaffoldInstance = static_cast<uint32_t>(_renderContext.partialBuildInstances.size());
		_renderContext.partialBuildInstances.push_back(
		    {.model = glm::translate(glm::mat4(1.0f), -up * build.scaffoldSink) * matrix});
		_renderContext.partialBuilds.push_back(draw);
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

	for (auto& [meshId, slots] : _treeSlots)
	{
		slots.filled = 0;
	}
	const auto& vegetation = Locator::vegetation::value();

	// Set the transforms of the trees, swaying or bent away from the hand
	bool fits = true;
	registry.Each<const Mesh, const Transform, const Tree, const Swayable>([this, &registry, &fits, drawBoundingBox,
	                                                                        &vegetation](entt::entity entity, const Mesh& mesh,
	                                                                                     const Transform& transform,
	                                                                                     const Tree& /*unused*/,
	                                                                                     const Swayable& swayable) {
		const auto slots = _treeSlots.find(mesh.id);
		if (!fits || slots == _treeSlots.end() || slots->second.filled >= slots->second.count)
		{
			fits = false;
			return;
		}

		const uint32_t idx = slots->second.offset + slots->second.filled;
		auto modelMatrix = glm::mat4(transform.rotation);
		modelMatrix = glm::translate(modelMatrix, transform.position * transform.rotation);
		modelMatrix = glm::scale(modelMatrix, transform.scale);
		// A body moving in the physics is drawn between its last two turns. Sunk wholly under the sea it isn't drawn at
		// all: every vertex lands on one point, which draws nothing, and it neither sways nor bends, which would take its
		// vertices out to the horizon
		const auto placed = physics_draw::ModelMatrix(modelMatrix, registry.TryGet<const PhysicsDrawPose>(entity));
		if (!placed.has_value())
		{
			_renderContext.treeInstanceData[idx] = {.modelMatrix = glm::mat4(0.0f), .burning = glm::vec4(0.0f)};
			++slots->second.filled;
			return;
		}
		modelMatrix = *placed;
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
		_renderContext.treeInstanceData[idx].burning = burning;
		_renderContext.treeInstanceData[idx].modelMatrix = vegetation.GetTreeMatrix(
		    modelMatrix, transform.position, transform.scale.y, slots->second.height, swayable.swaySlot);
		_renderContext.drawnObjects.push_back({.entity = entity, .model = _renderContext.treeInstanceData[idx].modelMatrix});

		if (drawBoundingBox && idx + _renderContext.treeInstanceData.size() / 2 < _renderContext.treeInstanceData.size())
		{
			auto l3dMesh = entt::locator<resources::ResourcesInterface>::value().GetMeshes().Handle(mesh.id);
			auto box = l3dMesh->GetBoundingBox();
			auto boxMatrix = modelMatrix * glm::translate(box.Center()) * glm::scale(box.Size());

			// Store bounding box matrix in the second half of the instance data array
			_renderContext.treeInstanceData[idx + (_renderContext.treeInstanceData.size() / 2)].modelMatrix = boxMatrix;
		}
		++slots->second.filled;
	});

	if (fits && !_renderContext.treeInstanceData.empty())
	{
		const auto size =
		    static_cast<uint32_t>(_renderContext.treeInstanceData.size() * sizeof(RenderContext::TreeInstanceData));

		// Update the buffer with the combined data
		bgfx::update(toBgfx(_renderContext.treeInstanceUniformBuffer), 0,
		             bgfx::makeRef(_renderContext.treeInstanceData.data(), size));
	}
	return fits;
}
