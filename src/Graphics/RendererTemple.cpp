/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The temple's insides. While the player is inside the temple every pass draws it in place of the world: its rooms,
// lit by their lightmaps and the temple's light, the main room mirrored in its floor, the pool, the map and its markers,
// the player's hand, the beams of light, the rooms' writing and the lights' glows. Outside the temple nothing here is
// called but InTemple and the map pass's first test.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstring>

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include <bgfx/bgfx.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandMorph.h"
#include "3D/OceanInterface.h"
#include "3D/OrientedText.h"
#include "3D/SkyInterface.h"
#include "3D/TempleDoors.h"
#include "3D/TempleInteriorInterface.h"
#include "3D/TempleLight.h"
#include "3D/TempleMap.h"
#include "Camera/Camera.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/LightBeam.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MistDome.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "ECS/Systems/TempleDrawPlan.h"
#include "EngineConfig.h"
#include "Graphics/DetailLevel.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/InstanceDesc.h"
#include "Graphics/LightBeams.h"
#include "Graphics/Mists.h"
#include "Graphics/ModelLight.h"
#include "Graphics/RenderModes.h"
#include "Graphics/SeaPass.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/Texture2D.h"
#include "Graphics/VertexBuffer.h"
#include "Graphics/WorldTriangles.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;
using namespace openblack::ecs::systems;

namespace
{
/// How far back, as a fraction of their depth, the temple's rooms the player isn't in are drawn: a few millimetres at the
/// doorways, where the rooms' copies of the arches meet, and the player's room's is seen
constexpr float k_OtherTempleRoomDepthBias = 5e-5f;

/// The glows are the atmosphere texture's cell 22 of 8 by 8
constexpr glm::vec2 k_GlowUvMin {6.0f / 8.0f, 2.0f / 8.0f};
constexpr glm::vec2 k_GlowUvExtent {1.0f / 8.0f, 1.0f / 8.0f};
constexpr entt::id_type k_Atmos = entt::hashed_string("raw/ATMOS").value();
constexpr entt::id_type k_AtmosAlpha = entt::hashed_string("raw/ATMOSA").value();

/// Added by alpha, without writing depth
constexpr uint64_t k_AdditiveState = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA |
                                     BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE);

/// The bgfx cull of a pass's cull of a face
uint64_t CullState(render_modes::Cull cull)
{
	switch (cull)
	{
	case render_modes::Cull::Ccw:
		return BGFX_STATE_CULL_CCW;
	case render_modes::Cull::Cw:
		return BGFX_STATE_CULL_CW;
	case render_modes::Cull::None:
		break;
	}
	return 0;
}

bgfx::VertexLayout BeamLayout()
{
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .end();
	return layout;
}

bgfx::VertexLayout OrientedTextLayout()
{
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .end();
	return layout;
}

/// Triangles laid out in the temple, as its text and map are, into a transient buffer of the frame; false when the
/// frame has no room left for them
bool AllocOrientedText(std::span<const OrientedTextVertex> vertices, bgfx::TransientVertexBuffer& buffer)
{
	static_assert(sizeof(OrientedTextVertex) == (3 + 2 + 1) * sizeof(float));
	const auto layout = OrientedTextLayout();
	const auto count = static_cast<uint32_t>(vertices.size());
	if (count == 0 || bgfx::getAvailTransientVertexBuffer(count, layout) < count)
	{
		return false;
	}
	bgfx::allocTransientVertexBuffer(&buffer, count, layout);
	std::memcpy(buffer.data, vertices.data(), count * sizeof(OrientedTextVertex));
	return true;
}

/// The rotation that turns a quad to face a pass's camera, which undoes the camera's own
glm::mat3 FacingCamera(const Camera& camera)
{
	return glm::mat3(glm::inverse(camera.GetViewMatrix(Camera::Interpolation::Current)));
}
} // namespace

bool Renderer::InTemple()
{
	return Locator::temple::has_value() && Locator::temple::value().Active();
}

void Renderer::SubmitBeamMesh(RenderPass viewId, const ShaderProgram* shader, const BeamMesh& mesh, const glm::mat4& model,
                              const TextureHandle& texture, std::optional<TextureHandle> alpha, uint64_t state) const
{
	static_assert(sizeof(BeamVertex) == 6 * sizeof(float));
	const auto layout = BeamLayout();
	const auto vertexCount = static_cast<uint32_t>(mesh.vertices.size());
	const auto indexCount = static_cast<uint32_t>(mesh.indices.size());
	if (vertexCount == 0 || indexCount == 0 || bgfx::getAvailTransientVertexBuffer(vertexCount, layout) < vertexCount ||
	    bgfx::getAvailTransientIndexBuffer(indexCount) < indexCount)
	{
		return;
	}
	bgfx::TransientVertexBuffer vertices;
	bgfx::TransientIndexBuffer indices;
	bgfx::allocTransientVertexBuffer(&vertices, vertexCount, layout);
	bgfx::allocTransientIndexBuffer(&indices, indexCount);
	std::memcpy(vertices.data, mesh.vertices.data(), vertexCount * sizeof(BeamVertex));
	std::memcpy(indices.data, mesh.indices.data(), indexCount * sizeof(uint16_t));

	const glm::vec4 u_beamParams {alpha.has_value() ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f};
	shader->SetTextureSampler("s_diffuse", 0, texture);
	shader->SetTextureSampler("s_alpha", 1, alpha.value_or(texture));
	shader->SetUniformValue("u_beamParams", &u_beamParams);
	bgfx::setTransform(glm::value_ptr(model));
	bgfx::setVertexBuffer(0, &vertices);
	bgfx::setIndexBuffer(&indices);
	bgfx::setState(state);
	bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(shader->GetRawHandle()));
}

void Renderer::DrawTempleMesh(const L3DMesh& mesh, const L3DMeshSubmitDesc& desc, const TempleLook& look,
                              const TemplePrograms& programs) const
{
	const auto& subMeshes = mesh.GetSubMeshes();
	for (uint32_t index = 0; index < subMeshes.size(); ++index)
	{
		if (std::ranges::find(look.hiddenSubMeshes, index) != look.hiddenSubMeshes.end())
		{
			continue;
		}
		const auto texture = std::ranges::find(look.subMeshTextures, index, &std::pair<uint32_t, TextureHandle>::first);
		const auto glow = std::ranges::find(look.subMeshGlows, index, &std::pair<uint32_t, glm::vec3>::first);
		DrawTempleSubMesh(mesh, *subMeshes[index], desc, look, programs,
		                  texture != look.subMeshTextures.end() ? &texture->second : nullptr,
		                  glow != look.subMeshGlows.end() ? glow->second : glm::vec3(0.0f));
	}
}

