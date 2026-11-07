/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The renderer is one of the locator's implementations
#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>
#include <cstring>

#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include <bgfx/bgfx.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>

#include "3D/AllMeshes.h"
#include "3D/ChimneySmoke.h"
#include "3D/Clouds.h"
#include "3D/HandMorph.h"
#include "3D/InfluenceCircle.h"
#include "3D/L3DAnim.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandLightTable.h"
#include "3D/Mists.h"
#include "3D/Rain.h"
#include "3D/SnowCover.h"
#include "3D/StormClouds.h"
#include "3D/TempleInteriorInterface.h"
#include "3D/TempleMap.h"
#include "3D/WaterRings.h"
#include "Camera/Camera.h"
#include "Creature/CreatureHairLight.h"
#include "ECS/Components/ChimneySmoke.h"
#include "ECS/Components/Cloud.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/DestructionGhost.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/LightBeam.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/MistDome.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Vortex.h"
#include "ECS/DrawnRegistry.h"
#include "ECS/PlayerColour.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/CloudSystemInterface.h"
#include "ECS/Systems/ExplosionSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/RainSystemInterface.h"
#include "ECS/Systems/SnowfallSystemInterface.h"
#include "ECS/Systems/WaterRingSystemInterface.h"
#include "EngineConfig.h"
#include "Graphics/CreatureReflection.h"
#include "Graphics/DebugLines.h"
#include "Graphics/GroundBlobs.h"
#include "Graphics/HandLight.h"
#include "Graphics/HandReflection.h"
#include "Graphics/HandWaterGlow.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/Mesh.h"
#include "Graphics/MeshDetail.h"
#include "Graphics/RendererDetail.h"
#include "Graphics/SeaRows.h"
#include "Graphics/SeeThroughState.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/ViewFrustum.h"
#include "Graphics/ZSort.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Profiler.h"
#include "Renderer.h"
#include "Resources/Rereads.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Scenery/ObjectDrawList.h"
#include "Visual/DetailLevel.h"
#include "Visual/LightBeams.h"
#include "entt/locator/locator.hpp"

using namespace openblack;
using namespace openblack::graphics;
using namespace openblack::graphics::detail;
using namespace openblack::ecs::systems;

void Renderer::DrawMistDomes(const DrawSceneDesc& desc) const
{
	if (desc.viewId == RenderPass::Reflection || !Locator::temple::has_value() || !Locator::temple::value().Active())
	{
		return;
	}
	using namespace ecs::components;
	const auto& temple = Locator::temple::value();
	const auto* shader = _shaderManager->GetShader("Beam");
	// The mists are things of the temple's rooms
	const auto& registry = temple.GetRooms();
	const auto& textures = Locator::resources::value().GetTextures();
	const auto smoke = entt::hashed_string("raw/smoke");
	const auto smokeAlpha = entt::hashed_string("raw/smokea");
	if (!textures.Contains(smoke.value()) || !textures.Contains(smokeAlpha.value()))
	{
		return;
	}

	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .end();
	// The smoke's material: blended by alpha, without writing depth, from both sides
	constexpr uint64_t k_State = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA |
	                             BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA);
	// It turns to the camera as the camera is turned
	const auto facing = glm::mat4(glm::mat3(Locator::camera::value().GetRotationMatrix()));

	registry.Each<const MistDome, const Transform, const TempleInteriorPart>(
	    [&](const MistDome& mist, const Transform& transform, const TempleInteriorPart& part) {
		    if (mist.dome == nullptr || !temple.IsRoomDrawn(part.room))
		    {
			    return;
		    }
		    const auto vertexCount = static_cast<uint32_t>(mist.dome->vertices.size());
		    const auto indexCount = static_cast<uint32_t>(mist.dome->indices.size());
		    if (vertexCount == 0 || indexCount == 0 || bgfx::getAvailTransientVertexBuffer(vertexCount, layout) < vertexCount ||
		        bgfx::getAvailTransientIndexBuffer(indexCount) < indexCount)
		    {
			    return;
		    }
		    bgfx::TransientVertexBuffer vertices;
		    bgfx::TransientIndexBuffer indices;
		    bgfx::allocTransientVertexBuffer(&vertices, vertexCount, layout);
		    bgfx::allocTransientIndexBuffer(&indices, indexCount);
		    auto* out = reinterpret_cast<BeamVertex*>(vertices.data);
		    const auto colour = glm::u8vec4(glm::clamp(mist.colour, 0.0f, 1.0f) * 255.0f);
		    for (uint32_t i = 0; i < vertexCount; ++i)
		    {
			    out[i] = mist.dome->vertices[i];
			    out[i].colour = colour;
			    out[i].uv += mist.uvOffset;
		    }
		    std::memcpy(indices.data, mist.dome->indices.data(), indexCount * sizeof(uint16_t));

		    const glm::vec4 u_beamParams {1.0f, 0.0f, 0.0f, 0.0f};
		    shader->SetTextureSampler("s_diffuse", 0, *textures.Handle(smoke));
		    shader->SetTextureSampler("s_alpha", 1, *textures.Handle(smokeAlpha));
		    shader->SetUniformValue("u_beamParams", &u_beamParams);
		    const auto model = glm::translate(glm::mat4(1.0f), transform.position) * facing * glm::scale(transform.scale);
		    bgfx::setTransform(glm::value_ptr(model));
		    bgfx::setVertexBuffer(0, &vertices);
		    bgfx::setIndexBuffer(&indices);
		    bgfx::setState(k_State);
		    shader->Submit(static_cast<bgfx::ViewId>(desc.viewId));
	    });
}

