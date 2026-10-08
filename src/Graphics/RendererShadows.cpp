/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The projected shadows (graphics::shadow_list; wiki: docs/bw1-notes/rendering.md, "Sombras proyectadas"): their
// update once a frame, their draw over the land blocks and over the objects (the tail loop of the objects' Draw).
//
// Where they go in the frame, against the Z-sorter (graphics::zsort, drained at the end of the frame after everything
// drawn at once): none is a Z object of its own and both draw at once:
// - over the land: right after each block of the main land, so before the queue (Renderer::DrawPass's block loop,
//   view Main);
// - over an object: at the tail of that object's own Draw (static, animated and the other object kinds alike). That
//   Draw runs at once for an opaque object and from the queue for a blended one (its Z object's callback is the Draw),
//   so the shadow goes right after the object either in the main view or inside the object's Z object, and takes no
//   entry of the queue (its 2048 cap) of its own.

#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <unordered_set>
#include <vector>

#include <glm/mat4x4.hpp>
#include <spdlog/spdlog.h>

#include "3D/Billboard.h"
#include "3D/L3DMesh.h"
#include "3D/LandBlock.h"
#include "3D/LandMorph.h"
#include "Camera/Camera.h"
#include "ECS/Animations.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Transform.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "Graphics/DetailLevel.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/RenderModes.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/ShadowList.h"
#include "Graphics/VertexBuffer.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
/// The shadow's texture stage in the land redraw: past the 0..10 of vs_terrain / fs_terrain, whose bindings stay for
/// the next blocks (the original binds it to stage 0, through the shadow's material)
constexpr uint8_t k_LandShadowStage = 11;

/// x0, z0, 1 / (x1 - x0), 1 / (z1 - z0) of the shadow's box
glm::vec4 BoxUniform(const shadow_math::Box& box)
{
	return {box.x0, box.z0, 1.0f / (box.x1 - box.x0), 1.0f / (box.z1 - box.z0)};
}

/// The cull code's inputs: the projection's d.x, d.z and the least k
glm::vec4 CullUniform(const shadow_list::ShadowInfo& shadow)
{
	return {shadow.projection.dir.x, shadow.projection.dir.z, shadow.box.kMin, 0.0f};
}

/// OPENBLACK_SHADOW_TRACE's two frame counts, in the debug hooks' store (Locator::debugHooks)
struct RendererShadowsDebugHooksState
{
	uint32_t receiversTraceFrame {0}; ///< CollectShadowReceivers' count
	uint32_t onObjectTraceCount {0};  ///< DrawShadowsOnObject's count
};