void Renderer::DrawTempleSubMesh(const L3DMesh& mesh, const L3DSubMesh& subMesh, const L3DMeshSubmitDesc& desc,
                                 const TempleLook& look, const TemplePrograms& programs, const TextureHandle* subMeshTexture,
                                 glm::vec3 glow) const
{
	// As the world's models: no physics meshes, statuses or lower levels of detail
	const auto& flags = subMesh.GetFlags();
	if (subMesh.IsPhysics() || flags.status != 0 || ((flags.lodMask & 1) != 1 && !flags.isWindow))
	{
		return;
	}
	// The main room's doors seen from the other rooms are its submeshes with joints, and the side rooms' own copies of
	// them are drawn only while they turn
	const auto& joint = subMesh.GetJoint();
	if (look.onlyJoints && !joint.has_value())
	{
		return;
	}
	if (look.hideShutJoints && joint.has_value() &&
	    (joint->index >= desc.joints.size() || desc.joints[joint->index] == glm::mat4(1.0f)))
	{
		return;
	}
	if (!subMesh.GetMesh().GetVertexBuffer().IsValid() ||
	    (subMesh.GetMesh().IsIndexed() && !subMesh.GetMesh().GetIndexBuffer().IsValid()))
	{
		return;
	}

	// The game draws the submeshes with a lightmap through it and the others as they are
	const auto lightmapSkin = subMesh.GetLightmapSkinID();
	const Texture2D* lightmap = lightmapSkin.has_value() ? world_triangles::PrimitiveTexture(mesh, *lightmapSkin) : nullptr;
	const bool lightmapped = desc.lightmapProgram != nullptr && lightmap != nullptr;
	const auto* program = lightmapped ? desc.lightmapProgram : desc.program;
	// The floor over the main room's reflection and the pool's reflection take no alpha test; the reflection alone
	// takes nothing but the reflection
	const bool reflectionOnly = program == programs.reflection;
	const bool overReflection = reflectionOnly || program == programs.reflective;

	// A submesh with a joint turns about its pivot by its matrix of the table, before the mesh's own matrix
	glm::mat4 model = desc.modelMatrices != nullptr ? *desc.modelMatrices : glm::mat4(1.0f);
	if (joint.has_value() && joint->index < desc.joints.size())
	{
		model = temple_draw::JointModel(model, joint->pivot, desc.joints[joint->index]);
	}

	const auto pass = sea_pass::ForPass(desc.viewId);
	const glm::vec4 u_depthBias {desc.depthBias, 0.0f, 0.0f, 0.0f};
	const glm::vec4 u_uvOffset {look.uvOffset, 0.0f, 0.0f};
	const glm::vec4 u_templeLight {desc.lightMultiply, 1.0f};
	// A control's glow takes the place of the light's colour added
	const glm::vec4 u_glow {glow != glm::vec3(0.0f) ? glow : desc.lightAdd, 0.0f};
	const glm::vec4 u_templeDraw {look.unshaded ? 1.0f : 0.0f, look.opacity, 0.0f, 0.0f};
	const auto u_modelLight = model_light::Uniform();
	for (const auto& prim : subMesh.GetPrimitives())
	{
		const Texture2D* texture = world_triangles::PrimitiveTexture(mesh, prim.skinID);
		bgfx::setTransform(glm::value_ptr(model));
		if (!reflectionOnly)
		{
			// A primitive without a skin is drawn in its material's colour
			if (subMeshTexture != nullptr)
			{
				program->SetTextureSampler("s_diffuse", 0, *subMeshTexture);
			}
			else
			{
				program->SetTextureSampler("s_diffuse", 0, texture != nullptr ? *texture : RevolvedSurfaceWhiteTexture());
			}
			const glm::vec4 u_materialColour {glm::vec3(prim.colour),
			                                  texture == nullptr && subMeshTexture == nullptr ? 1.0f : 0.0f};
			program->SetUniformValue("u_materialColour", &u_materialColour);
			program->SetUniformValue("u_templeLight", &u_templeLight);
			program->SetUniformValue("u_glow", &u_glow);
		}
		if (lightmapped)
		{
			program->SetTextureSampler("s_lightmap", 3, *lightmap);
		}
		else
		{
			program->SetUniformValue("u_modelLight", &u_modelLight);
		}
		if (overReflection && look.reflection != nullptr)
		{
			program->SetTextureSampler("s_reflection", 4, *look.reflection);
		}
		program->SetUniformValue("u_depthBias", &u_depthBias);
		program->SetUniformValue("u_uvOffset", &u_uvOffset);

		// Each primitive as its material says: its mode's blending, depth and alpha test, culled unless two-sided
		const auto drawn =
		    render_modes::Select(static_cast<render_modes::Mode>(prim.materialType), render_modes::Table::Normal);
		if (!overReflection)
		{
			const auto alpha = render_modes::PrimitiveAlpha(
			    drawn, render_modes::Table::Normal, static_cast<uint8_t>(std::lround(prim.alphaCutoutThreshold * 255.0f)));
			const glm::vec4 u_skyAlphaThreshold {0.0f, alpha.ref, 0.0f, static_cast<float>(alpha.source)};
			program->SetUniformValue("u_skyAlphaThreshold", &u_skyAlphaThreshold);
			program->SetUniformValue("u_templeDraw", &u_templeDraw);
		}
		const auto cull = pass.FaceCull(sea_pass::Surface::Model, prim.twoSided, false);
		const bool blended = prim.blend != L3DSubMesh::Primitive::BlendMode::Disabled && !prim.thresholdAlpha;
		const uint64_t state =
		    look.state.has_value()
		        ? *look.state | CullState(cull)
		        : render_modes::PrimitiveState(drawn, {.cull = cull, .writeAlpha = true, .msaa = true}, blended);

		if (look.instances != nullptr)
		{
			bgfx::setInstanceDataBuffer(look.instances);
		}
		else if (desc.instanceDesc != nullptr)
		{
			bgfx::setInstanceDataBuffer(toBgfx(desc.instanceDesc->GetRawHandle()), desc.instanceDesc->GetStart(),
			                            desc.instanceDesc->GetCount());
		}
		if (subMesh.GetMesh().IsIndexed())
		{
			subMesh.GetMesh().GetIndexBuffer().Bind(prim.indicesCount, prim.indicesOffset);
		}
		subMesh.GetMesh().GetVertexBuffer().Bind();
		bgfx::setState(state);
		bgfx::submit(static_cast<bgfx::ViewId>(desc.viewId), toBgfx(program->GetRawHandle()), 0, BGFX_DISCARD_ALL);
	}
}