void Renderer::DrawLightBeams(const DrawSceneDesc& desc) const
{
	// The lower detail levels draw no light shafts
	if (desc.viewId == RenderPass::Reflection || !Locator::temple::has_value() || !Locator::temple::value().Active() ||
	    !detail_level::TempleLightShafts(Locator::config::value().detail))
	{
		return;
	}
	using namespace ecs::components;
	const auto& temple = Locator::temple::value();
	const auto inDrawnRoom = [&temple](TempleRoom room) { return temple.IsRoomDrawn(room); };
	const auto* beamShader = _shaderManager->GetShader("Beam");
	// The cones and the rooms are things of the temple's rooms
	const auto& registry = temple.GetRooms();
	auto& resources = Locator::resources::value();

	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .end();
	static_assert(sizeof(BeamVertex) == 6 * sizeof(float));

	// Added by alpha, without writing depth, from both sides
	constexpr uint64_t k_State = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA |
	                             BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE);
	const auto submit = [&](const BeamMesh& mesh, const glm::mat4& model, const Texture2D& texture, const Texture2D* alpha) {
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

		const glm::vec4 u_beamParams {alpha != nullptr ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f};
		beamShader->SetTextureSampler("s_diffuse", 0, texture);
		beamShader->SetTextureSampler("s_alpha", 1, alpha != nullptr ? *alpha : texture);
		beamShader->SetUniformValue("u_beamParams", &u_beamParams);
		bgfx::setTransform(glm::value_ptr(model));
		bgfx::setVertexBuffer(0, &vertices);
		bgfx::setIndexBuffer(&indices);
		bgfx::setState(k_State);
		beamShader->Submit(static_cast<bgfx::ViewId>(desc.viewId));
	};

	// The spot lights' cones, with the atmosphere texture. Each room's lights drift on together, a step for each cone
	// drawn, so a room of more cones drifts faster.
	std::map<TempleRoom, uint32_t> coneCounts;
	registry.Each<const LightBeam, const TempleInteriorPart>(
	    [&coneCounts](const LightBeam& /*unused*/, const TempleInteriorPart& part) { coneCounts[part.room]++; });
	BeamMesh cones;
	registry.Each<const LightBeam, const TempleInteriorPart>(
	    [&cones, &coneCounts, &inDrawnRoom, &desc](const LightBeam& beam, const TempleInteriorPart& part) {
		    if (inDrawnRoom(part.room))
		    {
			    const float seconds = static_cast<float>(desc.time) * 0.001f;
			    AppendCone(beam.cone, seconds * static_cast<float>(coneCounts[part.room]) * 0.1f, cones);
		    }
	    });
	const auto atmos = resources.GetTextures().Handle(entt::hashed_string("raw/ATMOS"));
	const auto atmosAlpha = resources.GetTextures().Handle(entt::hashed_string("raw/ATMOSA"));
	if (atmos && atmosAlpha)
	{
		submit(cones, glm::mat4(1.0f), *atmos, &*atmosAlpha);
	}

	// The light the rooms' windows shed, with the windows' textures
	registry.Each<const ecs::components::Mesh, const Transform, const TempleInteriorPart>(
	    [&](const ecs::components::Mesh& mesh, const Transform& transform, const TempleInteriorPart& part) {
		    if (part.mesh != TempleInteriorMesh::Room || !inDrawnRoom(part.room))
		    {
			    return;
		    }
		    const auto l3dMesh = resources.GetMeshes().Handle(mesh.id);
		    const auto model = glm::translate(transform.position) * glm::mat4(transform.rotation) * glm::scale(transform.scale);
		    for (const auto& volumeLight : l3dMesh->GetVolumeLights())
		    {
			    if (const auto* texture = GetTexture(volumeLight.skinID, l3dMesh->GetSkins()); texture != nullptr)
			    {
				    submit(volumeLight.mesh, model, *texture, nullptr);
			    }
		    }
	    });
}

void Renderer::DrawVortexDepthWalls(const DrawSceneDesc& desc, RenderPass viewId) const
{
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto wallsId = resources::HashIdentifier(MeshId::SpellZCheatBox);
	if (!meshes.Contains(wallsId))
	{
		return;
	}
	const auto& walls = *meshes.Handle(wallsId);
	const auto& infos = Locator::infoConstants::value().vortex;
	// Every frame, open or not: four walls round a square under the vortex's middle, from the sea's level down, at the
	// kind's base size and facing out, that write their depth and no colour, before the funnel and the land
	desc.entities.Each<const ecs::components::Vortex>([&](entt::entity, const ecs::components::Vortex& vortex) {
		const auto row = static_cast<size_t>(vortex.type);
		if (row >= infos.size())
		{
			return;
		}
		const auto model =
		    glm::translate(glm::vec3(vortex.centre.x, 0.0f, vortex.centre.z)) * glm::scale(glm::vec3(infos.at(row).baseScale));
		L3DMeshSubmitDesc submitDesc = {};
		submitDesc.viewId = viewId;
		submitDesc.program = _shaderManager->GetShader("Object");
		submitDesc.state = BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA;
		submitDesc.useMaterialCulling = true;
		submitDesc.modelMatrices = &model;
		submitDesc.matrixCount = 1;
		DrawMesh(walls, submitDesc, std::numeric_limits<uint8_t>::max());
	});
}