RendererShadowsDebugHooksState& RendererShadowsDebugHooksData()
{
	if (!Locator::debugHooks::has_value())
	{
		std::fputs("renderer shadows: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return Locator::debugHooks::value().Get<RendererShadowsDebugHooksState>();
}
} // namespace

void Renderer::UpdateShadows(const DrawSceneDesc& drawDesc) const
{
	if (!drawDesc.drawIsland || !drawDesc.drawEntities)
	{
		_shadows->Clear();
		return;
	}
	shadow_list::FrameInputs inputs;
	inputs.camera = drawDesc.camera->GetOrigin();
	const auto frame = billboard::CameraFrame::From(*drawDesc.camera);
	// the world to clipping matrix (affine::FrameMatrices) and the near plane
	inputs.worldToClipping = frame.clipMatrices.worldToClipping;
	inputs.nearW = frame.nearZ;
	inputs.landRef = GetDetailLevel(Locator::config::value().detailLevel).landReflection;
	_shadows->Frame(inputs);
}

void Renderer::DrawLandShadows(RenderPass viewId, const LandBlock& block, uint64_t cull) const
{
	const auto* program = _shaderManager->GetShader("LandShadow");
	if (program == nullptr)
	{
		return;
	}
	const glm::vec2 corner = block.GetMapPosition();
	// the block's (bx, bz) for the box test: its corner / 160
	const int blockX = static_cast<int>(corner.x / shadow_math::k_BlockSize);
	const int blockZ = static_cast<int>(corner.y / shadow_math::k_BlockSize);
	const glm::vec4 u_blockPositionAndSize(corner, shadow_math::k_BlockSize, shadow_math::k_BlockSize);
	// The shadow material: mode 6, one-sided, SRCALPHA / INVSRCALPHA, no Z write, Z LESSEQUAL over the block just
	// drawn, the block's own culling. openblack's depth is
	// reversed and State's LessEqual is GREATER, which would drop the equal depth of the redraw: LessEqualInclusive
	// (GEQUAL; vs_land_shadow computes the depth as vs_terrain does, land_position.sh). The colour's alpha is not written.
	const uint64_t state = render_modes::State(render_modes::Mode::AlphaTexturedAlphaNoZWrite,
	                                           {.zFunc = render_modes::ZFunc::LessEqualInclusive, .msaa = true}) |
	                       cull;
	// the vertex streams, the state and the transform go after each draw; the terrain's bindings stay
	constexpr auto k_Discard = BGFX_DISCARD_INSTANCE_DATA | BGFX_DISCARD_INDEX_BUFFER | BGFX_DISCARD_TRANSFORM |
	                           BGFX_DISCARD_VERTEX_STREAMS | BGFX_DISCARD_STATE;
	// every active shadow whose box touches the block, newest first; all of them, onObjects is not read here
	_shadows->ForEachActive([&](const shadow_list::ShadowInfo& shadow) {
		if (!shadow_math::TouchesBlock(shadow.box, blockX, blockZ))
		{
			return;
		}
		// the shadow's light and its t' (one H per shadow)
		const glm::vec4 u_shadowLight(shadow.projection.light, shadow.landT);
		const auto u_shadowBox = BoxUniform(shadow.box);
		const auto u_shadowCull = CullUniform(shadow);
		program->SetUniformValue("u_blockPositionAndSize", &u_blockPositionAndSize);
		program->SetUniformValue("u_shadowLight", &u_shadowLight);
		program->SetUniformValue("u_shadowBox", &u_shadowBox);
		program->SetUniformValue("u_shadowCull", &u_shadowCull);
		program->SetTextureSampler("s_shadow", k_LandShadowStage, fromBgfx(shadow.texture.Get()));
		block.GetMesh().GetVertexBuffer().Bind();
		bgfx::setState(state, 0);
		bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()), 0, k_Discard);
	});
}