void Renderer::DrawTempleReflection(const DrawSceneDesc& drawDesc) const
{
	// The main room is mirrored for its floor to show it through
	const auto& descs = Locator::rendereringSystem::value().GetContext().instancedDrawDescs;
	if (!drawDesc.drawEntities || std::ranges::none_of(descs, [](const auto& entry) { return entry.second.showsReflection; }))
	{
		return;
	}
	UpdateReflectionTarget();
	DrawSceneDesc drawPassDesc = drawDesc;
	const auto& frameBuffer = Locator::oceanSystem::value().GetReflectionFramebuffer();
	// Mirrored through the plane of the temple's origin, the sea's level, where the temple stands
	auto reflectionCamera = drawDesc.camera->Reflect();
	drawPassDesc.viewId = RenderPass::Reflection;
	drawPassDesc.camera = reflectionCamera.get();
	drawPassDesc.frameBuffer = &frameBuffer;
	drawPassDesc.drawWater = false;
	drawPassDesc.drawBoundingBoxes = false;
	DrawTemplePass(drawPassDesc);
}

void Renderer::DrawTemplePass(const DrawSceneDesc& desc) const
{
	const auto& meshManager = Locator::resources::value().GetMeshes();
	const auto view = static_cast<bgfx::ViewId>(desc.viewId);
	const bool reflection = desc.viewId == RenderPass::Reflection;

	if (desc.frameBuffer != nullptr)
	{
		desc.frameBuffer->Bind(desc.viewId);
	}
	bgfx::touch(view);
	_shaderManager->SetCamera(desc.viewId, *desc.camera);
	// The hand's blended primitives go to the blended view after the main one, as they do outside
	if (desc.viewId == RenderPass::Main)
	{
		_shaderManager->SetCamera(RenderPass::MainBlended, *desc.camera);
	}
	const TemplePrograms programs {
	    .temple = _shaderManager->GetShader("ObjectTempleInstanced"),
	    .lightmap = _shaderManager->GetShader("ObjectLightmapInstanced"),
	    .reflective = _shaderManager->GetShader("ObjectReflectiveLightmapInstanced"),
	    .reflection = _shaderManager->GetShader("Reflection"),
	    .textured = _shaderManager->GetShader("Textured3D"),
	    .beam = _shaderManager->GetShader("Beam"),
	};

	// The sky behind the temple, which shows through the cracks between its rooms, as the world's pass draws it but
	// without the sun: inside the temple the original draws the dome and the moon only
	if (desc.drawSky)
	{
		const auto* skyShader = _shaderManager->GetShader("Sky");
		const auto modelMatrix = glm::mat4(1.0f);
		const glm::vec4 u_typeAlignment = {0.0f, _skyAlignment.Get() + 1.0f, 0.0f, 0.0f};
		skyShader->SetTextureSampler("s_diffuse", 0, Locator::skySystem::value().GetTexture());
		skyShader->SetUniformValue("u_typeAlignment", &u_typeAlignment);
		L3DMeshSubmitDesc submitDesc = {};
		submitDesc.viewId = desc.viewId;
		submitDesc.program = skyShader;
		submitDesc.options = {.cull = sea_pass::ForPass(desc.viewId).FaceCull(sea_pass::Surface::Sky, false, false),
		                      .writeAlpha = true,
		                      .msaa = true};
		submitDesc.modelMatrices = &modelMatrix;
		submitDesc.matrixCount = 1;
		submitDesc.isSky = true;
		DrawMesh(Locator::skySystem::value().GetMesh(), submitDesc, 0);
		DrawMoon(desc.viewId, *desc.camera);
	}

	if (desc.drawEntities)
	{
		const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
		const auto& temple = Locator::temple::value();
		const auto& light = temple.GetLight();
		const auto* reflectionTarget = &Locator::oceanSystem::value().GetReflectionFramebuffer().GetColorAttachment();
		const auto drawInstances = [&](entt::id_type meshId, const RenderContext::InstancedDrawDesc& placers, uint32_t first,
		                               uint32_t count) {
			if (!meshManager.Contains(meshId))
			{
				return;
			}
			const auto mesh = meshManager.Handle(meshId);
			L3DMeshSubmitDesc submitDesc = {};
			submitDesc.viewId = desc.viewId;
			submitDesc.program = programs.temple;
			submitDesc.instanceDesc = std::make_unique<InstanceDesc>(renderCtx.instanceUniformBuffer, first, count);
			const auto identity = glm::mat4(1.0f);
			submitDesc.modelMatrices = &identity;
			submitDesc.matrixCount = 1;
			// The game gives the rooms the temple's light
			submitDesc.lightMultiply = light.multiply;
			submitDesc.lightAdd = light.add;
			// The rooms meet at their doorways, whose arches each room has a copy of: the player's room's is seen
			submitDesc.depthBias = placers.behindCurrentRoom ? k_OtherTempleRoomDepthBias : 0.0f;
			submitDesc.joints = temple.GetDoors().GetJoints();
			// Only the temple's meshes have lightmaps; the main room's floor shows the room's reflection through it
			submitDesc.lightmapProgram = placers.showsReflection ? programs.reflective : programs.lightmap;
			const TempleLook look {
			    .uvOffset = placers.uvOffset,
			    .subMeshTextures = placers.subMeshTextures,
			    .subMeshGlows = placers.subMeshGlows,
			    .hiddenSubMeshes = placers.hiddenSubMeshes,
			    .onlyJoints = placers.onlyJoints,
			    .hideShutJoints = placers.hideShutJoints,
			    .reflection = placers.showsReflection ? reflectionTarget : nullptr,
			};
			DrawTempleMesh(*mesh, submitDesc, look, programs);
		};
		const auto drawn = [reflection](entt::id_type meshId, const RenderContext::InstancedDrawDesc& placers) {
			return meshId != ecs::components::Hand::k_MeshId && !(reflection && placers.hiddenFromReflection);
		};

		// The rooms' solid meshes, then those of nothing but blended primitives over them, one instance at a time
		for (const auto& [meshId, placers] : renderCtx.instancedDrawDescs)
		{
			if (drawn(meshId, placers) && !placers.translucent)
			{
				drawInstances(meshId, placers, placers.offset, placers.count);
			}
		}
		DrawTempleUnderside(desc, programs);
		for (const auto& [meshId, placers] : renderCtx.instancedDrawDescs)
		{
			if (drawn(meshId, placers) && placers.translucent)
			{
				for (uint32_t instance = placers.offset; instance < placers.offset + placers.count; ++instance)
				{
					drawInstances(meshId, placers, instance, 1);
				}
			}
		}
		// The main room's pool, the map over it and its markers, ahead of the hand, whose faded wrist they show through
		DrawTemplePool(desc, programs);
		DrawTempleMap(desc, programs);
		DrawTempleMapMarkers(desc, programs);
		DrawTempleCaveTrophies(desc, programs);

		// The player's hand, drawn as it is drawn on the land; the game's reflection of the main room has no hand in it
		if (const auto hand = renderCtx.instancedDrawDescs.find(ecs::components::Hand::k_MeshId);
		    hand != renderCtx.instancedDrawDescs.end() && !(reflection && hand->second.hiddenFromReflection) &&
		    meshManager.Contains(ecs::components::Hand::k_MeshId))
		{
			const auto mesh = meshManager.Handle(ecs::components::Hand::k_MeshId);
			L3DMeshSubmitDesc submitDesc = {};
			submitDesc.viewId = desc.viewId;
			submitDesc.program = land_morph::ObjectProgram(*_shaderManager, false);
			submitDesc.options = render_modes::k_ModelPass;
			submitDesc.instanceDesc =
			    std::make_unique<InstanceDesc>(renderCtx.instanceUniformBuffer, hand->second.offset, hand->second.count);
			const auto* bones = Locator::handSystem::has_value() ? Locator::handSystem::value().GetBoneMatrices() : nullptr;
			const auto identity = glm::mat4(1.0f);
			if (mesh->IsBoned() && bones != nullptr && bones->size() == mesh->GetBoneMatrices().size())
			{
				submitDesc.modelMatrices = bones->data();
				submitDesc.matrixCount = static_cast<uint8_t>(bones->size());
			}
			else if (mesh->IsBoned())
			{
				submitDesc.modelMatrices = mesh->GetBoneMatrices().data();
				submitDesc.matrixCount = static_cast<uint8_t>(mesh->GetBoneMatrices().size());
			}
			else
			{
				submitDesc.modelMatrices = &identity;
				submitDesc.matrixCount = 1;
			}
			submitDesc.isSky = false;
			submitDesc.lightBoost = 1.5f;
			submitDesc.noHaze = true;
			DrawMesh(*mesh, submitDesc, std::numeric_limits<uint8_t>::max());
		}

		DrawTempleLightBeams(desc, programs);
		DrawTempleMists(desc);
		DrawTempleText(desc);

		// Debug
		if (desc.viewId == RenderPass::Main && renderCtx.boundingBox)
		{
			const auto boundBoxOffset = static_cast<uint32_t>(renderCtx.instanceUniforms.size() / 2);
			const auto boundBoxCount = static_cast<uint32_t>(renderCtx.instanceUniforms.size() / 2);
			renderCtx.boundingBox->GetVertexBuffer().Bind();
			bgfx::setInstanceDataBuffer(toBgfx(renderCtx.instanceUniformBuffer), boundBoxOffset, boundBoxCount);
			bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_CULL_CW |
			               BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA | BGFX_STATE_PT_LINES);
			bgfx::submit(view, toBgfx(_shaderManager->GetShader("DebugLineInstanced")->GetRawHandle()));
		}
	}

	if (desc.drawSprites)
	{
		DrawTempleGlows(desc, programs);
	}

	// Enable stats or debug text.
	auto debugMode = BGFX_DEBUG_NONE;
	if (_bgfxDebug)
	{
		debugMode |= BGFX_DEBUG_STATS;
	}
	if (desc.wireframe)
	{
		debugMode |= BGFX_DEBUG_WIREFRAME;
	}
	if (_bgfxProfile)
	{
		debugMode |= BGFX_DEBUG_PROFILER;
	}
	bgfx::setDebug(debugMode);
}