void Renderer::DrawMists(const DrawSceneDesc& desc) const
{
	if (Locator::temple::has_value() && Locator::temple::value().Active())
	{
		return;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto& textures = Locator::resources::value().GetTextures();
	using ecs::components::Mist;
	if (!meshes.Contains(Mist::k_MeshId) || !textures.Contains(Mist::k_TextureId) ||
	    !textures.Contains(Mist::k_AlphaTextureId) || !Locator::terrainSystem::has_value())
	{
		return;
	}
	const auto mesh = meshes.Handle(Mist::k_MeshId);
	const auto texture = textures.Handle(Mist::k_TextureId);
	const auto alphaTexture = textures.Handle(Mist::k_AlphaTextureId);
	const auto* program = _shaderManager->GetShader("Mist");
	const auto& island = Locator::terrainSystem::value();
	const glm::vec4 islandExtent {island.GetExtent().minimum, island.GetExtent().maximum};

	// Every mist faces the camera: the mesh's x across the screen, its y towards the camera and its z up the screen
	const auto& camera = *desc.camera;
	const auto origin = camera.GetOrigin();
	const glm::mat3 facing {camera.GetRight(), -camera.GetForward(), camera.GetUp()};
	// A mist that shrinks edge on is lit from straight above, more brightly
	const glm::vec4 skyLight {0.0f, 500000.0f, 0.0f, 210.0f};

	// The clouds' colour follows the sky's alignment, in the land's light at full luminosity
	const bool cloudsShown = Locator::config::value().detail.clouds;
	uint32_t fullLight = 0xFFFFFF;
	if (_landLightTable)
	{
		const auto texel = _landLightTable->GetTexels().back();
		fullLight = ((texel & 0xFFu) << 16u) | (texel & 0xFF00u) | ((texel >> 16u) & 0xFFu);
	}
	const auto cloudColour = clouds::Colour(Locator::alignmentSystem::value().GetSkyAlignment(), fullLight);
	// The storms' clouds are tinted from the land's colour this frame
	const auto landColour = _landLightTable ? _landLightTable->GetLandColour() : 0xFFFFFFu;

	// Looked up once for all the mists
	const ProgramUniform s_diffuse {*program, "s_diffuse"};
	const ProgramUniform s_alpha {*program, "s_alpha"};
	const ProgramUniform s_landLuminosity {*program, "s_landLuminosity"};
	const ProgramUniform s_landLight {*program, "s_landLight"};
	const ProgramUniform s_landColour {*program, "s_landColour"};
	const ProgramUniform u_islandExtent {*program, "u_islandExtent"};
	const ProgramUniform u_landLightUniform {*program, "u_landLight"};
	const ProgramUniform u_haze {*program, "u_haze"};
	const ProgramUniform u_hazeColour {*program, "u_hazeColour"};
	const ProgramUniform u_modelLight {*program, "u_modelLight"};
	const ProgramUniform u_mistUniform {*program, "u_mist"};
	const ProgramUniform u_mistColourUniform {*program, "u_mistColour"};

	// The view of the main pass. A mist out of it puts no pixel on the screen, so it is not drawn. A warm-up and the
	// other passes draw all of them.
	const auto seenBy =
	    view_frustum::FromViewProjection(camera.GetProjectionMatrix() * camera.GetViewMatrix(Camera::Interpolation::Current));
	const bool testsView = desc.viewId == RenderPass::Main && !_warmingUp && !mesh->IsBoned();
	const auto box = mesh->GetBoundingBox();
	const bool reflection = desc.viewId == RenderPass::Reflection;

	// A storm's cloud is lit by the lightning of its storm
	const auto drawMist = [&](const Mist& mist, const glm::vec3& position, uint32_t colour,
	                          const ecs::systems::StormCloud* stormCloud) {
		const auto alpha = static_cast<float>(colour >> 24u);
		if (alpha <= 0.0f)
		{
			return;
		}
		// A mist that shrinks edge on keeps its width and squashes its depth and height
		glm::vec3 scale {mist.size};
		if (mist.shrinksEdgeOn)
		{
			scale.y = scale.z = mists::EdgeOnSize(mist.size, mist.edgeShrink, position - origin);
		}
		const auto model = glm::translate(position) * glm::mat4(facing) * glm::scale(scale);
		if (testsView && !view_frustum::SeesBox(seenBy, model, box.minima, box.maxima))
		{
			return;
		}
		const auto frame = mists::FrameOffset(mists::Frame(mist.counter), mist.shrinksEdgeOn);
		const glm::vec4 u_mist {frame, 0.0f, 0.0f};
		const glm::vec4 u_mistColour {static_cast<float>((colour >> 16u) & 0xFFu) / 255.0f,
		                              static_cast<float>((colour >> 8u) & 0xFFu) / 255.0f,
		                              static_cast<float>(colour & 0xFFu) / 255.0f, alpha / 255.0f};
		// The others take the land's light where they stand and the haze, in the models' light
		const bool landLit = !mist.shrinksEdgeOn && _landLightTexture.has_value();
		// A storm's cloud is faded by the haze, which adds the flash of its lightning
		const bool hazed = landLit || stormCloud != nullptr;
		const glm::vec4 u_landLight {landLit ? 1.0f : 0.0f, 1.0f, stormCloud != nullptr ? 1.0f : 0.0f,
		                             stormCloud != nullptr ? static_cast<float>(stormCloud->flashGrey) : 0.0f};
		const glm::vec4 noHaze {0.0f};

		for (const auto& subMesh : mesh->GetSubMeshes())
		{
			for (const auto& primitive : subMesh->GetPrimitives())
			{
				bgfx::setTransform(glm::value_ptr(model));
				s_diffuse.Set(0, *texture);
				s_alpha.Set(1, *alphaTexture);
				s_landLuminosity.Set(6, GetLandLuminosity());
				s_landLight.Set(7, GetLandLightTexture());
				s_landColour.Set(8, GetLandColour());
				u_islandExtent.Set(&islandExtent);
				u_landLightUniform.Set(&u_landLight);
				u_haze.Set(hazed ? &_haze[0] : &noHaze);
				u_hazeColour.Set(&_haze[1]);
				u_modelLight.Set(mist.shrinksEdgeOn ? &skyLight : &_modelLight);
				u_mistUniform.Set(&u_mist);
				u_mistColourUniform.Set(&u_mistColour);
				if (subMesh->GetMesh().IsIndexed())
				{
					subMesh->GetMesh().GetIndexBuffer().Bind(primitive.indicesCount, primitive.indicesOffset);
				}
				subMesh->GetMesh().GetVertexBuffer().Bind();
				// Blended over what is behind, tested against but not writing depth. The material of the mesh tells if the
				// back faces show: the dome of a mist shows only its outside, so an eye in a cloud sees no inner wall.
				// L3D meshes face clockwise; the mirrored reflection sees them from the other side.
				const uint64_t cull = primitive.twoSided ? 0 : (reflection ? BGFX_STATE_CULL_CW : BGFX_STATE_CULL_CCW);
				bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_MSAA |
				               cull);
				// In its place among everything that blends, the farthest first
				program->Submit(static_cast<bgfx::ViewId>(TranslucentView(desc.viewId)), zsort::Depth(position, origin));
			}
		}
	};

	desc.entities.Each<const Mist, const ecs::components::Transform>(
	    [&](const Mist& mist, const ecs::components::Transform& transform) {
		    // A land mist draws with an alpha that its whole alpha gives, not with the alpha byte of its colour
		    const auto drawnAlpha = mists::DrawnAlpha(mists::WholeAlphaOf(mist.colour, mist.alphaHighBits));
		    drawMist(mist, transform.position, (static_cast<uint32_t>(drawnAlpha) << 24u) | (mist.colour & 0xFFFFFFu), nullptr);
	    });
	// The sky's clouds are things of the drawn registry
	if (cloudsShown && Locator::drawnRegistry::has_value())
	{
		std::as_const(Locator::drawnRegistry::value())
		    .Each<const ecs::components::Cloud, const Mist, const ecs::components::Transform>(
		        [&](const ecs::components::Cloud& /*unused*/, const Mist& mist, const ecs::components::Transform& transform) {
			        // How far the cloud has faded at the track's ends, times the sky's own alpha, in whole numbers
			        const auto cloudAlpha = ((mist.colour >> 24u) * (cloudColour >> 24u)) / 255u;
			        drawMist(mist, transform.position, (cloudAlpha << 24u) | (cloudColour & 0xFFFFFFu), nullptr);
		        });
	}
	// A storm's cloud takes the land's colour, darkened by the storm's shade
	if (Locator::cloudSystem::has_value())
	{
		for (const auto& stormCloud : Locator::cloudSystem::value().GetStormClouds())
		{
			const auto colour = (stormCloud.mist.colour & 0xFF000000u) | storm_clouds::Tint(landColour, stormCloud.shade);
			drawMist(stormCloud.mist, stormCloud.position, colour, &stormCloud);
		}
	}
}