void Renderer::CollectShadowReceivers(bool mainView) const
{
	_shadowReceivers.clear();
	// "ShadowsOnObjects" detail key; only the main view (the reflection draws no shadows)
	if (!mainView || !GetDetailLevel(Locator::config::value().detailLevel).shadowsOnObjects)
	{
		return;
	}
	// the entries cast on objects: the hand and the launched boat (the creature too, not here yet), in the list's order
	// (newest first)
	std::vector<const shadow_list::ShadowInfo*> shadows;
	_shadows->ForEachActive([&shadows](const shadow_list::ShadowInfo& shadow) {
		if (shadow.onObjects)
		{
			shadows.push_back(&shadow);
		}
	});
	if (shadows.empty())
	{
		return;
	}
	const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto& registry = Locator::entitiesRegistry::value();
	// holding an object clears its flag of receiving
	// the render hand's object: set when it is picked up (at the press), undone when it is thrown
	const auto held = Locator::handSystem::has_value() ? Locator::handSystem::value().GetRenderHandObject() : std::nullopt;
	for (const auto& [entity, instance] : renderCtx.entityInstances)
	{
		// the receiver: RenderingSystem's ReceivesDynamicShadow
		if (!instance.receivesDynamicShadow || (held.has_value() && *held == entity) || !meshes.Contains(instance.meshId))
		{
			continue;
		}
		// a broken house's FragMesh and a fragment are not drawn by an object's Draw, so its shadow loop never runs over
		// them; the partly built part over it is drawn without one either
		if (registry.AllOf<ecs::components::Fragment>(entity))
		{
			continue;
		}
		if (const auto* damage = registry.TryGet<const ecs::components::BuildingDamage>(entity);
		    damage != nullptr && damage->generatedMesh == instance.meshId)
		{
			continue;
		}
		const auto mesh = meshes.Handle(instance.meshId);
		// the bounding box test: the mesh box centre +- half its size, moved to the object, x and z only; the morphable
		// Draw has its own test instead (shadow_math::ReachesMorphable)
		const auto& meshBox = mesh->GetBoundingBox();
		const auto* transform = registry.TryGet<const ecs::components::Transform>(entity);
		if (transform == nullptr)
		{
			continue;
		}
		// the object's matrix (x and z read only). (inferred) valid while no MorphWithTerrain entity gets
		// RenderingSystem's per-instance edits of that matrix (the fields' and trees' wind sway, the trees' bend, the
		// burning trees' shrink): those edits are the original's draw matrix, not the object's; none carries the
		// component
		const auto& model = renderCtx.instanceUniforms[instance.index];
		const float scale = transform->scale.x;
		const float halfDiagonal = ecs::object::MeshHalfDiagonal(instance.meshId);
		const glm::vec2 centre =
		    glm::vec2(meshBox.Center().x, meshBox.Center().z) + glm::vec2(transform->position.x, transform->position.z);
		const glm::vec2 half = glm::vec2(meshBox.Size().x, meshBox.Size().z) * 0.5f;
		for (const auto* shadow : shadows)
		{
			// not the caster of the shadow, and not a complex object's own shadow (the hand's body)
			if (shadow->caster == entity)
			{
				continue;
			}
			const auto& box = shadow->box;
			// (inferred) MorphWithTerrain stands for the morphable object class here (3D type 1). The citadel class (3D
			// type 8) draws through the static Draw: the bounding box test and ZFUNC EQUAL. No type 8 entity carries the
			// component today (CitadelArchetype has none; CitadelPart is type 1)
			if (instance.morphWithTerrain)
			{
				// the morphable Draw's test (no complex object test there, only the caster's)
				if (!shadow_math::ReachesMorphable(box, meshBox.Center(), model, scale, halfDiagonal))
				{
					continue;
				}
			}
			else if (centre.x + half.x < box.x0 || centre.x - half.x > box.x1 || centre.y + half.y < box.z0 ||
			         centre.y - half.y > box.z1)
			{
				continue;
			}
			auto& receiver = _shadowReceivers[instance.index];
			receiver.meshId = instance.meshId;
			receiver.morphWithTerrain = instance.morphWithTerrain;
			receiver.shadows.push_back(shadow);
		}
	}
	// OPENBLACK_SHADOW_TRACE=1: the receivers of this frame, once a second
	static const bool k_Trace = std::getenv("OPENBLACK_SHADOW_TRACE") != nullptr;
	if (k_Trace && ++RendererShadowsDebugHooksData().receiversTraceFrame % 60 == 0)
	{
		for (const auto& [instance, receiver] : _shadowReceivers)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "shadow receiver: instance {} mesh {} shadows {}", instance,
			                   receiver.meshId, receiver.shadows.size());
		}
	}
}