void Renderer::DrawTempleUnderside(const DrawSceneDesc& desc, const TemplePrograms& programs) const
{
	// The rooms' meshes don't quite meet everywhere: the sills of the main room's doorways stand a hundredth of a unit off
	// their doors' frames, and their edges have vertices of others part way along them. The game draws the sky behind
	// the temple, so it shows through the cracks there too, as specks of the outside. Looking down through them, this
	// shows black instead. The creature's room goes deepest, to 70.5 below the temple.
	constexpr float k_Depth = -75.0f;
	constexpr float k_Extent = 10000.0f;
	if (desc.viewId != RenderPass::Main)
	{
		return;
	}
	constexpr uint32_t k_Black = 0xFF000000;
	const std::array<OrientedTextVertex, 6> vertices {{
	    {{-k_Extent, k_Depth, -k_Extent}, {0.0f, 0.0f}, k_Black},
	    {{k_Extent, k_Depth, -k_Extent}, {1.0f, 0.0f}, k_Black},
	    {{k_Extent, k_Depth, k_Extent}, {1.0f, 1.0f}, k_Black},
	    {{-k_Extent, k_Depth, -k_Extent}, {0.0f, 0.0f}, k_Black},
	    {{k_Extent, k_Depth, k_Extent}, {1.0f, 1.0f}, k_Black},
	    {{-k_Extent, k_Depth, k_Extent}, {0.0f, 1.0f}, k_Black},
	}};
	bgfx::TransientVertexBuffer buffer;
	if (!AllocOrientedText(vertices, buffer))
	{
		return;
	}
	const auto* shader = programs.textured;
	const auto model = glm::translate(Locator::temple::value().GetPosition());
	bgfx::setTransform(glm::value_ptr(model));
	bgfx::setVertexBuffer(0, &buffer);
	shader->SetTextureSampler("s_texture", 0, RevolvedSurfaceWhiteTexture());
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_GREATER |
	               BGFX_STATE_MSAA);
	bgfx::submit(static_cast<bgfx::ViewId>(desc.viewId), toBgfx(shader->GetRawHandle()));
}