void Renderer::DrawInfluenceRipples(const DrawSceneDesc& desc) const
{
	using ecs::components::Mist;
	if (desc.viewId != RenderPass::Main || !Locator::influenceSystem::has_value())
	{
		return;
	}
	const auto& textures = Locator::resources::value().GetTextures();
	const auto ripples = Locator::influenceSystem::value().GetRipples();
	if (ripples.empty() || !textures.Contains(Mist::k_TextureId) || !textures.Contains(Mist::k_AlphaTextureId))
	{
		return;
	}
	struct Vertex
	{
		glm::vec3 position;
		glm::vec2 uv;
		uint32_t colour;
	};
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .end();
	const auto* program = _shaderManager->GetShader("WorldTextured");
	const auto viewId = static_cast<bgfx::ViewId>(TranslucentView(desc.viewId));
	const auto origin = desc.camera->GetOrigin();
	// The last frame of the smoke texture, its corners as the puff's
	constexpr std::array<glm::vec2, 4> k_Uvs = {glm::vec2(0.875f, 0.875f), glm::vec2(1.0f, 0.875f), glm::vec2(1.0f, 1.0f),
	                                            glm::vec2(0.875f, 1.0f)};
	constexpr std::array<uint16_t, 6> k_Triangles = {0, 1, 2, 0, 2, 3};
	constexpr auto k_Vertices = static_cast<uint32_t>(influence::Ripple::k_Puffs * 4);
	constexpr auto k_Indices = static_cast<uint32_t>(influence::Ripple::k_Puffs * 6);
	for (const auto& ripple : ripples)
	{
		if (bgfx::getAvailTransientVertexBuffer(k_Vertices, layout) < k_Vertices ||
		    bgfx::getAvailTransientIndexBuffer(k_Indices) < k_Indices)
		{
			return;
		}
		bgfx::TransientVertexBuffer vertexBuffer;
		bgfx::TransientIndexBuffer indexBuffer;
		bgfx::allocTransientVertexBuffer(&vertexBuffer, k_Vertices, layout);
		bgfx::allocTransientIndexBuffer(&indexBuffer, k_Indices);
		const auto vertices = std::span(reinterpret_cast<Vertex*>(vertexBuffer.data), k_Vertices);
		const auto indices = std::span(reinterpret_cast<uint16_t*>(indexBuffer.data), k_Indices);
		const auto abgr = ((ripple.rgb & 0xFFu) << 16u) | (ripple.rgb & 0xFF00u) | ((ripple.rgb >> 16u) & 0xFFu);
		for (size_t puff = 0; puff < influence::Ripple::k_Puffs; ++puff)
		{
			const auto corners = influence::PuffCorners(ripple, puff);
			const auto colour = (static_cast<uint32_t>(ripple.alphas.at(puff)) << 24u) | abgr;
			for (size_t c = 0; c < corners.size(); ++c)
			{
				vertices[(puff * 4) + c] = {corners.at(c), k_Uvs.at(c), colour};
			}
			for (size_t i = 0; i < k_Triangles.size(); ++i)
			{
				indices[(puff * 6) + i] = static_cast<uint16_t>((puff * 4) + k_Triangles.at(i));
			}
		}
		program->SetTextureSampler("s_diffuse", 0, *textures.Handle(Mist::k_TextureId));
		program->SetTextureSampler("s_alpha", 1, *textures.Handle(Mist::k_AlphaTextureId));
		bgfx::setVertexBuffer(0, &vertexBuffer);
		bgfx::setIndexBuffer(&indexBuffer);
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_BLEND_ALPHA);
		program->Submit(viewId, zsort::Depth(ripple.point, origin));
	}
}

