/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The two object draws of the pass under the sea (graphics::sea_pass): DrawUnderWater (B: static, animated and
// complex objects) and the "cut" render mode DrawCutByPlane (C: animated and static objects). The original clips every
// triangle against the user plane on the CPU (one clipper shared by both) and lights each vertex itself (C); here the
// plane is a fragment discard in fs_object (sea_plane.sh) and the lights are vs_object's modes 2 / 3 (B) and 4 (C).

#include <limits>
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include <glm/mat4x4.hpp>

#include "3D/L3DMesh.h"
#include "3D/LandLight.h"
#include "3D/LandLightTable.h"
#include "3D/LandMorph.h"
#include "ECS/Components/CutByPlane.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Registry.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "Graphics/Mesh.h"
#include "Graphics/RenderModes.h"
#include "Graphics/SeaPass.h"
#include "Graphics/ShaderManager.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

void Renderer::DrawUnderWater(RenderPass viewId, const L3DMesh& mesh, std::unique_ptr<const InstanceDesc> instances,
                              const glm::mat4* matrices, uint8_t matrixCount, bool morphWithTerrain,
                              const sea_pass::SeaDraw& sea) const
{
	L3DMeshSubmitDesc submitDesc = {};
	submitDesc.viewId = viewId;
	// the material's own mode: the global alpha mode table is only taken when the object's global alpha flag is set.
	// (inferred) no caller here has that flag: the hand's object (a complex one) starts without it, and no call that
	// sets it on the hand's object was found; a setter through another pointer path is not ruled out. The moon does set
	// it before both its Draw and its DrawUnderWater, see Renderer::DrawMoon
	submitDesc.options = render_modes::k_ModelPass;
	submitDesc.sea = sea;
	submitDesc.instanceDesc = std::move(instances);
	submitDesc.modelMatrices = matrices;
	submitDesc.matrixCount = matrixCount;
	submitDesc.morphWithTerrain = morphWithTerrain;
	submitDesc.program = land_morph::ObjectProgram(*_shaderManager, morphWithTerrain);
	DrawMesh(mesh, submitDesc, std::numeric_limits<uint8_t>::max());
}

void Renderer::DrawUnderWater(RenderPass viewId, entt::entity entity, const sea_pass::SeaDraw& sea) const
{
	const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto instance = renderCtx.entityInstances.find(entity);
	if (!Locator::entitiesRegistry::value().Valid(entity) || instance == renderCtx.entityInstances.end() ||
	    !meshes.Contains(instance->second.meshId))
	{
		return;
	}
	// the morphable objects' (and the citadel's) DrawUnderWater does nothing: an object that morphs with the land is not
	// drawn under the sea, as DrawCutByPlane below
	if (instance->second.morphWithTerrain)
	{
		return;
	}
	const auto mesh = meshes.Handle(instance->second.meshId);
	static const auto k_Identity = glm::mat4(1.0f);
	DrawUnderWater(
	    viewId, *mesh, std::make_unique<graphics::InstanceDesc>(renderCtx.instanceUniformBuffer, instance->second.index, 1),
	    mesh->IsBoned() ? mesh->GetBoneMatrices().data() : &k_Identity,
	    mesh->IsBoned() ? static_cast<uint8_t>(mesh->GetBoneMatrices().size()) : 1, instance->second.morphWithTerrain, sea);
}

void Renderer::DrawCutByPlane(RenderPass viewId, entt::entity entity, sea_pass::SeaPlane plane, uint32_t argb,
                              uint32_t specular) const
{
	const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto instance = renderCtx.entityInstances.find(entity);
	if (!Locator::entitiesRegistry::value().Valid(entity) || instance == renderCtx.entityInstances.end() ||
	    !meshes.Contains(instance->second.meshId))
	{
		return;
	}
	// the morphable objects' (and the citadel's) DrawCutByPlane does nothing: cut, they are not drawn at all (none of
	// them is cut today)
	if (instance->second.morphWithTerrain)
	{
		return;
	}
	const auto mesh = meshes.Handle(instance->second.meshId);
	L3DMeshSubmitDesc submitDesc = {};
	submitDesc.viewId = viewId;
	// the material's normal mode: opaque with Z, the blended materials as DrawMesh blends them
	submitDesc.options = render_modes::k_ModelPass;
	// mirrored back inside openblack's mirrored Reflection pass: the cut draw never mirrors
	submitDesc.sea = sea_pass::Cut(plane, argb, specular, viewId);
	submitDesc.instanceDesc =
	    std::make_unique<graphics::InstanceDesc>(renderCtx.instanceUniformBuffer, instance->second.index, 1);
	static const auto k_Identity = glm::mat4(1.0f);
	// the animated objects skin their bones first, with the clip's pose of this frame
	// (ecs/Animations.h), the static ones draw as they are
	submitDesc.modelMatrices = mesh->IsBoned() ? mesh->GetBoneMatrices().data() : &k_Identity;
	submitDesc.matrixCount = mesh->IsBoned() ? static_cast<uint8_t>(mesh->GetBoneMatrices().size()) : 1;
	if (const auto* animation = Locator::entitiesRegistry::value().TryGet<const ecs::components::SkeletalAnimation>(entity);
	    animation != nullptr && mesh->IsBoned() && animation->pose.size() == mesh->GetBoneMatrices().size())
	{
		submitDesc.modelMatrices = animation->pose.data();
		submitDesc.matrixCount = static_cast<uint8_t>(animation->pose.size());
	}
	submitDesc.morphWithTerrain = false;
	submitDesc.program = land_morph::ObjectProgram(*_shaderManager, false);
	DrawMesh(*mesh, submitDesc, std::numeric_limits<uint8_t>::max());
}

void Renderer::DrawCutBelowWater(RenderPass viewId) const
{
	// the land draw's sharks under the water: each sets its own clip plane (0, -1, 0, 0) (sea_pass::k_SharkPlane),
	// keep y <= 0, the object's colour, DrawCutByPlane, then the plane (0, 1, 0, 0) again
	const auto plane = sea_pass::Kept(sea_pass::Mechanism::CutByPlane, sea_pass::k_SharkPlane);
	std::vector<std::pair<entt::entity, uint32_t>> cut;
	Locator::entitiesRegistry::value().Each<const ecs::components::CutByPlane>(
	    [&cut](entt::entity entity, const ecs::components::CutByPlane& component) {
		    cut.emplace_back(entity, component.belowColour);
	    });
	for (const auto& [entity, colour] : cut)
	{
		// the specular: 0, the shark's colour 0xFF303070 and specular 0 under the water, set before the cut draw
		DrawCutByPlane(viewId, entity, plane, colour, 0u);
	}
}

void Renderer::DrawCutAboveWater(RenderPass viewId) const
{
	// the shark's own Draw (in the normal object list): its colour is the land light table[255] of this frame with its
	// alpha, specular 0, then DrawCutByPlane with the default plane (0, 1, 0, 0): keep y >= 0
	std::vector<entt::entity> cut;
	Locator::entitiesRegistry::value().Each<const ecs::components::CutByPlane>(
	    [&cut](entt::entity entity, const ecs::components::CutByPlane& component) {
		    if (component.drawAbove)
		    {
			    cut.push_back(entity);
		    }
	    });
	const uint32_t colour = IsLandLit() ? land_light::FullLight(*_landLight) : 0xFFFFFFFFu;
	for (const auto entity : cut)
	{
		DrawCutByPlane(viewId, entity, sea_pass::Kept(sea_pass::Mechanism::CutByPlane, sea_pass::k_DefaultPlane), colour, 0u);
	}
}

std::unordered_set<uint32_t> Renderer::CutAboveInstances() const
{
	std::unordered_set<uint32_t> instances;
	const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
	Locator::entitiesRegistry::value().Each<const ecs::components::CutByPlane>(
	    [&](entt::entity entity, const ecs::components::CutByPlane& component) {
		    if (const auto instance = renderCtx.entityInstances.find(entity);
		        component.drawAbove && instance != renderCtx.entityInstances.end())
		    {
			    instances.insert(instance->second.index);
		    }
	    });
	return instances;
}