void Renderer::DrawTemplePool(const DrawSceneDesc& desc, const TemplePrograms& programs) const
{
	const auto& temple = Locator::temple::value();
	constexpr entt::id_type k_Pool = entt::hashed_string("temple/interior/mainwater_l3d").value();
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (desc.viewId != RenderPass::Main || !temple.IsRoomDrawn(TempleRoom::Main) || !meshes.Contains(k_Pool))
	{
		return;
	}
	const auto& mesh = *meshes.Handle(k_Pool);
	// The water's alpha swells and ebbs with the sine of the time, and the second layer's is what the first's lacks
	const float time = temple.GetPoolTime();
	const auto alpha = static_cast<int32_t>((std::sin(time) * 32.0f) + 128.0f);
	// The layers take 13 sixteenths of the temple's light, and of its colour added
	constexpr float k_Light = 13.0f / 16.0f;
	const auto& light = temple.GetLight();
	struct Layer
	{
		glm::vec3 position;
		float yaw;
		int32_t alpha;
		glm::vec2 uvOffset;
	};
	const std::array layers = {
	    Layer {glm::vec3(0.0f), 0.0f, alpha, glm::vec2(-0.01f, 0.007f) * time},
	    Layer {glm::vec3(0.0f, 0.05f, 0.0f), glm::quarter_pi<float>(), 255 - alpha, glm::vec2(0.01f, 0.005f) * time},
	};
	// Each draw is of one instance the renderer places itself
	const auto instanceAt = [](const glm::mat4& model, bgfx::InstanceDataBuffer& buffer) {
		constexpr uint16_t k_Stride = sizeof(glm::mat4);
		if (bgfx::getAvailInstanceDataBuffer(1, k_Stride) < 1)
		{
			return false;
		}
		bgfx::allocInstanceDataBuffer(&buffer, 1, k_Stride);
		std::memcpy(buffer.data, glm::value_ptr(model), sizeof(glm::mat4));
		return true;
	};
	const auto identity = glm::mat4(1.0f);

	// Under the water the room's reflection shows, where the floor leaves it uncovered: beneath the first layer, at its
	// height, so it leaves the depth to the layers
	{
		bgfx::InstanceDataBuffer instances;
		if (instanceAt(glm::translate(temple.GetPosition()), instances))
		{
			L3DMeshSubmitDesc submitDesc = {};
			submitDesc.viewId = desc.viewId;
			submitDesc.program = programs.reflection;
			submitDesc.modelMatrices = &identity;
			submitDesc.matrixCount = 1;
			const TempleLook look {
			    .reflection = &Locator::oceanSystem::value().GetReflectionFramebuffer().GetColorAttachment(),
			    .instances = &instances,
			    .state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA,
			};
			DrawTempleMesh(mesh, submitDesc, look, programs);
		}
	}
	for (const auto& layer : layers)
	{
		bgfx::InstanceDataBuffer instances;
		const auto model =
		    glm::translate(temple.GetPosition() + layer.position) * glm::rotate(layer.yaw, glm::vec3(0.0f, 1.0f, 0.0f));
		if (!instanceAt(model, instances))
		{
			continue;
		}
		// In their colour alone, without the pool's lightmap: through it, the water is far darker than the game's. Each
		// primitive blended, culled and writing depth as its material says, as the room's meshes are.
		L3DMeshSubmitDesc submitDesc = {};
		submitDesc.viewId = desc.viewId;
		submitDesc.program = programs.temple;
		submitDesc.modelMatrices = &identity;
		submitDesc.matrixCount = 1;
		submitDesc.lightMultiply = light.multiply * k_Light;
		submitDesc.lightAdd = light.add * k_Light;
		const TempleLook look {
		    .uvOffset = layer.uvOffset,
		    .unshaded = true,
		    .opacity = static_cast<float>(layer.alpha) / 255.0f,
		    .instances = &instances,
		};
		DrawTempleMesh(mesh, submitDesc, look, programs);
	}
}