void Renderer::DrawInfluenceBorder(const DrawSceneDesc& desc) const
{
	if (desc.viewId != RenderPass::Main || !Locator::influenceSystem::has_value())
	{
		return;
	}
	// In the temple, the main room shows the border at full strength over its map, while the influence button is on.
	// Outside, the border only shows from high enough over the land.
	std::optional<TempleMapInfluence> onMap;
	if (Locator::temple::has_value() && Locator::temple::value().Active())
	{
		onMap = Locator::temple::value().GetMapInfluence();
		if (!onMap.has_value())
		{
			return;
		}
	}
	const auto alpha = onMap.has_value() ? std::optional<uint8_t>(std::numeric_limits<uint8_t>::max())
	                                     : influence::CurtainAlpha(desc.camera->GetOrigin().y);
	static constexpr auto k_TextureId = entt::hashed_string("raw/burn");
	static constexpr auto k_AlphaTextureId = entt::hashed_string("raw/burna");
	const auto& textures = Locator::resources::value().GetTextures();
	// The border shows only from high over the land, or on the temple's map. Thus a warm-up draws it at all heights.
	if ((!alpha.has_value() && !_warmingUp) || !textures.Contains(k_TextureId.value()) ||
	    !textures.Contains(k_AlphaTextureId.value()))
	{
		return;
	}
	const auto& influenceSystem = Locator::influenceSystem::value();
	const auto offset = influenceSystem.GetScrollOffset();
	struct Vertex
	{
		glm::vec3 position;
		glm::vec2 uv;
		uint32_t colour;
	};
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .end();
	const auto* program = _shaderManager->GetShader("WorldTextured");
	// A warm-up draws one triangle of no size, with the border's textures and states
	if (_warmingUp)
	{
		constexpr uint32_t k_Corners = 3;
		if (bgfx::getAvailTransientVertexBuffer(k_Corners, layout) < k_Corners)
		{
			return;
		}
		bgfx::TransientVertexBuffer vertexBuffer;
		bgfx::allocTransientVertexBuffer(&vertexBuffer, k_Corners, layout);
		std::ranges::fill(std::span(reinterpret_cast<Vertex*>(vertexBuffer.data), k_Corners), Vertex {});
		program->SetTextureSampler("s_diffuse", 0, *textures.Handle(k_TextureId));
		program->SetTextureSampler("s_alpha", 1, *textures.Handle(k_AlphaTextureId));
		bgfx::setVertexBuffer(0, &vertexBuffer);
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_BLEND_ALPHA);
		program->Submit(static_cast<bgfx::ViewId>(desc.viewId));
		return;
	}
	for (const auto& circle : influenceSystem.GetCircles())
	{
		if (!influenceSystem.IsBorderShown(circle.player))
		{
			continue;
		}
		const auto& curtain = circle.curtain;
		const auto vertexCount = static_cast<uint32_t>(curtain.positions.size());
		const auto indexCount = static_cast<uint32_t>(curtain.indices.size());
		if (bgfx::getAvailTransientVertexBuffer(vertexCount, layout) < vertexCount ||
		    bgfx::getAvailTransientIndexBuffer(indexCount) < indexCount)
		{
			return;
		}
		bgfx::TransientVertexBuffer vertexBuffer;
		bgfx::TransientIndexBuffer indexBuffer;
		bgfx::allocTransientVertexBuffer(&vertexBuffer, vertexCount, layout);
		bgfx::allocTransientIndexBuffer(&indexBuffer, indexCount);
		const auto vertices = std::span(reinterpret_cast<Vertex*>(vertexBuffer.data), vertexCount);
		const auto indices = std::span(reinterpret_cast<uint16_t*>(indexBuffer.data), indexCount);
		// The player's colour; only the middle row shows, the ground and top fading out to it, and not where the
		// circle is inside another of the player's. A hidden closing column fades to white, as the game's does.
		const auto rgb = player_colour::FromTable(influence::k_PlayerColours, circle.player);
		const auto abgr = ((rgb & 0xFFu) << 16u) | (rgb & 0xFF00u) | ((rgb >> 16u) & 0xFFu);
		const auto lastColumn = circle.Columns() - 1;
		for (size_t i = 0; i < vertices.size(); ++i)
		{
			const auto column = i / 3;
			const bool hidden = circle.hidden.at(column);
			const auto colour = hidden && column == lastColumn ? 0xFFFFFFu : abgr;
			const uint32_t a = i % 3 == 1 && !hidden ? *alpha : 0u;
			const auto position = onMap.has_value() ? onMap->ToRoom(curtain.positions[i]) : curtain.positions[i];
			vertices[i] = {position, curtain.uvs[i] + offset, (a << 24u) | colour};
		}
		for (size_t i = 0; i < indices.size(); ++i)
		{
			indices[i] = static_cast<uint16_t>(curtain.indices[i]);
		}
		program->SetTextureSampler("s_diffuse", 0, *textures.Handle(k_TextureId));
		program->SetTextureSampler("s_alpha", 1, *textures.Handle(k_AlphaTextureId));
		bgfx::setVertexBuffer(0, &vertexBuffer);
		bgfx::setIndexBuffer(&indexBuffer);
		// Blended over what is behind, tested against depth but leaving none, both sides
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_BLEND_ALPHA);
		program->Submit(static_cast<bgfx::ViewId>(desc.viewId));
	}
}

