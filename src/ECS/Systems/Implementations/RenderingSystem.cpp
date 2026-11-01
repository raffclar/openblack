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

#include <cmath>

#include <algorithm>
#include <array>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include <glm/gtx/transform.hpp>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "3D/PhysicsDrawMatrix.h"
#include "Camera/Camera.h"
#include "Creature/CreatureFizzLook.h"
#include "ECS/BoatFilmBoat.h"
#include "ECS/BoatLaunchRules.h"
#include "ECS/BuildingConstruction.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/AppleBarrel.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/BlendedByMaterial.h"
#include "ECS/Components/BoatFilm.h"
#include "ECS/Components/BuildingDamage.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/ChangedMaterials.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/DetailMeshes.h"
#include "ECS/Components/FallingRoots.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/Graveyard.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/HiddenByState.h"
#include "ECS/Components/HighDetail.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/NeverFades.h"
#include "ECS/Components/ObjectGlow.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ResourcePile.h"
#include "ECS/Components/RisingSoul.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/SeeThrough.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Stream.h"
#include "ECS/Components/Swayable.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Translucent.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Unlit.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerPose.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/DrawnPlace.h"
#include "ECS/DrawnRegistry.h"
#include "ECS/Graveyards.h"
#include "ECS/ObjectDetail.h"
#include "ECS/PlayerColour.h"
#include "ECS/Registry.h"
#include "ECS/SnowLevels.h"
#include "ECS/Systems/BuildingDamageSystemInterface.h"
#include "ECS/Systems/ExplosionSystemInterface.h"
#include "ECS/Systems/FieldSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HeartbeatSystemInterface.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "ECS/Systems/SpellIconSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/VegetationInterface.h"
#include "ECS/TreeRoots.h"
#include "ECS/VillageTotem.h"
#include "ECS/VillagerHeldObject.h"
#include "ECS/WorldObjects.h"
#include "Game.h"
#include "Graphics/BonePalette.h"
#include "Graphics/DebugLines.h"
#include "Graphics/DrawnHand.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/GroundBlobs.h"
#include "Graphics/InstanceLightPoint.h"
#include "Graphics/MeshDetail.h"
#include "Graphics/ObjectTint.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/TempleHeartLook.h"
#include "Graphics/Texture2D.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Physics/DamageMesh.h"
#include "Physics/ObjectRules.h"
#include "Profiler.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Scenery/AppleBarrelRules.h"
#include "Temple/TempleDestruction.h"
#include "Visual/DetailLevel.h"

using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
namespace object_detail = openblack::ecs::object_detail;
using openblack::physics::objects::TreePlace;

namespace
{
static_assert(openblack::graphics::object_tint::k_MostGrainFlow == openblack::magic::piles::k_GrainFlowSpan);

/// An icon at a worship site that shines is lit through this colour: all the light, with a little loss
constexpr uint32_t k_IconLitColour = 0xFFFFFFu;

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

/// Where the camera is and the way that it looks. The frame finds them one time, not one time for each thing.
struct ViewPoint
{
	glm::vec3 eye;
	glm::vec3 forward;

	[[nodiscard]] static ViewPoint Of(const openblack::Camera& camera)
	{
		return {.eye = camera.GetOrigin(), .forward = camera.GetForward()};
	}
};

/// How a thing is drawn this frame by how deep into the view the middle of `box` is, drawn by `model`: the box of its
/// own mesh, or of a villager's standard mesh. Whether it fades out is only asked of one that has come that far.
template <typename Disappears>
openblack::graphics::mesh_detail::Choice ChooseDetail(const openblack::AxisAlignedBoundingBox& box, float importance,
                                                      const glm::mat4& model, float scale, Disappears&& disappears,
                                                      const ViewPoint& camera, float modelDetail)
{
	namespace mesh_detail = openblack::graphics::mesh_detail;
	const auto centre = glm::vec3(model * glm::vec4(box.Center(), 1.0f));
	const float depth = mesh_detail::ViewDepth(centre, camera.eye, camera.forward);
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

/// Whether a model is a fence's: the fences read their snow by each face, as the trees do
bool FenceModel(entt::id_type meshId)
{
	using openblack::MeshId;
	constexpr std::array k_Fences {MeshId::BuildingAmericanFence, MeshId::BuildingCelticFenceShort,
	                               MeshId::BuildingCelticFenceTall};
	return std::ranges::any_of(k_Fences,
	                           [meshId](MeshId fence) { return openblack::resources::HashIdentifier(fence) == meshId; });
}

/// The model gets three corners of its own for each face, which the snow of a tree or a fence reads
void SplitFacesForSnow(entt::id_type meshId)
{
	auto& meshes = entt::locator<openblack::resources::ResourcesInterface>::value().GetMeshes();
	if (meshes.Contains(meshId))
	{
		meshes.Handle(meshId)->SplitFacesForSnow();
	}
}

/// The passes over the things that the land's draw list divides into segments (see graphics::fizz_draw_order)
struct SegmentPasses
{
	const openblack::ecs::Registry& registry;
	/// The segment that the pass places. Each pass increases it.
	uint32_t& placed;
	/// The last segment that a thing is in, found by the first pass
	const uint32_t& last;
};

/// Call `place` for each entity with a model and a transform: in the first pass, then again in a pass for each later
/// segment. `place` places only the things of the segment that the pass places. The phantom building of a scaffold
/// keeps its model in the copy, not in a Mesh, so `place` gets the model of the copy.
template <typename Place>
void EachBySegment(const Place& place, SegmentPasses passes)
{
	using namespace openblack::ecs::components;
	constexpr auto k_NotPlaced = entt::exclude<TempleInteriorPart, Tree, AtHome, HiddenByState, VillageTotemGhostHidden>;
	do
	{
		passes.registry.Each<const Mesh, const Transform>(place, k_NotPlaced);
		passes.registry.Each<const ScaffoldGhost, const Transform>(
		    [&place](entt::entity entity, const ScaffoldGhost& ghost, const Transform& transform) {
			    if (ghost.model.has_value())
			    {
				    place(entity, *ghost.model, transform);
			    }
		    },
		    k_NotPlaced);
	} while (passes.placed++ < passes.last);
}

/// Uploads the instances that changed since the last upload into the buffer
template <typename Instance>
void UploadChanged(bgfx::DynamicVertexBufferHandle buffer, std::span<const Instance> instances,
                   openblack::graphics::instance_upload::Mirror<Instance>& uploaded)
{
	for (const auto& run : uploaded.Take(instances))
	{
		// Copied, never referenced: bgfx's render thread reads the update while the next frame refills (and may grow,
		// freeing the old storage) these instances
		bgfx::update(buffer, run.first,
		             bgfx::copy(instances.data() + run.first, static_cast<uint32_t>(run.count * sizeof(Instance))));
	}
}

/// The hand that the frame draws: the left hand of the god of this computer
/// A script highlight that the frame does not draw
bool HiddenHighlight(const openblack::ecs::Registry& registry, entt::entity entity)
{
	const auto* highlight = registry.TryGet<const ScriptHighlight>(entity);
	return highlight != nullptr && highlight->hidden;
}

/// The plinths and the icons of the village totems that show nothing: the totems with no life
std::vector<entt::entity> LifelessTotemParts(const openblack::ecs::Registry& registry)
{
	using openblack::ecs::components::VillageTotem;
	std::vector<entt::entity> parts;
	registry.Each<const VillageTotem>([&parts](entt::entity totem, const VillageTotem& component) {
		if (!openblack::ecs::village_totem::Shows(openblack::ecs::world_objects::LifeOf(totem)))
		{
			parts.push_back(totem);
			parts.push_back(component.icon);
		}
	});
	return parts;
}

entt::entity DrawnHand()
{
	using openblack::ecs::systems::HandSystemInterface;
	if (!openblack::Locator::handSystem::has_value())
	{
		return entt::null;
	}
	return openblack::Locator::handSystem::value().GetOwnHands()[static_cast<size_t>(HandSystemInterface::Side::Left)];
}
} // namespace

RenderingSystem::~RenderingSystem() = default;

void RenderingSystem::PrepareDrawDescs(bool drawBoundingBox)
{
	auto section = Locator::profiler::value().BeginScoped(Profiler::Stage::UpdateEntitiesDescs);
	const auto& registry = Locator::entitiesRegistry::value();

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
		bool alphaTexturedAdditive;
		std::optional<glm::vec3> shineColour;
		std::optional<float> additiveShare;
		bool instanceAlpha;
		/// Every instance is a posed villager
		bool villagers;
		/// Every instance is a thing of the explorers' boat film
		bool film;
		/// Every instance is drawn with the first environment map
		bool environmentMapped;
		std::optional<graphics::render_modes::Properties> changedMaterials;
	};
	std::unordered_map<entt::id_type, MeshInstances> meshIds;
	std::unordered_map<entt::id_type, uint32_t> fadingCounts;
	_meshBoxes.clear();