void Renderer::DrawTempleMapPass(const DrawSceneDesc& drawDesc) const
{
	_templeMapFresh = false;
	if (!InTemple() || !Locator::terrainSystem::has_value())
	{
		return;
	}
	// The view keeps its target from one frame to the next, so it is only drawn again for a new visit
	const auto visit = Locator::temple::value().GetVisits();
	if (_templeMapVisit == visit)
	{
		return;
	}
	_templeMapVisit = visit;
	_templeMapFresh = true;

	// 8 texels a block, as the game's pages of the land's textures give the map, over the 32 by 32 blocks there can be
	constexpr uint16_t k_Size = 256;
	const auto viewId = static_cast<bgfx::ViewId>(RenderPass::TempleMap);
	if (!_templeMapFrameBuffer)
	{
		_templeMapFrameBuffer = std::make_unique<FrameBuffer>("Temple Map", k_Size, k_Size, TextureFormat::RGBA8);
	}
	_templeMapFrameBuffer->Bind(RenderPass::TempleMap);
	bgfx::setViewRect(viewId, 0, 0, k_Size, k_Size);
	bgfx::setViewClear(viewId, BGFX_CLEAR_COLOR, 0x00000000);
	bgfx::setViewMode(viewId, bgfx::ViewMode::Sequential);

	// From above, x across the texture and z down it, as the map's texture coordinates run
	constexpr float k_Half = TempleMap::k_TextureSpan * 0.5f;
	const auto view = glm::translate(glm::vec3(-k_Half, 0.0f, -k_Half));
	const float down = bgfx::getCaps()->originBottomLeft ? 1.0f : -1.0f;
	auto projection = glm::mat4(0.0f);
	projection[0][0] = 1.0f / k_Half;
	projection[2][1] = down / k_Half;
	projection[3][2] = 0.5f;
	projection[3][3] = 1.0f;
	bgfx::setViewTransform(viewId, glm::value_ptr(view), glm::value_ptr(projection));
	bgfx::touch(viewId);

	// The land's own textures and light, as the world's pass draws it, without the haze or the small bump
	const auto& island = Locator::terrainSystem::value();
	const auto* terrainShader = _shaderManager->GetShader("Terrain");
	const auto islandExtent = glm::vec4(island.GetExtent().minimum, island.GetExtent().maximum);
	const glm::vec4 u_skyAndBump = {0.0f, drawDesc.bumpMapStrength, 0.0f, 0.0f};
	const glm::vec4 u_smallBumpLine = {0.0f, 0.0f, 0.0f, 0.0f};
	const glm::vec4 noHaze {0.0f};
	terrainShader->SetTextureSampler("s0_materials", 0, island.GetAlbedoArray());
	terrainShader->SetTextureSampler("s1_bump", 1, island.GetBump());
	terrainShader->SetTextureSampler("s2_smallBump", 2, island.GetSmallBump());
	terrainShader->SetTextureSampler("s3_footprints", 3, island.GetFootprintFramebuffer().GetColorAttachment());
	terrainShader->SetTextureSampler("s_landLightTable", 4, fromBgfx(_landLightTexture.Get()));
	const auto cellMapSize = glm::vec2(island.GetCellMap().GetResolution());
	if (_landCellsTexture.IsValid() && glm::vec2(_landCellsSize) == cellMapSize)
	{
		terrainShader->SetTextureSampler("s_landCells", 6, fromBgfx(_landCellsTexture.Get()));
	}
	else
	{
		terrainShader->SetTextureSampler("s_landCells", 6, island.GetCellMap());
	}
	const glm::vec4 u_cellMap = {island.GetExtent().minimum, cellMapSize};
	terrainShader->SetUniformValue("u_cellMap", &u_cellMap);
	terrainShader->SetTextureSampler("s5_staticShadow", 5, island.GetStaticShadowFramebuffer().GetColorAttachment());
	terrainShader->SetTextureSampler("s8_landAlpha", 8, island.GetLandAlphaFramebuffer().GetColorAttachment());
	glm::vec4 u_blockTexture {0.0f};
	if (const auto* blockTexture = island.GetBlockTexture(); blockTexture != nullptr)
	{
		terrainShader->SetTextureSampler("s10_blockTexture", 10, *blockTexture);
		u_blockTexture.x = 1.0f;
		u_blockTexture.y = 1.0f;
	}
	else
	{
		terrainShader->SetTextureSampler("s10_blockTexture", 10, island.GetLandAlphaFramebuffer().GetColorAttachment());
	}
	terrainShader->SetUniformValue("u_blockTexture", &u_blockTexture);
	terrainShader->SetUniformValue("u_skyAndBump", &u_skyAndBump);
	terrainShader->SetUniformValue("u_smallBumpLine", &u_smallBumpLine);
	terrainShader->SetUniformValue("u_haze", &noHaze);
	terrainShader->SetUniformValue("u_hazeColour", &noHaze);
	const float staticShadowStrength = GetDetailLevel(Locator::config::value().detailLevel).useHighTexture ? 0.5f : 0.25f;
	const glm::vec4 u_terrainPass = {1.0f, 0.0f, staticShadowStrength, 0.0f};
	terrainShader->SetUniformValue("u_terrainPass", &u_terrainPass);
	terrainShader->SetUniformValue("u_islandExtent", &islandExtent);
	const glm::vec4 u_hazeBlock {0.0f};
	terrainShader->SetUniformValue("u_hazeBlock", &u_hazeBlock);
	constexpr auto k_Discard = BGFX_DISCARD_INSTANCE_DATA | BGFX_DISCARD_INDEX_BUFFER | BGFX_DISCARD_TRANSFORM |
	                           BGFX_DISCARD_VERTEX_STREAMS | BGFX_DISCARD_STATE;
	for (const auto& block : island.GetBlocks())
	{
		const glm::vec4 mapPositionAndSize = glm::vec4(block.GetMapPosition(), 160.0f, 160.0f);
		terrainShader->SetUniformValue("u_blockPositionAndSize", &mapPositionAndSize);
		block.GetMesh().GetVertexBuffer().Bind();
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
		// The textures stay bound from one block to the next
		bgfx::submit(viewId, toBgfx(terrainShader->GetRawHandle()), 0, k_Discard);
	}
	bgfx::discard(BGFX_DISCARD_BINDINGS);
}

void Renderer::DrawTempleMap(const DrawSceneDesc& desc, const TemplePrograms& programs) const
{
	if (desc.viewId != RenderPass::Main || !_templeMapFrameBuffer || _templeMapFresh)
	{
		return;
	}
	const auto& temple = Locator::temple::value();
	bgfx::TransientVertexBuffer buffer;
	if (!AllocOrientedText(temple.GetMap(), buffer))
	{
		return;
	}
	// The land's texture by the vertices' colours, blended by their alpha
	const auto* shader = programs.textured;
	const auto model = glm::translate(temple.GetPosition());
	bgfx::setTransform(glm::value_ptr(model));
	bgfx::setVertexBuffer(0, &buffer);
	shader->SetTextureSampler("s_texture", 0, _templeMapFrameBuffer->GetColorAttachment());
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_GREATER |
	               BGFX_STATE_MSAA | BGFX_STATE_BLEND_ALPHA);
	bgfx::submit(static_cast<bgfx::ViewId>(desc.viewId), toBgfx(shader->GetRawHandle()));
}