void Renderer::DrawChimneySmoke(const DrawSceneDesc& desc) const
{
	using ecs::components::ChimneySmoke;
	using ecs::components::Mist;
	if (desc.viewId != RenderPass::Main || (Locator::temple::has_value() && Locator::temple::value().Active()))
	{
		return;
	}
	const auto& textures = Locator::resources::value().GetTextures();
	if (!textures.Contains(Mist::k_TextureId) || !textures.Contains(Mist::k_AlphaTextureId))
	{
		return;
	}
	const auto texture = textures.Handle(Mist::k_TextureId);
	const auto alphaTexture = textures.Handle(Mist::k_AlphaTextureId);
	const auto* spriteShader = _shaderManager->GetShader("Sprite");
	const auto viewId = static_cast<bgfx::ViewId>(TranslucentView(desc.viewId));
	const auto origin = desc.camera->GetOrigin();
	// Turned to face the camera, with the texture's own alpha, blended over what is behind
	const glm::vec4 u_spriteParams {1.0f, 1.0f, 0.0f, 0.0f};
	desc.entities.Each<const ChimneySmoke>([&](const ChimneySmoke& smoke) {
		if (smoke.state == ChimneySmoke::State::Out)
		{
			return;
		}
		// The whole smoke takes its place in the sort by its chimney, its puffs in turn
		const auto depth = zsort::Depth(smoke.chimney, origin);
		for (const auto& puff : smoke.puffs)
		{
			if (puff.hidden)
			{
				continue;
			}
			const auto look = chimney_smoke::LookOf(puff.age, smoke.rgb);
			// Spinning in the plane of the screen
			const auto model = glm::translate(puff.position) * glm::rotate(-puff.angle, glm::vec3(0.0f, 0.0f, 1.0f)) *
			                   glm::scale(glm::vec3(look.halfWidth));
			const glm::vec4 u_sampleRect {0.125f, 0.125f, mists::FrameOffset(look.frame, false)};
			const float alpha = static_cast<float>(look.argb >> 24u) / 255.0f;
			const glm::vec3 colour =
			    glm::vec3(static_cast<float>((look.argb >> 16u) & 0xFFu), static_cast<float>((look.argb >> 8u) & 0xFFu),
			              static_cast<float>(look.argb & 0xFFu)) /
			    255.0f;
			// The shader multiplies the tint by the texture's alpha: premultiplied for the blend
			const glm::vec4 u_tint {colour * alpha, alpha};
			bgfx::setTransform(glm::value_ptr(model));
			spriteShader->SetUniformValue("u_sampleRect", glm::value_ptr(u_sampleRect));
			spriteShader->SetUniformValue("u_spriteParams", glm::value_ptr(u_spriteParams));
			spriteShader->SetUniformValue("u_tint", glm::value_ptr(u_tint));
			spriteShader->SetTextureSampler("s_diffuse", 0, *texture);
			spriteShader->SetTextureSampler("s_alpha", 1, *alphaTexture);
			_plane->GetVertexBuffer().Bind();
			bgfx::setState(BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
			               BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA));
			spriteShader->Submit(viewId, depth);
		}
	});
}

void Renderer::DrawWaterRings(const DrawSceneDesc& desc) const
{
	if (desc.viewId != RenderPass::Main || !Locator::waterRingSystem::has_value() ||
	    (Locator::temple::has_value() && Locator::temple::value().Active()))
	{
		return;
	}
	const auto rings = Locator::waterRingSystem::value().GetRings();
	const auto& textures = Locator::resources::value().GetTextures();
	if (rings.empty() || !textures.Contains(water_rings::k_TextureId.value()) ||
	    !textures.Contains(water_rings::k_AlphaTextureId.value()))
	{
		return;
	}
	struct Vertex
	{
		glm::vec3 position;
		glm::vec2 uv;
		uint32_t colour;
	};
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .end();
	const auto count = static_cast<uint32_t>(rings.size());
	if (bgfx::getAvailTransientVertexBuffer(count * 4, layout) < count * 4 ||
	    bgfx::getAvailTransientIndexBuffer(count * 6) < count * 6)
	{
		return;
	}
	bgfx::TransientVertexBuffer vertexBuffer;
	bgfx::TransientIndexBuffer indexBuffer;
	bgfx::allocTransientVertexBuffer(&vertexBuffer, count * 4, layout);
	bgfx::allocTransientIndexBuffer(&indexBuffer, count * 6);
	const auto vertices = std::span(reinterpret_cast<Vertex*>(vertexBuffer.data), count * 4);
	const auto indices = std::span(reinterpret_cast<uint16_t*>(indexBuffer.data), count * 6);
	constexpr std::array<uint16_t, 6> k_Triangles = {0, 1, 2, 0, 2, 3};
	for (size_t i = 0; i < rings.size(); ++i)
	{
		const auto& ring = rings[i];
		const auto corners = water_rings::Corners(ring);
		const auto uvs = water_rings::CellUvs(ring.cell);
		const auto rgb = ring.argb & 0xFFFFFFu;
		const auto abgr = (static_cast<uint32_t>(water_rings::Alpha(ring)) << 24u) | ((rgb & 0xFFu) << 16u) | (rgb & 0xFF00u) |
		                  ((rgb >> 16u) & 0xFFu);
		for (size_t c = 0; c < corners.size(); ++c)
		{
			vertices[(i * 4) + c] = {corners.at(c), uvs.at(c), abgr};
		}
		for (size_t t = 0; t < k_Triangles.size(); ++t)
		{
			indices[(i * 6) + t] = static_cast<uint16_t>((i * 4) + k_Triangles.at(t));
		}
	}
	const auto* program = _shaderManager->GetShader("WorldTextured");
	program->SetTextureSampler("s_diffuse", 0, *textures.Handle(water_rings::k_TextureId.value()));
	program->SetTextureSampler("s_alpha", 1, *textures.Handle(water_rings::k_AlphaTextureId.value()));
	bgfx::setVertexBuffer(0, &vertexBuffer);
	bgfx::setIndexBuffer(&indexBuffer);
	// Added over what is behind by their alpha, both sides, tested against depth but leaving none
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_GREATER |
	               BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE));
	program->Submit(static_cast<bgfx::ViewId>(desc.viewId));
}