void Renderer::DrawShadowsOnObject(RenderPass viewId, uint32_t instance, const glm::mat4* matrices, uint8_t matrixCount) const
{
	const auto found = _shadowReceivers.find(instance);
	if (found == _shadowReceivers.end())
	{
		return;
	}
	const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto& receiver = found->second;
	const auto mesh = meshes.Handle(receiver.meshId);
	L3DMeshSubmitDesc submitDesc = {};
	submitDesc.viewId = viewId;
	// the shadow material through the current mode table, mode 6 (SRCALPHA / INVSRCALPHA, no Z write); the current
	// table is the normal one again by then, also for a fading object (its Draw puts it back after the global alpha
	// one). Each primitive is drawn with no state of its own, at once. The shadow material's culling: one-sided,
	// CULLMODE CCW, two-sided primitives too.
	// The Z test over the object as it was just drawn: the static Draw sets ZFUNC EQUAL before each shadow and
	// LESSEQUAL after the loop, and so does one more object Draw; the animated one sets nothing, so the frame's
	// LESSEQUAL holds: LessEqualInclusive (GEQUAL in openblack's reversed depth), which lets the redraw's equal depth
	// pass (as DrawLandShadows). The morphable Draw sets nothing either around its loop: LESSEQUAL too. (inferred) that
	// a boned mesh is one of the animated class
	const bool lessEqual = mesh->IsBoned() || receiver.morphWithTerrain;
	submitDesc.mode = render_modes::Mode::AlphaTexturedAlphaNoZWrite;
	submitDesc.options = {.zFunc = lessEqual ? render_modes::ZFunc::LessEqualInclusive : render_modes::ZFunc::Equal,
	                      .cull = render_modes::Cull::Ccw,
	                      .msaa = true};
	submitDesc.morphWithTerrain = receiver.morphWithTerrain;
	submitDesc.program = land_morph::ObjectProgram(*_shaderManager, receiver.morphWithTerrain, land_morph::ObjectPass::Shadow);
	static const auto k_Identity = glm::mat4(1.0f);
	submitDesc.modelMatrices = matrices != nullptr ? matrices : &k_Identity;
	submitDesc.matrixCount = matrices != nullptr ? matrixCount : 1;
	for (const auto* shadow : receiver.shadows)
	{
		submitDesc.dynamicShadow = fromBgfx(shadow->texture.Get());
		submitDesc.dynamicShadowBox = BoxUniform(shadow->box);
		submitDesc.dynamicShadowCull = CullUniform(*shadow);
		submitDesc.instanceDesc = std::make_unique<graphics::InstanceDesc>(renderCtx.instanceUniformBuffer, instance, 1);
		DrawMesh(*mesh, submitDesc, std::numeric_limits<uint8_t>::max());
	}
	static const bool k_Trace = std::getenv("OPENBLACK_SHADOW_TRACE") != nullptr;
	if (k_Trace && ++RendererShadowsDebugHooksData().onObjectTraceCount % 60 == 0)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "shadow on object: instance {} mesh {} view {} matrices {}", instance,
		                   receiver.meshId, static_cast<int>(viewId), matrixCount);
	}
	_shadowReceivers.erase(found);
}

void Renderer::DrawShadowsOnCutObjects(RenderPass viewId, const std::unordered_set<uint32_t>& cut) const
{
	if (_shadowReceivers.empty() || cut.empty())
	{
		return;
	}
	const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto poses = ecs::PosesByInstance(renderCtx);
	// (inferred) after all of them instead of right after each one: they are opaque and drawn with the Z test of the
	// receivers, so nothing drawn in between can take their shadow. Only the instances DrawCutAboveWater drew
	std::vector<uint32_t> drawn;
	for (const auto& [instance, receiver] : _shadowReceivers)
	{
		if (cut.contains(instance))
		{
			drawn.push_back(instance);
		}
	}
	for (const auto instance : drawn)
	{
		const auto mesh = meshes.Handle(_shadowReceivers.at(instance).meshId);
		const glm::mat4* matrices = nullptr;
		uint8_t count = 0;
		if (mesh->IsBoned())
		{
			matrices = mesh->GetBoneMatrices().data();
			count = static_cast<uint8_t>(std::min<size_t>(255, mesh->GetBoneMatrices().size()));
			ecs::UsePose(poses, instance, *mesh, matrices, count);
		}
		DrawShadowsOnObject(viewId, instance, matrices, count);
	}
}