void Renderer::DrawTempleMapMarkers(const DrawSceneDesc& desc, const TemplePrograms& programs) const
{
	if (desc.viewId != RenderPass::Main)
	{
		return;
	}
	const auto& temple = Locator::temple::value();
	const auto& markers = temple.GetMapMarkers();
	if (markers.empty())
	{
		return;
	}
	const auto origin = temple.GetPosition();

	// First a glow under each marker, the room's light glow drawn over everything: 1.3 across, a tenth above the marker
	const auto& textures = Locator::resources::value().GetTextures();
	if (textures.Contains(k_Atmos) && textures.Contains(k_AtmosAlpha))
	{
		constexpr float k_GlowSize = 1.3f;
		constexpr glm::vec4 k_GlowColour {0x61 / 255.0f, 0x6E / 255.0f, 0x7C / 255.0f, 1.0f};
		const auto facing = FacingCamera(*desc.camera);
		BeamMesh glows;
		for (const auto& marker : markers)
		{
			AppendGlowQuad(glows, origin + marker.position + glm::vec3(0.0f, 0.1f, 0.0f), facing, glm::vec2(k_GlowSize),
			               k_GlowUvMin, k_GlowUvExtent, k_GlowColour);
		}
		SubmitBeamMesh(desc.viewId, programs.beam, glows, glm::mat4(1.0f), textures.Handle(k_Atmos)->GetNativeHandle(),
		               textures.Handle(k_AtmosAlpha)->GetNativeHandle(), k_AdditiveState);
	}

	// Then the markers, a twentieth of their size, turning, in their colours in the temple's light
	constexpr std::array<entt::id_type, 3> k_Icons = {
	    entt::hashed_string("temple/icons/I_citadel_on_map"),
	    entt::hashed_string("temple/icons/I_creature_on_map"),
	    entt::hashed_string("temple/icons/I_challenge_on_map"),
	};
	constexpr float k_MarkerScale = 0.05f;
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto turn = glm::rotate(temple.GetMapMarkerTurn(), glm::vec3(0.0f, 1.0f, 0.0f));
	const auto identity = glm::mat4(1.0f);
	for (const auto& marker : markers)
	{
		const auto icon = k_Icons.at(static_cast<size_t>(marker.kind));
		if (!meshes.Contains(icon))
		{
			continue;
		}
		const auto model = glm::translate(origin + marker.position) * turn * glm::scale(glm::vec3(k_MarkerScale));
		bgfx::InstanceDataBuffer instances;
		constexpr uint16_t k_Stride = sizeof(glm::mat4);
		if (bgfx::getAvailInstanceDataBuffer(1, k_Stride) < 1)
		{
			return;
		}
		bgfx::allocInstanceDataBuffer(&instances, 1, k_Stride);
		std::memcpy(instances.data, glm::value_ptr(model), sizeof(glm::mat4));
		L3DMeshSubmitDesc submitDesc = {};
		submitDesc.viewId = desc.viewId;
		submitDesc.program = programs.temple;
		submitDesc.modelMatrices = &identity;
		submitDesc.matrixCount = 1;
		submitDesc.lightMultiply = temple.GetLight().Colour(glm::vec3(marker.colour) / 255.0f);
		submitDesc.lightAdd = temple.GetLight().add;
		const TempleLook look {
		    .unshaded = true,
		    .instances = &instances,
		    .state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_GREATER |
		             BGFX_STATE_MSAA,
		};
		DrawTempleMesh(*meshes.Handle(icon), submitDesc, look, programs);
	}
}

void Renderer::DrawTempleCaveTrophies(const DrawSceneDesc& desc, const TemplePrograms& programs) const
{
	if (desc.viewId != RenderPass::Main)
	{
		return;
	}
	const auto& temple = Locator::temple::value();
	const auto& trophies = temple.GetCaveTrophies();
	const auto& textures = Locator::resources::value().GetTextures();
	if (trophies.empty() || !textures.Contains(TempleCaveTrophy::k_Texture))
	{
		return;
	}
	// The game gives every material of the icons the one texture, with the alpha read from beside it. (pending) the
	// belts and the medals past wood add the game's first environment map, which the temple's programs have not got
	const auto icons = textures.Handle(TempleCaveTrophy::k_Texture)->GetNativeHandle();
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto identity = glm::mat4(1.0f);
	for (const auto& trophy : trophies)
	{
		if (!meshes.Contains(trophy.mesh))
		{
			continue;
		}
		const auto mesh = meshes.Handle(trophy.mesh);
		bgfx::InstanceDataBuffer instances;
		constexpr uint16_t k_Stride = sizeof(glm::mat4);
		if (bgfx::getAvailInstanceDataBuffer(1, k_Stride) < 1)
		{
			return;
		}
		bgfx::allocInstanceDataBuffer(&instances, 1, k_Stride);
		std::memcpy(instances.data, glm::value_ptr(trophy.model), sizeof(glm::mat4));
		std::vector<std::pair<uint32_t, TextureHandle>> skins;
		skins.reserve(mesh->GetSubMeshes().size());
		for (uint32_t subMesh = 0; subMesh < mesh->GetSubMeshes().size(); ++subMesh)
		{
			skins.emplace_back(subMesh, icons);
		}
		L3DMeshSubmitDesc submitDesc = {};
		submitDesc.viewId = desc.viewId;
		submitDesc.program = programs.temple;
		submitDesc.modelMatrices = &identity;
		submitDesc.matrixCount = 1;
		// The colour multiplies what lights them: the medals come out the mid grey of the game's at 0x80. The game puts
		// them in the temple's light.
		const auto colour = glm::vec3((trophy.colour >> 16) & 0xFF, (trophy.colour >> 8) & 0xFF, trophy.colour & 0xFF);
		submitDesc.lightMultiply = temple.GetLight().Colour(colour / 255.0f);
		submitDesc.lightAdd = temple.GetLight().add;
		const TempleLook look {
		    .subMeshTextures = skins,
		    .instances = &instances,
		};
		DrawTempleMesh(*mesh, submitDesc, look, programs);
	}
}

void Renderer::DrawTempleMists(const DrawSceneDesc& desc) const
{
	if (desc.viewId == RenderPass::Reflection)
	{
		return;
	}
	using namespace ecs::components;
	const auto& temple = Locator::temple::value();
	// Read through the const registry, which makes no storage of mists while there are none
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	registry.Each<const MistDome, const Transform, const TempleInteriorPart>(
	    [&](const MistDome& mist, const Transform& transform, const TempleInteriorPart& part) {
		    if (!temple.IsRoomDrawn(part.room))
		    {
			    return;
		    }
		    // The game's mist object, drawn as the island's mists are (approximate: lit, as they are, by the land
		    // under its place, which inside the temple is not known)
		    DrawMist(desc.viewId, *desc.camera,
		             mists::MistDesc {
		                 .position = transform.position,
		                 .size = mist.size,
		                 .colour = mist.colour,
		                 .edgeShrink = false,
		                 .k = 1.0f,
		                 .counter = mist.clock.counter,
		             });
	    });
}