void Renderer::DrawSnowfall(const DrawSceneDesc& desc) const
{
	// Taking the tiles counts them as drawn, which a warm-up must not do
	if (_warmingUp || desc.viewId != RenderPass::Main || !Locator::snowfallSystem::has_value() ||
	    (Locator::temple::has_value() && Locator::temple::value().Active()))
	{
		return;
	}
	static constexpr auto k_TextureId = entt::hashed_string("raw/ATMOS");
	static constexpr auto k_AlphaTextureId = entt::hashed_string("raw/ATMOSA");
	const auto& textures = Locator::resources::value().GetTextures();
	if (!textures.Contains(k_TextureId.value()) || !textures.Contains(k_AlphaTextureId.value()))
	{
		return;
	}
	auto& snowfallSystem = Locator::snowfallSystem::value();
	const auto origin = desc.camera->GetOrigin();
	const auto tiles = snowfallSystem.TakeTiles(
	    {.eye = origin, .viewProjection = desc.camera->GetViewProjectionMatrix(), .nearPlane = desc.camera->GetNearClip()});
	if (tiles.empty())
	{
		return;
	}
	const auto flakes = snowfallSystem.GetFlakes();
	struct Vertex
	{
		glm::vec3 position;
		glm::vec2 uv;
		uint32_t colour;
	};
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .end();
	// The flakes take the colour of the land's light at its brightest
	const auto landColour = _landLightTable ? _landLightTable->GetLandColour() : 0xFFFFFFu;
	const auto bgr = ((landColour & 0xFFu) << 16u) | (landColour & 0xFF00u) | ((landColour >> 16u) & 0xFFu);
	const auto* program = _shaderManager->GetShader("WorldTextured");
	const auto viewId = static_cast<bgfx::ViewId>(TranslucentView(desc.viewId));
	constexpr std::array<uint16_t, 6> k_Triangles = {0, 1, 2, 2, 3, 0};
	for (const auto& tile : tiles)
	{
		const auto count = static_cast<uint32_t>(tile.flakes);
		if (bgfx::getAvailTransientVertexBuffer(count * 4, layout) < count * 4 ||
		    bgfx::getAvailTransientIndexBuffer(count * 6) < count * 6)
		{
			return;
		}
		bgfx::TransientVertexBuffer vertexBuffer;
		bgfx::TransientIndexBuffer indexBuffer;
		bgfx::allocTransientVertexBuffer(&vertexBuffer, count * 4, layout);
		bgfx::allocTransientIndexBuffer(&indexBuffer, count * 6);
		const auto vertices = std::span(reinterpret_cast<Vertex*>(vertexBuffer.data), count * 4);
		const auto indices = std::span(reinterpret_cast<uint16_t*>(indexBuffer.data), count * 6);
		const glm::vec3 corner {tile.corner.x, tile.ground, tile.corner.y};
		const auto colour = (static_cast<uint32_t>(tile.alpha) << 24u) | bgr;
		for (size_t i = 0; i < count; ++i)
		{
			const auto corners = snowfall::Corners(flakes[i]);
			for (size_t c = 0; c < corners.size(); ++c)
			{
				vertices[(i * 4) + c] = {corner + corners.at(c), snowfall::k_Uvs.at(c), colour};
			}
			for (size_t t = 0; t < k_Triangles.size(); ++t)
			{
				indices[(i * 6) + t] = static_cast<uint16_t>((i * 4) + k_Triangles.at(t));
			}
		}
		program->SetTextureSampler("s_diffuse", 0, *textures.Handle(k_TextureId));
		program->SetTextureSampler("s_alpha", 1, *textures.Handle(k_AlphaTextureId));
		bgfx::setVertexBuffer(0, &vertexBuffer);
		bgfx::setIndexBuffer(&indexBuffer);
		// Both sides of each flake, blended over what is behind, tested against depth but leaving none
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_BLEND_ALPHA);
		// In its place among what blends, by the ground under the quarter's corner
		program->Submit(viewId, zsort::Depth(corner, origin));
	}
}

void Renderer::DrawRain(const DrawSceneDesc& desc) const
{
	// Taking the tiles counts them as drawn, which a warm-up must not do
	if (_warmingUp || desc.viewId != RenderPass::Main || !Locator::rainSystem::has_value() ||
	    (Locator::temple::has_value() && Locator::temple::value().Active()))
	{
		return;
	}
	static constexpr auto k_TextureId = entt::hashed_string("raw/ATMOS");
	static constexpr auto k_AlphaTextureId = entt::hashed_string("raw/ATMOSA");
	const auto& textures = Locator::resources::value().GetTextures();
	if (!textures.Contains(k_TextureId.value()) || !textures.Contains(k_AlphaTextureId.value()))
	{
		return;
	}
	auto& rainSystem = Locator::rainSystem::value();
	const auto origin = desc.camera->GetOrigin();
	const auto tiles = rainSystem.TakeTiles(
	    {.eye = origin, .viewProjection = desc.camera->GetViewProjectionMatrix(), .nearPlane = desc.camera->GetNearClip()});
	if (tiles.empty())
	{
		return;
	}
	const auto streaks = rainSystem.GetStreaks();
	const float height = rainSystem.GetHeight();
	struct Vertex
	{
		glm::vec3 position;
		glm::vec2 uv;
		uint32_t colour;
	};
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .end();
	const auto* program = _shaderManager->GetShader("WorldTextured");
	const auto viewId = static_cast<bgfx::ViewId>(TranslucentView(desc.viewId));
	for (const auto& tile : tiles)
	{
		const auto count = static_cast<uint32_t>(tile.streaks) * 2;
		if (bgfx::getAvailTransientVertexBuffer(count, layout) < count)
		{
			return;
		}
		bgfx::TransientVertexBuffer buffer;
		bgfx::allocTransientVertexBuffer(&buffer, count, layout);
		const auto vertices = std::span(reinterpret_cast<Vertex*>(buffer.data), count);
		const glm::vec3 centre {tile.centre.x, tile.ground, tile.centre.y};
		// Each streak is a white line, as opaque as the block's rain at the bottom and fainter at the top, with a row of
		// the texture scrolling down it
		for (size_t i = 0; i < static_cast<size_t>(tile.streaks); ++i)
		{
			const auto& streak = streaks[i];
			const auto ends = rain::Ends(streak, height);
			const auto bottom = static_cast<uint32_t>(rain::StreakAlpha(tile.alpha, streak.phase)) & 0xFFu;
			const auto top = static_cast<uint32_t>(rain::StreakAlpha(tile.alphaTop, streak.phase)) & 0xFFu;
			vertices[i * 2] = {centre + ends[0], {streak.scroll, rain::k_TextureRow}, (bottom << 24u) | 0xFFFFFFu};
			vertices[(i * 2) + 1] = {centre + ends[1], {streak.scroll + 1.0f, rain::k_TextureRow}, (top << 24u) | 0xFFFFFFu};
		}
		program->SetTextureSampler("s_diffuse", 0, *textures.Handle(k_TextureId));
		program->SetTextureSampler("s_alpha", 1, *textures.Handle(k_AlphaTextureId));
		bgfx::setVertexBuffer(0, &buffer);
		// Lines, blended over what is behind, tested against depth but leaving none
		bgfx::setState(BGFX_STATE_PT_LINES | BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_BLEND_ALPHA);
		// In its place among what blends, by the ground under the block's centre
		program->Submit(viewId, zsort::Depth(centre, origin));
	}
}