	// `from` is the registry of the thing: the land's, or the drawn registry
	auto prepMesh = [&meshIds, &instanceCount](const Registry& from, entt::entity entity, entt::id_type drawnMesh,
	                                           const Mesh& mesh, bool morphWithTerrain) {
		auto count = meshIds.insert(std::make_pair(drawnMesh, MeshInstances {.count = static_cast<uint32_t>(mesh.submeshId),
		                                                                     .morphWithTerrain = morphWithTerrain,
		                                                                     .unlit = false,
		                                                                     .perEntity = false,
		                                                                     .translucent = false,
		                                                                     .alphaTexturedAdditive = false,
		                                                                     .shineColour = std::nullopt,
		                                                                     .additiveShare = std::nullopt,
		                                                                     .instanceAlpha = false,
		                                                                     .villagers = true,
		                                                                     .film = true,
		                                                                     .environmentMapped = true,
		                                                                     .changedMaterials = std::nullopt}));
		count.first->second.count++;
		count.first->second.unlit |= from.AnyOf<Unlit>(entity);
		// The creatures and the animals are each posed as they are
		count.first->second.perEntity |= from.AnyOf<CreatureMorph, AnimalPose, VillagerPose, AnimatedStaticPose>(entity);
		count.first->second.villagers &= from.AllOf<VillagerPose>(entity) && !morphWithTerrain;
		count.first->second.film &= from.AllOf<BoatFilmPart>(entity);
		// The game draws every scroll and sign that a script puts in the world with its first environment map
		count.first->second.environmentMapped &= from.AllOf<ScriptHighlight>(entity);
		if (const auto* translucent = from.TryGet<const Translucent>(entity))
		{
			count.first->second.translucent = true;
			count.first->second.additiveShare = translucent->share;
		}
		// Its solid parts are drawn as their materials say, and its alpha-textured parts are added over what is behind
		if (const auto* blended = from.TryGet<const BlendedByMaterial>(entity))
		{
			count.first->second.translucent = true;
			count.first->second.alphaTexturedAdditive = true;
			count.first->second.shineColour = blended->shineColour;
		}
		if (from.AllOf<SeeThrough>(entity))
		{
			count.first->second.translucent = true;
			count.first->second.instanceAlpha = true;
		}
		// Its model's materials changed while the game ran, which it is drawn by, blended by its alpha after the opaque
		// things
		if (const auto* changed = from.TryGet<const ChangedMaterials>(entity))
		{
			count.first->second.translucent = true;
			count.first->second.instanceAlpha = true;
			count.first->second.changedMaterials = changed->properties;
		}
		instanceCount++;
	};
	auto prep = [&registry, &prepMesh, &fadingCounts, &instanceCount](entt::entity entity, const Mesh& mesh,
	                                                                  bool morphWithTerrain) {
		const auto* detail = registry.TryGet<const DetailMeshes>(entity);
		if (detail == nullptr || morphWithTerrain)
		{
			const auto drawnMesh = DrawnMeshOf(entity, mesh);
			prepMesh(registry, entity, drawnMesh, mesh, morphWithTerrain);
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
				prepMesh(registry, entity, *it, mesh, morphWithTerrain);
			}
		}
		++fadingCounts[detail->meshes.back()];
		++instanceCount;
	};

	// Of the hands, the frame draws only the hand of the god of this computer
	const auto drawnHand = DrawnHand();
	registry.Each<const Mesh, const Transform>(
	    [&prep, &registry, drawnHand](entt::entity entity, const Mesh& mesh, const Transform& /*unused*/) {
		    if (graphics::drawn_hand::Drawn(registry.AllOf<Hand>(entity), entity, drawnHand) &&
		        !HiddenHighlight(registry, entity))
		    {
			    prep(entity, mesh, false);
		    }
	    },
	    entt::exclude<MorphWithTerrain, Tree, TempleInteriorPart, AtHome, HiddenByState, VillageTotemGhostHidden>);
	// The phantom building of a scaffold keeps its model in the copy, not in a Mesh
	registry.Each<const ScaffoldGhost, const Transform>(
	    [&prep](entt::entity entity, const ScaffoldGhost& ghost, const Transform& /*unused*/) {
		    if (ghost.model.has_value())
		    {
			    prep(entity, *ghost.model, false);
		    }
	    },
	    entt::exclude<MorphWithTerrain, Tree, TempleInteriorPart, AtHome, HiddenByState, VillageTotemGhostHidden>);
	registry.Each<const Mesh, const Transform, const MorphWithTerrain>(
	    [&prep](entt::entity entity, const Mesh& mesh, const Transform& /*unused*/, const MorphWithTerrain& /*unused*/) {
		    prep(entity, mesh, true);
	    },
	    entt::exclude<Tree>);
	// The things that only the frames make, as the stones on a plinth, are drawn as the things of the land of their model
	// are. Each one that can fade has room among those fading out too.
	if (Locator::drawnRegistry::has_value())
	{
		const auto& drawn = Locator::drawnRegistry::value();
		drawn.Each<const Mesh, const Transform>(
		    [&drawn, &prepMesh, &fadingCounts, &instanceCount](entt::entity entity, const Mesh& mesh,
		                                                       const Transform& /*unused*/) {
			    prepMesh(drawn, entity, mesh.id, mesh, false);
			    if (openblack::ecs::object_detail::Banded(drawn, entity))
			    {
				    ++fadingCounts[mesh.id];
				    ++instanceCount;
			    }
		    },
		    entt::exclude<HiddenByState>);
	}
	// The marks on the land, which only the frames draw, are drawn as the things of their model are. Each one has room
	// among those fading out too.
	if (Locator::explosionSystem::has_value())
	{
		for (const auto& mark : Locator::explosionSystem::value().GroundMarks())
		{
			auto count = meshIds.insert(std::make_pair(mark.mesh, MeshInstances {.count = 0,
			                                                                     .morphWithTerrain = mark.morphWithTerrain,
			                                                                     .unlit = false,
			                                                                     .perEntity = false,
			                                                                     .translucent = false,
			                                                                     .alphaTexturedAdditive = false,
			                                                                     .shineColour = std::nullopt,
			                                                                     .additiveShare = std::nullopt,
			                                                                     .instanceAlpha = false,
			                                                                     .villagers = true,
			                                                                     .film = true,
			                                                                     .environmentMapped = true,
			                                                                     .changedMaterials = std::nullopt}));
			count.first->second.count++;
			count.first->second.villagers = false;
			count.first->second.film = false;
			count.first->second.environmentMapped = false;
			// A mark that fades by its alpha is blended
			if (mark.alpha.has_value())
			{
				count.first->second.translucent = true;
				count.first->second.instanceAlpha = true;
			}
			++fadingCounts[mark.mesh];
			instanceCount += 2;
		}
	}
	// A tree out of the land has its roots drawn under it: once, or twice while it moves in the physics
	const Mesh roots {.id = RootsMesh(), .submeshId = 0, .bbSubmeshId = 0};
	registry.Each<const Tree, const ShownRoots>(
	    [&registry, &prepMesh, &roots](entt::entity tree, const Tree& /*unused*/, const ShownRoots& shown) {
		    for (size_t copy = 0; copy < openblack::physics::objects::ShownRootsScales(shown.place).size(); ++copy)
		    {
			    prepMesh(registry, tree, roots.id, roots, false);
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
		_uploadedInstances.Forget();
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
		// The sea doesn't reflect villagers, nor the explorers' boat film, which it mirrors in a look of its own
		drawDesc->second.hiddenFromReflection = desc.villagers || desc.film;
		// Blended by its materials, over the opaque things
		drawDesc->second.materialBlending = desc.translucent;
		drawDesc->second.translucent = desc.translucent;
		drawDesc->second.alphaTexturedAdditive = desc.alphaTexturedAdditive;
		drawDesc->second.shineColour = desc.shineColour;
		drawDesc->second.additiveShare = desc.additiveShare;
		drawDesc->second.instanceAlpha = desc.instanceAlpha;
		drawDesc->second.environmentMapped = desc.environmentMapped;
		if (FenceModel(meshId))
		{
			SplitFacesForSnow(meshId);
			drawDesc->second.snowByFace = true;
		}
		drawDesc->second.changedMaterials = desc.changedMaterials;
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
		drawDesc->second.environmentMapped = desc.has_value() && desc->environmentMapped;
		// The sea doesn't reflect villagers, nor the explorers' boat film, which it mirrors in a look of its own
		drawDesc->second.hiddenFromReflection = villagers || (desc.has_value() && desc->film);
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
	const auto& registry = Locator::entitiesRegistry::value();

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
		_uploadedTreeInstances.Forget();
		_renderContext.treeInstanceData.resize(treeInstanceCount);
	}

	// Determine tree uniform buffer offsets and instance count for draw
	uint32_t treeOffset = 0;
	_renderContext.treeInstancedDrawDescs.clear();
	_treeSlots.clear();
	auto& meshes = entt::locator<resources::ResourcesInterface>::value().GetMeshes();
	for (const auto& [meshId, count] : treeMeshIds)
	{
		// The trees read their snow by each face
		SplitFacesForSnow(meshId);
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
	const auto& registry = Locator::entitiesRegistry::value();

	for (auto& [meshId, slots] : _instanceSlots)
	{
		slots.filled = 0;
		slots.gone = 0;
		slots.segmentStarts.clear();
	}
	for (auto& [meshId, slots] : _fadingSlots)
	{
		slots.filled = 0;
		slots.segmentStarts.clear();
	}
	_renderContext.farVillagers.clear();
	_fadingThings.clear();
	_goneThings.clear();
	const auto* camera = Locator::camera::has_value() ? &Locator::camera::value() : nullptr;
	const auto view = camera != nullptr ? std::optional(ViewPoint::Of(*camera)) : std::nullopt;
	const float modelDetail = Locator::config::value().detail.modelDetail;

	const auto& vegetation = Locator::vegetation::value();

	// Set transforms for instanced draw at offsets
	_renderContext.entityDraws.clear();
	_renderContext.drawnEntityDraws.clear();
	_renderContext.drawnObjects.clear();
	_renderContext.animalDraws.clear();
	_renderContext.highDetailDraws.clear();
	_renderContext.filmShadowDraws.clear();
	_renderContext.filmReflections.clear();
	auto lastFilmLevels = std::exchange(_filmReflectionLevels, {});
	_renderContext.inHandDraws.clear();
	_renderContext.bonePalette.clear();
	_heldThings.clear();
	const auto& meshes = entt::locator<resources::ResourcesInterface>::value().GetMeshes();
	auto* drawList = Locator::objectDrawListSystem::has_value() ? &Locator::objectDrawListSystem::value() : nullptr;
	bool fits = true;
	// The creatures that fizz divide the land's draw list into segments. The things of each segment come after the
	// passes of the creature before them.
	_renderContext.fizzOrder = drawList != nullptr
	                               ? graphics::fizz_draw_order::Order::Of(drawList->GetList(),
	                                                                      [&registry, drawList](entt::entity entity) {
		                                                                      return FizzesInList(registry, *drawList, entity);
	                                                                      })
	                               : graphics::fizz_draw_order::Order {};
	const auto& fizzOrder = _renderContext.fizzOrder;
	// The things that only the frames make are in the first segment, before the things of the land. The gate stones that
	// the cursor finds come after the things of the land.
	std::vector<RenderContext::DrawnObject> drawnPicks;
	UploadDrawnThings(drawBoundingBox, fits, drawnPicks, lastFilmLevels);
	// The instances of each mesh go in the order of their segments. Each pass places the things of one segment.
	uint32_t segmentPlaced = 0;
	uint32_t lastSegment = 0;
	const auto drawnHand = DrawnHand();
	const auto lifelessTotemParts = LifelessTotemParts(registry);
	// The walk makes no component, so the stores of the components stay the same for all of it
	const openblack::ecs::drawn_place::DrawnPlaces drawnPlaces(registry);
	const openblack::ecs::ComponentLookup<VillagerPose> villagerPoses(registry);
	const openblack::ecs::ComponentLookup<HighDetail> highDetails(registry);
	const openblack::ecs::ComponentLookup<ScaffoldDrawnScale> drawnScales(registry);
	const openblack::ecs::ComponentLookup<PhysicsDrawPose> physicsPoses(registry);
	const openblack::ecs::ComponentLookup<DetailMeshes> detailMeshes(registry);
	EachBySegment(
	    [this, &registry, &vegetation, &meshes, &fits, &fizzOrder, &segmentPlaced, &lastSegment, drawBoundingBox, camera, &view,
	     modelDetail, drawList, drawnHand, &drawnPlaces, &villagerPoses, &highDetails, &drawnScales, &physicsPoses,
	     &detailMeshes, &lifelessTotemParts](entt::entity entity, const Mesh& mesh, const Transform& transform) {
		    // The hands of the other gods and the right hands have no model on the screen
		    if (!graphics::drawn_hand::Drawn(registry.AllOf<Hand>(entity), entity, drawnHand))
		    {
			    return;
		    }
		    // A script highlight that its draw list did not select in this frame is not drawn
		    if (HiddenHighlight(registry, entity))
		    {
			    return;
		    }
		    // A village totem with no life shows nothing. It stays drawn while the camera does not move.
		    if (std::ranges::find(lifelessTotemParts, entity) != lifelessTotemParts.end())
		    {
			    if (drawList != nullptr && drawList->GetFrame().drawAll && drawList->NotesKept(entity))
			    {
				    drawList->NoteFullFrame(entity, true);
			    }
			    return;
		    }
		    const auto segment = fizzOrder.SegmentOf(entity);
		    lastSegment = std::max(lastSegment, segment);
		    if (segment != segmentPlaced)
		    {
			    return;
		    }
		    // A villager is drawn gliding between its turns and turning after the way it faces; one held by the opening's
		    // hand is drawn in its grip
		    const auto* villagerPose = villagerPoses.TryGet(entity);
		    // A walking creature is drawn between its last two turns, and what it holds in its drawn hand
		    const auto drawnPlace = drawnPlaces.Of(entity, transform);
		    const auto standsAt = DrawnPosition(drawnPlace.position, villagerPose);
		    const auto rotation = DrawnRotation(drawnPlace.rotation, villagerPose);
		    const auto* highDetail = highDetails.TryGet(entity);
		    const auto drawnAt =
		        highDetail != nullptr ? openblack::ecs::high_detail_rules::DrawnAt(highDetail->heldAt, standsAt) : standsAt;
		    // A scaffold on a building site becomes small as the building grows
		    const auto* drawnScale = drawnScales.TryGet(entity);
		    auto modelMatrix =
		        PlacedMatrix(drawnAt, rotation, drawnScale != nullptr ? glm::vec3(drawnScale->scale) : transform.scale);
		    // A body moving in the physics is drawn between its last two turns, and not at all once sunk under the sea
		    const auto* drawn = physicsPoses.TryGet(entity);
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
		    const auto* detail = detailMeshes.TryGet(entity);
		    // On a frame that draws all of the land's draw list, note which listed things stay drawn while the camera
		    // stands still. The icon of a village totem is noted too: it keeps its totem drawn. A thing of several meshes
		    // uses the box of its standard mesh. Another thing uses its own mesh, also a broken building, which shows its
		    // broken model.
		    const bool noteKept = drawList != nullptr && drawList->GetFrame().drawAll && drawList->NotesKept(entity);
		    const auto keptBox =
		        detail != nullptr ? detail->meshes.at(static_cast<size_t>(graphics::mesh_detail::Mesh::Standard)) : mesh.id;
		    bool noted = false;
		    if (drawList != nullptr && !drawList->Drawn(registry, entity))
		    {
			    // Of the things on the land's blocks only those in the land's draw list are drawn. One left out leaves
			    // what it stands on painted, as one too far off does; the living and the creatures leave nothing.
			    if (detail != nullptr || registry.AllOf<CreatureMorph>(entity))
			    {
				    return;
			    }
			    gone = true;
			    _goneThings.push(entity);
		    }
		    else if (const auto* ghost = registry.TryGet<const ScaffoldGhost>(entity); ghost != nullptr)
		    {
			    // The phantom building of a scaffold is drawn whole at any distance, blended by its own alpha among the
			    // things that fade out
			    slotMap = &_fadingSlots;
			    fade = ghost->alpha;
			    _fadingThings.emplace(entity, ghost->alpha);
		    }
		    else if (openblack::ecs::high_detail_rules::DrawsHighMeshAtAnyDistance(detail != nullptr, highDetails.Has(entity),
		                                                                           registry.AllOf<MorphWithTerrain>(entity)))
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
				    const float drawImportance = object_detail::Importance(registry, entity);
				    const auto choice = ChooseDetail(
				        *box, drawImportance, modelMatrix, transform.scale.x,
				        [&registry, entity] {
					        return object_detail::Disappears(registry, entity, IsTownCentre(registry, entity));
				        },
				        *view, modelDetail);
				    if (noteKept)
				    {
					    NoteKept(*drawList, registry, entity, modelMatrix, transform.scale.x, keptBox,
					             !choice.mesh.has_value());
					    noted = true;
				    }
				    if (!choice.mesh.has_value())
				    {
					    if (detail == nullptr)
					    {
						    gone = true;
						    _goneThings.push(entity);
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

		    if (noteKept && !noted)
		    {
			    NoteKept(*drawList, registry, entity, modelMatrix, transform.scale.x, keptBox, false);
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
		    graphics::fizz_draw_order::NoteSegment(slots->second.segmentStarts, slots->second.filled, segment);
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
			    // When a part of it shows, the cursor can point at it. The pick tests its whole model, where it stands.
			    if (Locator::buildingDamageSystem::value().PartialShare(entity).has_value())
			    {
				    _renderContext.drawnObjects.push_back({.entity = entity, .model = modelMatrix});
			    }
			    return;
		    }
		    // A home with someone in lights its windows at night
		    const auto* abode = registry.TryGet<const Abode>(entity);
		    // The w is the snow that the thing shows. A broken building shows the snow of its broken model.
		    glm::vec4 look {abode != nullptr && abode->presentAtHome > 0 ? 1.0f : 0.0f, 0.0f, 0.0f,
		                    static_cast<float>(openblack::ecs::snow_levels::ShownLevelOf(registry, entity))};
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
		    float grainFlow = 0.0f;
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
					    grainFlow = magic::piles::GrainFlow(*sink, pile->height);
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
				    look.w = -static_cast<float>(creature_spells::IceAlpha(spells->freeze)) / 255.0f;
			    }
		    }

		    // A colour added over it after its texture: the heal's glow on the people it heals, the glow of a fire on it
		    // and the poison on a pot. A house's positive x is its windows' light, and a house's glow goes above it: 1 plus
		    // the glow while someone is home.
		    const bool onFire = registry.AllOf<Fire>(entity);
		    uint32_t added = 0;
		    if (const auto* glow = registry.TryGet<const ObjectGlow>(entity); glow != nullptr && abode == nullptr)
		    {
			    added = glow->Packed();
		    }
		    // A see-through copy is blended by its own alpha
		    if (const auto* seeThrough = registry.TryGet<const SeeThrough>(entity))
		    {
			    look.z = -(1.0f - static_cast<float>(seeThrough->alpha) / 255.0f);
		    }
		    // A poisoned pot with no fire on it is tinted green, on the ground and in the hand
		    if (const auto* pot = registry.TryGet<const Pot>(entity); pot != nullptr)
		    {
			    if (const auto poison = graphics::object_tint::PotColours(pot->poisoned, onFire))
			    {
				    look.y = static_cast<float>(poison->tint);
				    added = graphics::object_tint::SumAdded(added, poison->added);
			    }
		    }
		    // Something charred by fire is drawn grey
		    if (look.y == 0.0f && Locator::fireSystem::has_value())
		    {
			    if (const auto charred = Locator::fireSystem::value().GetCharredColour(entity))
			    {
				    look.y = static_cast<float>(*charred);
			    }
		    }
		    // Something hot glows red-orange, a pot or a pile too
		    if (onFire && Locator::fireSystem::has_value())
		    {
			    if (const auto heat = Locator::fireSystem::value().GetGlowColour(entity); heat.has_value() && *heat != 0)
			    {
				    if (abode != nullptr)
				    {
					    look.x = look.x > 0.5f ? 1.0f + static_cast<float>(*heat) : -static_cast<float>(*heat);
				    }
				    else
				    {
					    added = graphics::object_tint::SumAdded(added, *heat);
				    }
			    }
		    }
		    // An icon at a worship site that shows its charge is lit through white, and it shines in its player's colour
		    if (Locator::spellIconSystem::has_value() && registry.AllOf<WorshipIcon>(entity))
		    {
			    if (const auto shine = Locator::spellIconSystem::value().ShownTint(entity); shine != 0)
			    {
				    look.y = static_cast<float>(k_IconLitColour);
				    added = graphics::object_tint::SumAdded(added, shine);
			    }
		    }
		    // The heart of a temple is lit through white. In its destruction it glows in its player's colour, then fades
		    // out, and it is not drawn just before the temple goes.
		    if (const auto* temple = registry.TryGet<const Temple>(entity); temple != nullptr)
		    {
			    namespace heart_look = graphics::temple_heart_look;
			    if (look.y == 0.0f)
			    {
				    look.y = static_cast<float>(heart_look::k_Colour);
			    }
			    // The glow that the heart takes is the one set the frame before
			    auto& glow = _heartGlows[entity];
			    if (!temple->destroying)
			    {
				    glow = 0;
			    }
			    added = graphics::object_tint::SumAdded(added, glow);
			    if (heart_look::Glows(temple->destroying, temple->destructionClock))
			    {
				    const float turnFraction = Locator::time::has_value() ? Locator::time::value().GetTurnFraction() : 0.0f;
				    const float pulse = Locator::heartbeatSystem::has_value()
				                            ? Locator::heartbeatSystem::value().Pulse(temple->owner, turnFraction)
				                            : 0.0f;
				    const auto colour = player_colour::Of(temple->owner);
				    glow = heart_look::Glow(colour, heart_look::GlowStrength(temple->destructionClock, pulse));
			    }
			    if (registry.AllOf<ChangedMaterials>(entity))
			    {
				    const auto alpha = heart_look::DrawnAlpha(temple_destruction::HeartAlpha(temple->destructionClock));
				    look.z = -(1.0f - static_cast<float>(alpha) / 255.0f);
				    look.w = static_cast<float>(heart_look::FadedSnowLevel(static_cast<int32_t>(look.w), alpha));
			    }
			    if (!heart_look::Drawn(temple->destroying, temple->destructionClock))
			    {
				    look.z = 1.0f;
				    modelMatrix = glm::mat4(0.0f);
			    }
		    }
		    if (abode == nullptr)
		    {
			    look.x = graphics::object_tint::PackAdded(added, grainFlow);
		    }

		    // A flag over a town's store, the wood flag of a workshop or a need flag of a worship site is not there at all
		    // while it does not show
		    const auto* townFlag = registry.TryGet<const TownDesireFlag>(entity);
		    const auto* woodFlag = registry.TryGet<const WorkshopFlag>(entity);
		    const auto* needFlag = registry.TryGet<const WorshipSiteFlag>(entity);
		    if ((townFlag != nullptr && !townFlag->shown) || (woodFlag != nullptr && !woodFlag->shown) ||
		        (needFlag != nullptr && !needFlag->shown))
		    {
			    look.z = 1.0f;
			    modelMatrix = glm::mat4(0.0f);
		    }

		    // A lantern on the Norse gate shows only while the gate is in the draw list and on screen. It takes the land's
		    // light of the point where the gate stands, and no snow.
		    std::optional<glm::vec2> lightOffset;
		    if (const auto* lantern = registry.TryGet<const GateLantern>(entity); lantern != nullptr)
		    {
			    const auto* gatePose =
			        registry.Valid(lantern->gate) ? registry.TryGet<const AnimatedStaticPose>(lantern->gate) : nullptr;
			    const auto* gateTransform =
			        registry.Valid(lantern->gate) ? registry.TryGet<const Transform>(lantern->gate) : nullptr;
			    if (gatePose == nullptr || gateTransform == nullptr)
			    {
				    look.z = 1.0f;
			    }
			    else
			    {
				    const auto lanternLook = openblack::animated_static::GateLanternLook(
				        gatePose->inDrawList, gatePose->onScreen,
				        glm::vec2(gateTransform->position.x, gateTransform->position.z),
				        glm::vec2(transform.position.x, transform.position.z));
				    look.z = lanternLook.shown ? look.z : 1.0f;
				    look.w = lanternLook.snowCap;
				    lightOffset = lanternLook.lightOffset;
			    }
		    }

		    // A body sunk wholly under the sea isn't drawn in any pass, its shadow included
		    if (!placed.has_value())
		    {
			    look.z = 1.0f;
			    modelMatrix = glm::mat4(0.0f);
		    }

		    // A building going up is drawn only as far up as it stands, with the broken and the unfinished ones, though
		    // what of it stands can still be pointed at. A building with all of it built is drawn whole, also before it
		    // is finished.
		    const auto* progress = registry.TryGet<const BuildProgress>(entity);
		    const auto* feature = registry.TryGet<const Feature>(entity);
		    const bool goingUp =
		        DrawnMeshOf(entity, mesh) == mesh.id &&
		        building_construction::DrawnGoingUp(progress != nullptr ? std::optional(progress->built) : std::nullopt,
		                                            feature != nullptr ? std::optional(feature->type) : std::nullopt);
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
		    if (lightOffset.has_value())
		    {
			    graphics::instance_light::SetLightOffset(instanceModel, *lightOffset);
		    }
		    _renderContext.instanceUniforms[idx] = {.model = instanceModel, .look = look};
		    _renderContext.instanceLevels[idx] = graphics::mesh_detail::LevelBit(level);
		    // A villager's bones go into the palette, as its clip poses it or as its model rests, and its instance says
		    // where they start
		    if (slots->second.bonePalette)
		    {
			    const auto* pose = villagerPoses.TryGet(entity);
			    const auto& rest = meshes.Handle(slots->first)->GetBoneMatrices();
			    const auto bones = pose != nullptr && pose->bones.size() == rest.size()
			                           ? std::span<const glm::mat4>(pose->bones)
			                           : std::span<const glm::mat4>(rest);
			    graphics::bone_palette::SetFirstBone(_renderContext.instanceUniforms[idx].model,
			                                         graphics::bone_palette::Append(_renderContext.bonePalette, bones));
		    }
		    // What a villager holds is drawn on the bone at the end of its arm, as its clip poses it, with the villager's
		    // look; not while a hand or a creature holds it
		    const auto holder = registry.AllOf<InHand>(entity)           ? openblack::ecs::villager_held::Holder::PlayersHand
		                        : registry.AllOf<HeldByCreature>(entity) ? openblack::ecs::villager_held::Holder::Creature
		                                                                 : openblack::ecs::villager_held::Holder::None;
		    if (const auto* person = registry.TryGet<const Villager>(entity);
		        person != nullptr && villagerPose != nullptr && look.z != 1.0f && placed.has_value() &&
		        openblack::ecs::villager_held::DrawsHeldThing(holder))
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
						    const float depth = graphics::mesh_detail::ViewDepth(centre, view->eye, view->forward);
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
						    // What a villager carries is one model for all the villagers, which the land never gives
						    // snow
						    heldLook.w = 0.0f;
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
			    if (registry.AllOf<Animal>(entity))
			    {
				    _renderContext.animalDraws.push_back({.entity = entity, .mesh = drawnMesh, .model = modelMatrix});
			    }
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
			        {.entity = entity, .mesh = drawnMesh, .model = modelMatrix, .castModel = modelMatrix, .instance = idx});
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
	    SegmentPasses {.registry = registry, .placed = segmentPlaced, .last = lastSegment});
	std::ranges::copy(drawnPicks, std::back_inserter(_renderContext.drawnObjects));

	// The marks on the land, which only the frames draw. They are drawn in less detail the further off they are, and fade
	// out, as the other things of the land do. A mark that fades by its own alpha is drawn whole until it has gone.
	if (Locator::explosionSystem::has_value())
	{
		for (const auto& mark : Locator::explosionSystem::value().GroundMarks())
		{
			if (!fits)
			{
				break;
			}
			const auto modelMatrix = PlacedMatrix(mark.transform.position, mark.transform.rotation, mark.transform.scale);
			auto* slotMap = &_instanceSlots;
			std::optional<uint8_t> fade;
			auto level = graphics::mesh_detail::Mesh::High;
			bool gone = false;
			if (view.has_value() && openblack::ecs::object_detail::MarkBandedNow(mark.alpha))
			{
				if (const auto& box = MeshBox(mark.mesh); box.has_value())
				{
					const auto choice =
					    ChooseDetail(*box, 0.0f, modelMatrix, mark.transform.scale.x, [] { return true; }, *view, modelDetail);
					if (!choice.mesh.has_value())
					{
						gone = true;
					}
					else
					{
						level = *choice.mesh;
						if (choice.alpha.has_value())
						{
							slotMap = &_fadingSlots;
							fade = choice.alpha;
						}
					}
				}
			}
			const auto slots = slotMap->find(mark.mesh);
			if (slots == slotMap->end() || slots->second.filled + slots->second.gone >= slots->second.count)
			{
				fits = false;
				break;
			}
			// One too far off keeps a place at the back of its model's instances, not drawn
			const uint32_t idx = gone ? slots->second.offset + slots->second.count - 1 - slots->second.gone
			                          : slots->second.offset + slots->second.filled;
			glm::vec4 look {graphics::object_tint::PackAdded(0, 0.0f), 0.0f, 0.0f, 0.0f};
			if (gone)
			{
				look.z = 1.0f;
			}
			else if (mark.alpha.has_value())
			{
				look.z = -(1.0f - static_cast<float>(*mark.alpha) / 255.0f);
			}
			else if (fade.has_value())
			{
				look.z = -(1.0f - (static_cast<float>(*fade) / 255.0f));
			}
			_renderContext.instanceUniforms[idx] = {.model = modelMatrix, .look = look};
			_renderContext.instanceLevels[idx] = graphics::mesh_detail::LevelBit(level);
			if (drawBoundingBox)
			{
				_renderContext.instanceUniforms[idx + (_renderContext.instanceUniforms.size() / 2)] = {.model =
				                                                                                           glm::mat4(0.0f)};
			}
			if (gone)
			{
				++slots->second.gone;
			}
			else
			{
				++slots->second.filled;
			}
		}
	}

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
		    // A tree in a creature's hand has its roots where the drawn hand holds it
		    const auto drawnPlace = openblack::ecs::drawn_place::DrawnTransformOf(registry, tree, transform);
		    const auto standing = PlacedMatrix(drawnPlace.position, drawnPlace.rotation, transform.scale);
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

	if (fits)
	{
		UploadChanged(toBgfx(_renderContext.instanceUniformBuffer),
		              std::span<const RenderContext::ObjectInstance>(_renderContext.instanceUniforms), _uploadedInstances);
	}
	for (auto& [meshId, desc] : _renderContext.instancedDrawDescs)
	{
		const auto slots = _instanceSlots.find(meshId);
		desc.filled = slots != _instanceSlots.end() ? slots->second.filled : 0;
		desc.gone = slots != _instanceSlots.end() ? slots->second.gone : 0;
		if (slots != _instanceSlots.end())
		{
			desc.segmentStarts = slots->second.segmentStarts;
		}
		else
		{
			desc.segmentStarts.clear();
		}
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

void RenderingSystem::UploadDrawnThings(bool drawBoundingBox, bool& fits, std::vector<RenderContext::DrawnObject>& picks,
                                        const std::unordered_map<entt::entity, uint8_t>& lastFilmLevels)
{
	if (!Locator::drawnRegistry::has_value())
	{
		return;
	}
	namespace object_detail = openblack::ecs::object_detail;
	const auto& drawn = Locator::drawnRegistry::value();
	const auto* camera = Locator::camera::has_value() ? &Locator::camera::value() : nullptr;
	const auto view = camera != nullptr ? std::optional(ViewPoint::Of(*camera)) : std::nullopt;
	const float modelDetail = Locator::config::value().detail.modelDetail;
	const auto& meshes = entt::locator<resources::ResourcesInterface>::value().GetMeshes();
	// The matrix of the explorers' boat that the frame shows, if its film plays. The parts on its deck take the land's
	// light of its place.
	const auto filmBoat = openblack::ecs::boat_film::ShownBoatMatrix(drawn);
	// A thing of the drawn registry has only its model, its place and its look: the systems of the land know nothing of
	// it, so it takes nothing from them
	drawn.Each<const Mesh, const Transform>(
	    [&](entt::entity entity, const Mesh& mesh, const Transform& transform) {
		    if (!fits)
		    {
			    return;
		    }
		    // A thing that a film moves by a whole matrix uses that matrix
		    const auto* exact = drawn.TryGet<const DrawnMatrix>(entity);
		    const auto modelMatrix =
		        exact != nullptr ? exact->model : PlacedMatrix(transform.position, transform.rotation, transform.scale);
		    // The sea mirrors the explorers' boat, and no other part of its film, wherever it is and however far off. It
		    // mirrors the boat at the detail of the frame before.
		    const auto* filmPart = drawn.TryGet<const BoatFilmPart>(entity);
		    const bool mirroredBoat = filmPart != nullptr && filmPart->role == BoatFilmPart::Role::Boat;
		    if (mirroredBoat)
		    {
			    const auto last = lastFilmLevels.find(entity);
			    const auto lastLevel = last != lastFilmLevels.end()
			                               ? last->second
			                               : graphics::mesh_detail::LevelBit(graphics::mesh_detail::Mesh::High);
			    _filmReflectionLevels[entity] = lastLevel;
			    _renderContext.filmReflections.push_back({.mesh = mesh.id, .model = modelMatrix, .detailLevel = lastLevel});
		    }
		    auto* slotMap = &_instanceSlots;
		    std::optional<uint8_t> fade;
		    auto level = graphics::mesh_detail::Mesh::High;
		    bool gone = false;
		    // It is drawn in less detail the further off it is, fades out, then is not drawn, as a thing of the land is
		    if (camera != nullptr && object_detail::Banded(drawn, entity) && object_detail::BandedNow(drawn, entity))
		    {
			    if (const auto& box = MeshBox(mesh.id); box.has_value())
			    {
				    const auto choice = ChooseDetail(
				        *box, object_detail::Importance(drawn, entity), modelMatrix, transform.scale.x,
				        [&drawn, entity] { return object_detail::Disappears(drawn, entity, false); }, *view, modelDetail);
				    if (!choice.mesh.has_value())
				    {
					    gone = true;
				    }
				    else
				    {
					    level = *choice.mesh;
					    if (choice.alpha.has_value())
					    {
						    slotMap = &_fadingSlots;
						    fade = choice.alpha;
					    }
				    }
			    }
		    }
		    if (mirroredBoat && !gone)
		    {
			    _filmReflectionLevels[entity] = graphics::mesh_detail::LevelBit(level);
		    }
		    // One too far off to be drawn keeps a place at the back of its model's instances
		    const auto slots = (gone ? &_instanceSlots : slotMap)->find(mesh.id);
		    if (slots == (gone ? &_instanceSlots : slotMap)->end() ||
		        slots->second.filled + slots->second.gone >= slots->second.count)
		    {
			    fits = false;
			    return;
		    }
		    if (gone)
		    {
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
		    graphics::fizz_draw_order::NoteSegment(slots->second.segmentStarts, slots->second.filled, 0);
		    // The w is the snow that the thing shows
		    glm::vec4 look {graphics::object_tint::PackAdded(0, 0.0f), 0.0f, 0.0f,
		                    static_cast<float>(openblack::ecs::snow_levels::LevelOf(drawn, entity))};
		    if (fade.has_value())
		    {
			    look.z = -(1.0f - (static_cast<float>(*fade) / 255.0f));
		    }
		    const uint32_t idx = slots->second.offset + slots->second.filled;
		    // The worm on the apple barrel takes the land's light of the point where the barrel stands
		    auto instanceModel = modelMatrix;
		    if (const auto* worm = drawn.TryGet<const AppleBarrelWorm>(entity); worm != nullptr && drawn.Valid(worm->barrel))
		    {
			    if (const auto* barrelTransform = drawn.TryGet<const Transform>(worm->barrel); barrelTransform != nullptr)
			    {
				    graphics::instance_light::SetLightOffset(
				        instanceModel, apple_barrel::WormLightOffset(barrelTransform->position, transform.position));
			    }
		    }
		    // A part on the deck of the explorers' boat takes the land's light of the boat's place, and no colour of the land
		    if (filmPart != nullptr && filmPart->role == BoatFilmPart::Role::DeckPart && filmBoat.has_value())
		    {
			    graphics::instance_light::SetLightOffset(instanceModel,
			                                             openblack::ecs::boat_launch::DeckLightOffset(*filmBoat, modelMatrix));
			    look.y = graphics::instance_light::k_NoLandColour;
		    }
		    _renderContext.instanceUniforms[idx] = {.model = instanceModel, .look = look};
		    _renderContext.instanceLevels[idx] = graphics::mesh_detail::LevelBit(level);
		    // The boat casts its shadow from the far sun while the sailors push it down the beach. It has no entity of the
		    // land.
		    if (filmPart != nullptr && filmPart->castsShadow)
		    {
			    _renderContext.filmShadowDraws.push_back(
			        {.entity = entt::null, .mesh = mesh.id, .model = modelMatrix, .castModel = modelMatrix});
		    }
		    // One posed by its own clip, as the worm, is drawn by itself
		    if (slots->second.perEntity)
		    {
			    _renderContext.drawnEntityDraws.push_back({.entity = entity, .instance = idx});
		    }
		    // A gate stone that the cursor can find is the plinth that holds it to the cursor
		    if (const auto* stone = drawn.TryGet<const PlinthStone>(entity); stone != nullptr && stone->pickable)
		    {
			    picks.push_back({.entity = stone->plinth, .model = modelMatrix, .mesh = mesh.id});
		    }
		    if (drawBoundingBox)
		    {
			    const auto box = meshes.Handle(mesh.id)->GetBoundingBox();
			    const auto boxMatrix = modelMatrix * glm::translate(box.Center()) * glm::scale(box.Size());
			    _renderContext.instanceUniforms[idx + (_renderContext.instanceUniforms.size() / 2)] = {.model = boxMatrix};
		    }
		    ++slots->second.filled;
	    },
	    entt::exclude<HiddenByState>);
}

void RenderingSystem::UploadSouls()
{
	const auto& registry = Locator::entitiesRegistry::value();
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
	const auto& registry = Locator::entitiesRegistry::value();
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
		// The building shows its own snow, and fades out in the distance as the whole building does
		const auto snow = static_cast<float>(openblack::ecs::snow_levels::LevelOf(registry, entity));
		const auto look = fading != _fadingThings.end()
		                      ? glm::vec4(0.0f, 0.0f, -(1.0f - (static_cast<float>(fading->second) / 255.0f)), snow)
		                      : glm::vec4(0.0f, 0.0f, 0.0f, snow);
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
		draw.segment = _renderContext.fizzOrder.SegmentOf(entity);
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
		    .segment = _renderContext.fizzOrder.SegmentOf(entity),
		    .fading = fading != _fadingThings.end(),
		});
		_renderContext.partialBuildInstances.push_back(
		    {.model = glm::translate(glm::mat4(1.0f), transform.position) * glm::mat4(transform.rotation) *
		              glm::scale(glm::mat4(1.0f), transform.scale),
		     .look = glm::vec4(0.0f, 0.0f,
		                       fading != _fadingThings.end() ? -(1.0f - (static_cast<float>(fading->second) / 255.0f)) : 0.0f,
		                       static_cast<float>(openblack::ecs::snow_levels::LevelOf(registry, entity)))});
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
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& vegetation = Locator::vegetation::value();
	const auto* camera = Locator::camera::has_value() ? &Locator::camera::value() : nullptr;
	const auto view = camera != nullptr ? std::optional(ViewPoint::Of(*camera)) : std::nullopt;
	const float modelDetail = Locator::config::value().detail.modelDetail;
	const auto& meshes = entt::locator<resources::ResourcesInterface>::value().GetMeshes();
	using Group = PlacedTree::Group;
	// Every far tree turns the same way, by the camera's heading
	const auto paperTurn =
	    camera != nullptr ? graphics::mesh_detail::PaperTurn(camera->GetOrigin(), camera->GetFocus()) : glm::mat3(1.0f);

	auto* drawList = Locator::objectDrawListSystem::has_value() ? &Locator::objectDrawListSystem::value() : nullptr;

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
		    // A tree in a creature's hand is drawn where the drawn hand is
		    const auto drawnPlace = openblack::ecs::drawn_place::DrawnTransformOf(registry, entity, transform);
		    auto modelMatrix = PlacedMatrix(drawnPlace.position, drawnPlace.rotation, transform.scale);
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

		    // A tree left out of the land's draw list isn't drawn, as one too far off isn't
		    if (drawList != nullptr && !drawList->Drawn(registry, entity))
		    {
			    _placedTrees.push_back({.mesh = mesh.id,
			                            .slots = &slots->second,
			                            .group = Group::Gone,
			                            .data = {.modelMatrix = modelMatrix, .burning = glm::vec4(0.0f)},
			                            .box = glm::mat4(0.0f)});
			    return;
		    }

		    // On a frame that draws all of the land's draw list, note whether a listed tree stays drawn while the camera
		    // stands still
		    const bool noteKept = drawList != nullptr && drawList->GetFrame().drawAll && drawList->Listed(entity);

		    // Drawn in less detail the further off it is, fading out, then not at all, unless something holds it
		    auto group = Group::High;
		    std::optional<uint8_t> fade;
		    if (const auto& box = MeshBox(mesh.id);
		        camera != nullptr && box.has_value() && object_detail::BandedNow(registry, entity))
		    {
			    const auto choice = ChooseDetail(
			        *box, 0.0f, modelMatrix, transform.scale.x,
			        [&registry, entity] { return object_detail::Disappears(registry, entity, false); }, *view, modelDetail);
			    if (noteKept)
			    {
				    NoteKept(*drawList, registry, entity, modelMatrix, transform.scale.x, mesh.id, !choice.mesh.has_value());
			    }
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
		    else if (noteKept)
		    {
			    NoteKept(*drawList, registry, entity, modelMatrix, transform.scale.x, mesh.id, false);
		    }

		    // A tree with a fire on it is drawn darker, its foliage thinning as it burns, and narrows away at the last,
		    // keeping its height
		    glm::vec4 burning(0.0f);
		    float widthShare = 1.0f;
		    if (Locator::fireSystem::has_value())
		    {
			    if (const auto look = Locator::fireSystem::value().GetBurningTreeLook(entity))
			    {
				    burning = {static_cast<float>(look->grey) / 256.0f, look->alphaReference / 255.0f, 0.0f, 0.0f};
				    modelMatrix = glm::scale(modelMatrix, glm::vec3(look->scale, 1.0f, look->scale));
				    widthShare = look->scale;
			    }
		    }
		    if (fade.has_value())
		    {
			    burning.z = 1.0f - (static_cast<float>(*fade) / 255.0f);
			    group = Group::Fading;
		    }
		    burning.w = static_cast<float>(openblack::ecs::snow_levels::LevelOf(registry, entity));
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
			                      ? vegetation.GetTreeMatrix(entity, modelMatrix, drawnPlace.position, transform.scale.y,
			                                                 swayable.swaySlot)
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
		                            .segment = _renderContext.fizzOrder.SegmentOf(entity),
		                            .data = {.modelMatrix = drawnMatrix, .burning = burning},
		                            .box = boxMatrix,
		                            .entity = entity,
		                            .held = registry.AllOf<InHand>(entity),
		                            .widthShare = widthShare});
	    });

	// Laid out within each mesh's instances by detail, each run drawn in its own: counted by mesh and detail first, then
	// each tree put in its place. The solid trees of each later segment of the land's draw list have runs of their own,
	// after the first five groups.
	constexpr size_t k_Groups = static_cast<size_t>(Group::Gone) + 1;
	constexpr size_t k_SolidGroups = static_cast<size_t>(Group::Fading);
	const auto groupOf = [](const PlacedTree& tree) {
		const auto group = static_cast<size_t>(tree.group);
		if (tree.segment == 0 || group >= k_SolidGroups)
		{
			return group;
		}
		return k_Groups + ((tree.segment - 1) * k_SolidGroups) + group;
	};
	for (auto& [meshId, slots] : _treeSlots)
	{
		slots.treeGroups.assign(k_Groups, 0);
	}
	for (const auto& tree : _placedTrees)
	{
		const auto group = groupOf(tree);
		if (tree.slots->treeGroups.size() <= group)
		{
			tree.slots->treeGroups.resize(group + 1, 0);
		}
		++tree.slots->treeGroups.at(group);
	}
	_renderContext.treeRuns.clear();
	_renderContext.heldTreeDraws.clear();
	bool fits = true;
	const auto low = graphics::mesh_detail::LevelBit(graphics::mesh_detail::Mesh::Low);
	for (auto& [meshId, slots] : _treeSlots)
	{
		slots.filled = 0;
		uint32_t first = slots.offset;
		for (size_t group = 0; group < slots.treeGroups.size(); ++group)
		{
			const auto count = slots.treeGroups.at(group);
			slots.treeGroups.at(group) = first;
			if (count == 0)
			{
				continue;
			}
			const bool laterSegment = group >= k_Groups;
			const auto detail = laterSegment ? (group - k_Groups) % k_SolidGroups : group;
			const auto segment = laterSegment ? static_cast<uint32_t>(((group - k_Groups) / k_SolidGroups) + 1) : 0u;
			const auto drawn = static_cast<Group>(detail);
			const bool fading = drawn == Group::Fading;
			const bool gone = drawn == Group::Gone;
			const auto level =
			    fading || gone ? low : graphics::mesh_detail::LevelBit(static_cast<graphics::mesh_detail::Mesh>(detail));
			_renderContext.treeRuns.push_back({.mesh = meshId,
			                                   .first = first,
			                                   .count = count,
			                                   .level = level,
			                                   .fading = fading,
			                                   .gone = gone,
			                                   .segment = segment});
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
		const uint32_t idx = tree.slots->treeGroups.at(groupOf(tree))++;
		_renderContext.treeInstanceData[idx] = tree.data;
		// A tree in a hand is mirrored in the sea with the hand
		if (tree.held)
		{
			_renderContext.heldTreeDraws.push_back({.entity = tree.entity,
			                                        .mesh = tree.mesh,
			                                        .model = tree.data.modelMatrix,
			                                        .castModel = tree.data.modelMatrix,
			                                        .instance = idx,
			                                        .widthShare = tree.widthShare});
		}
		if (drawBoundingBox && idx + _renderContext.treeInstanceData.size() / 2 < _renderContext.treeInstanceData.size())
		{
			// Bounding box matrices in the second half of the instance data
			_renderContext.treeInstanceData[idx + (_renderContext.treeInstanceData.size() / 2)].modelMatrix = tree.box;
		}
	}

	if (fits)
	{
		UploadChanged(toBgfx(_renderContext.treeInstanceUniformBuffer),
		              std::span<const RenderContext::TreeInstanceData>(_renderContext.treeInstanceData),
		              _uploadedTreeInstances);
	}
	return fits;
}

bool RenderingSystem::FizzesInList(const Registry& registry, const ObjectDrawListSystemInterface& drawList, entt::entity entity)
{
	const auto* spells = registry.TryGet<const CreatureSpells>(entity);
	return spells != nullptr && creature_fizz_look::Fizzing(spells->fizz) && registry.AllOf<CreatureMorph>(entity) &&
	       drawList.Drawn(registry, entity);
}

void RenderingSystem::NoteKept(ObjectDrawListSystemInterface& drawList, const Registry& registry, entt::entity entity,
                               const glm::mat4& model, float scale, entt::id_type boxMesh, bool fadedOut)
{
	// Villagers and creatures stay drawn wherever they are. A building that its site draws, or a broken building, has
	// no fade test.
	const auto test = object_draw_list::KeptTestOf({
	    .alwaysDrawn = registry.AnyOf<Villager, Creature>(entity),
	    .broken = registry.AllOf<BuildingDamage>(entity),
	    .drawnWhole = DrawnWhole(entity),
	    .partlyShown = Locator::buildingDamageSystem::has_value() &&
	                   Locator::buildingDamageSystem::value().PartialShare(entity).has_value(),
	});
	const bool fadeCounts = test == object_draw_list::KeptTest::SphereAndFade && fadedOut;
	bool onScreen = true;
	if (test != object_draw_list::KeptTest::Always && !fadeCounts && drawList.GetView().has_value())
	{
		if (const auto& box = MeshBox(boxMesh); box.has_value())
		{
			const auto centre = glm::vec3(model * glm::vec4(box->Center(), 1.0f));
			const float radius = std::abs(scale) * graphics::mesh_detail::HalfDiagonal(box->Size());
			onScreen = object_draw_list::SphereOnScreen(*drawList.GetView(), glm::vec3(model[3]), centre, radius);
		}
	}
	drawList.NoteFullFrame(entity, object_draw_list::StaysDrawn(test, onScreen, fadedOut));
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