void Renderer::DrawTempleText(const DrawSceneDesc& desc) const
{
	if (desc.viewId != RenderPass::Main)
	{
		return;
	}
	const auto& temple = Locator::temple::value();
	const auto* texture = temple.GetTextTexture();
	bgfx::TransientVertexBuffer buffer;
	if (texture == nullptr || !AllocOrientedText(temple.GetText(), buffer))
	{
		return;
	}
	// The game draws the glyphs blended by their coverage, tested against the room's depth
	const auto* shader = _shaderManager->GetShader("Text3D");
	const auto model = glm::translate(temple.GetPosition());
	bgfx::setTransform(glm::value_ptr(model));
	bgfx::setVertexBuffer(0, &buffer);
	shader->SetTextureSampler("s_diffuse", 0, *texture);
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA |
	               BGFX_STATE_BLEND_ALPHA);
	bgfx::submit(static_cast<bgfx::ViewId>(desc.viewId), toBgfx(shader->GetRawHandle()));
}

void Renderer::DrawTempleLightBeams(const DrawSceneDesc& desc, const TemplePrograms& programs) const
{
	if (desc.viewId == RenderPass::Reflection)
	{
		return;
	}
	using namespace ecs::components;
	const auto& temple = Locator::temple::value();
	auto& registry = Locator::entitiesRegistry::value();
	auto& resources = Locator::resources::value();
	// Added by alpha, without writing depth, from both sides
	constexpr uint64_t k_State = k_AdditiveState | BGFX_STATE_DEPTH_TEST_GREATER;

	// The spot lights' cones, with the atmosphere texture. Each room's lights drift on together, a step for each cone
	// drawn, so a room of more cones drifts faster.
	constexpr size_t k_RoomCount = static_cast<size_t>(TempleRoom::Unknown) + 1;
	std::array<uint32_t, k_RoomCount> coneCounts {};
	registry.Each<const LightBeam, const TempleInteriorPart>(
	    [&coneCounts](const LightBeam& /*unused*/, const TempleInteriorPart& part) {
		    coneCounts.at(static_cast<size_t>(part.room))++;
	    });
	auto& cones = _templeCones;
	cones.vertices.clear();
	cones.indices.clear();
	const float seconds = static_cast<float>(desc.time) * 0.001f;
	registry.Each<const LightBeam, const TempleInteriorPart>(
	    [&cones, &coneCounts, &temple, seconds](const LightBeam& beam, const TempleInteriorPart& part) {
		    if (temple.IsRoomDrawn(part.room))
		    {
			    AppendCone(beam.cone, seconds * static_cast<float>(coneCounts.at(static_cast<size_t>(part.room))) * 0.1f,
			               cones);
		    }
	    });
	const auto& textures = resources.GetTextures();
	if (textures.Contains(k_Atmos) && textures.Contains(k_AtmosAlpha))
	{
		SubmitBeamMesh(desc.viewId, programs.beam, cones, glm::mat4(1.0f), textures.Handle(k_Atmos)->GetNativeHandle(),
		               textures.Handle(k_AtmosAlpha)->GetNativeHandle(), k_State);
	}

	// The light the rooms' windows shed, with the windows' textures
	registry.Each<const ecs::components::Mesh, const Transform, const TempleInteriorPart>(
	    [&](const ecs::components::Mesh& mesh, const Transform& transform, const TempleInteriorPart& part) {
		    if (part.mesh != TempleInteriorMesh::Room || !temple.IsRoomDrawn(part.room) ||
		        !resources.GetMeshes().Contains(mesh.id))
		    {
			    return;
		    }
		    const auto l3dMesh = resources.GetMeshes().Handle(mesh.id);
		    const auto model = glm::translate(transform.position) * glm::mat4(transform.rotation) * glm::scale(transform.scale);
		    for (const auto& volumeLight : l3dMesh->GetVolumeLights())
		    {
			    if (const auto* texture = world_triangles::PrimitiveTexture(*l3dMesh, volumeLight.skinID); texture != nullptr)
			    {
				    SubmitBeamMesh(desc.viewId, programs.beam, volumeLight.mesh, model, texture->GetNativeHandle(),
				                   std::nullopt, k_State);
			    }
		    }
	    });
}

void Renderer::DrawTempleGlows(const DrawSceneDesc& desc, const TemplePrograms& programs) const
{
	using namespace ecs::components;
	const auto& temple = Locator::temple::value();
	auto& registry = Locator::entitiesRegistry::value();
	const bool reflection = desc.viewId == RenderPass::Reflection;
	const auto facing = FacingCamera(*desc.camera);

	registry.Each<const Sprite, const Transform, const TempleInteriorPart>(
	    [&](const Sprite& sprite, const Transform& transform, const TempleInteriorPart& part) {
		    // The temple draws the glows of the rooms it draws whole, and the main room reflects its own glows alone in
		    // its floor
		    const bool inMainRoom = part.room == TempleRoom::Main;
		    if (reflection ? !inMainRoom : !temple.IsRoomDrawn(part.room))
		    {
			    return;
		    }
		    BeamMesh glow;
		    AppendGlowQuad(glow, transform.position, sprite.facesCamera ? facing : transform.rotation,
		                   glm::vec2(transform.scale), sprite.uvMin, sprite.uvExtent, sprite.tint);
		    // Added to what is behind by alpha, or blended over it, tested against the rooms' depth
		    const uint64_t blend = sprite.additive ? BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE)
		                                           : BGFX_STATE_BLEND_ALPHA;
		    SubmitBeamMesh(desc.viewId, programs.beam, glow, glm::mat4(1.0f), sprite.texture, sprite.alpha,
		                   BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA | BGFX_STATE_DEPTH_TEST_GREATER | blend);
	    });
}