void Renderer::DrawDestructionGhosts(const DrawSceneDesc& desc) const
{
	if (desc.viewId != RenderPass::Main || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	const auto& textures = Locator::resources::value().GetTextures();
	const auto pattern = entt::hashed_string("raw/cool_effecta").value();
	if (!textures.Contains(pattern))
	{
		return;
	}
	const auto& patternTexture = textures.Handle(pattern)->GetNativeHandle();
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto camera = desc.camera->GetOrigin();
	const auto translucent = TranslucentView(desc.viewId);
	// The game's whole span of a ghost, in milliseconds, and its pattern's scroll
	constexpr int k_GhostMilliseconds = 500;
	constexpr double k_ScrollRate = 0.7;
	constexpr double k_ScrollHeight = 1.7000000476837158;
	if (!Locator::explosionSystem::has_value())
	{
		return;
	}
	// A warm-up draws a ghost of each building model that is loaded, at no size. Thus the first building that goes makes
	// no new states of the graphics card, also when the building comes after the load.
	std::vector<ecs::components::DestructionGhost> warmUpGhosts;
	if (_warmingUp && Locator::infoConstants::has_value())
	{
		for (const auto& info : Locator::infoConstants::value().abode)
		{
			const auto id = resources::HashIdentifier(info.meshId);
			if (meshes.Contains(id) && std::ranges::none_of(warmUpGhosts, [id](const auto& ghost) { return ghost.mesh == id; }))
			{
				warmUpGhosts.push_back({.mesh = id, .model = glm::mat4(0.0f)});
			}
		}
	}
	const auto ghosts = _warmingUp ? std::span<const ecs::components::DestructionGhost>(warmUpGhosts)
	                               : Locator::explosionSystem::value().Ghosts();
	std::ranges::for_each(ghosts, [&](const ecs::components::DestructionGhost& ghost) {
		if (!meshes.Contains(ghost.mesh))
		{
			return;
		}
		const auto& mesh = *meshes.Handle(ghost.mesh);
		const auto centre = glm::vec3(ghost.model * glm::vec4(mesh.GetBoundingBox().Center(), 1.0f));
		const auto depth = zsort::Depth(centre, camera);
		// The pattern must pass an alpha that rises as the ghost runs out, so less and less of it lays its depth
		const auto passed =
		    static_cast<int32_t>(static_cast<float>(ghost.millisecondsLeft) * 255.0f / static_cast<float>(k_GhostMilliseconds));
		const auto threshold = static_cast<uint8_t>(0xFF - static_cast<uint8_t>(passed));
		const double share = static_cast<float>(ghost.millisecondsLeft) / static_cast<float>(k_GhostMilliseconds);
		const glm::vec2 scroll {static_cast<float>(std::cos(share) + std::cos(share)),
		                        static_cast<float>(std::sin(share * k_ScrollRate) * k_ScrollHeight)};

		L3DMeshSubmitDesc submitDesc = {};
		submitDesc.viewId = translucent;
		submitDesc.modelMatrices = &ghost.model;
		submitDesc.matrixCount = 1;
		submitDesc.sortDepth = depth;
		// First its depth alone, from both sides, through the scrolling pattern
		submitDesc.program = _shaderManager->GetShader("GhostDepth");
		submitDesc.state = BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA;
		submitDesc.skinTexture = &patternTexture;
		submitDesc.uvOffset = scroll;
		submitDesc.alphaThreshold = static_cast<float>(threshold) / 255.0f;
		DrawMesh(mesh, submitDesc, std::numeric_limits<uint8_t>::max());
		// Then itself, added and tested against its materials' alpha, only where that depth was laid
		submitDesc.program = _shaderManager->GetShader("Object");
		submitDesc.state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_EQUAL | BGFX_STATE_MSAA |
		                   BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE);
		submitDesc.useMaterialCulling = true;
		submitDesc.skinTexture = nullptr;
		submitDesc.uvOffset = glm::vec2(0.0f);
		submitDesc.alphaThreshold.reset();
		submitDesc.materialAlphaTest = true;
		// Just after its depth, in the sort
		submitDesc.sortDepth = depth > 0 ? depth - 1 : 0;
		DrawMesh(mesh, submitDesc, std::numeric_limits<uint8_t>::max());
	});
}
