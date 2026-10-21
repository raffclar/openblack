/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The renderer is one of the locator's implementations
#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstring>

#include <string>

#include <bgfx/bgfx.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "3D/AllMeshes.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "ECS/Components/LightBeam.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HeartbeatSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "EngineConfig.h"
#include "Graphics/DetailLevel.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/TempleExitGlow.h"
#include "Graphics/TempleHeartLook.h"
#include "Graphics/Texture2D.h"
#include "Graphics/ZSort.h"
#include "Locator.h"
#include "Particles/ParticleDrawFrame.h"
#include "Particles/VolumeGlow.h"
#include "Renderer.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

void Renderer::DrawTempleExitGlow(const DrawSceneDesc& desc) const
{
	if (desc.viewId == RenderPass::Reflection || !Locator::temple::has_value() || !Locator::temple::value().Active())
	{
		return;
	}
	const auto& temple = Locator::temple::value();
	const auto seconds = temple.GetWayOutSeconds();
	if (!seconds.has_value() || !temple.IsRoomDrawn(TempleRoom::Main))
	{
		return;
	}
	const auto sheet = temple_exit_glow::At(*seconds);
	if (!sheet.has_value())
	{
		return;
	}
	using namespace ecs::components;
	const auto& registry = Locator::entitiesRegistry::value();
	// The sheet is in the main room's space. Its texture drifts with the room's spot lights' textures, a step for each
	// of the room's cones.
	glm::mat4 model {1.0f};
	uint32_t cones = 0;
	registry.Each<const TempleInteriorPart, const Transform>(
	    [&model](const TempleInteriorPart& part, const Transform& transform) {
		    if (part.room == TempleRoom::Main && part.mesh == TempleInteriorMesh::Room)
		    {
			    model = glm::translate(transform.position) * glm::mat4(transform.rotation) * glm::scale(transform.scale);
		    }
	    });
	// The cones step the drift only where the detail level draws them
	if (detail_level::TempleLightShafts(detail_level::For(Locator::config::value().detailLevel)))
	{
		registry.Each<const LightBeam, const TempleInteriorPart>(
		    [&cones](const LightBeam& /*unused*/, const TempleInteriorPart& part) {
			    if (part.room == TempleRoom::Main)
			    {
				    ++cones;
			    }
		    });
	}
	const float phase = static_cast<float>(desc.time) * 0.001f * static_cast<float>(cones) * 0.1f;
	BeamMesh mesh;
	temple_exit_glow::Append(*sheet, phase, mesh);

	auto& textures = Locator::resources::value().GetTextures();
	const auto atmos = textures.Handle(entt::hashed_string("raw/ATMOS"));
	const auto atmosAlpha = textures.Handle(entt::hashed_string("raw/ATMOSA"));
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .end();
	const auto vertexCount = static_cast<uint32_t>(mesh.vertices.size());
	const auto indexCount = static_cast<uint32_t>(mesh.indices.size());
	if (!atmos || !atmosAlpha || bgfx::getAvailTransientVertexBuffer(vertexCount, layout) < vertexCount ||
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

	// Added by alpha over all of the room: the game draws it with no depth test and writes no depth
	constexpr uint64_t k_State = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA |
	                             BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE);
	const auto* shader = _shaderManager->GetShader("Beam");
	const glm::vec4 beamParams {1.0f, 0.0f, 0.0f, 0.0f};
	shader->SetTextureSampler("s_diffuse", 0, *atmos);
	shader->SetTextureSampler("s_alpha", 1, *atmosAlpha);
	shader->SetUniformValue("u_beamParams", &beamParams);
	bgfx::setTransform(glm::value_ptr(model));
	bgfx::setVertexBuffer(0, &vertices);
	bgfx::setIndexBuffer(&indices);
	bgfx::setState(k_State);
	shader->Submit(static_cast<bgfx::ViewId>(desc.viewId));
}

void Renderer::DrawTempleCreedShells(const DrawSceneDesc& desc) const
{
	if (desc.viewId != RenderPass::Main || (Locator::temple::has_value() && Locator::temple::value().Active()))
	{
		return;
	}
	namespace heart_look = temple_heart_look;
	namespace volume_glow = particles::volume_glow;
	using namespace ecs::components;
	const float turnFraction = Locator::time::has_value() ? Locator::time::value().GetTurnFraction() : 0.0f;
	const auto name = std::string("raw/") + std::string(volume_glow::k_Textures.at(heart_look::k_ShellTexture));
	const auto texture = entt::hashed_string(name.c_str()).value();
	const auto alpha = entt::hashed_string((name + "a").c_str()).value();
	const auto eye = desc.camera->GetOrigin();
	const auto& registry = Locator::entitiesRegistry::value();
	registry.Each<const Temple, const Transform>([&](const Temple& temple, const Transform& transform) {
		if (!heart_look::ShellShown(temple.destroying, temple.destructionClock))
		{
			return;
		}
		// The shell reaches out with the heartbeat of the temple's player, and its texture slides from its start
		const float pulse =
		    Locator::heartbeatSystem::has_value() ? Locator::heartbeatSystem::value().Pulse(temple.owner, turnFraction) : 0.0f;
		const auto since = static_cast<uint64_t>((temple.destructionClock - heart_look::k_ShellFrom) * 1000.0f);
		const particles::draw::ShellDraw shell {
		    .object = entt::null,
		    .zoom = heart_look::ShellReach(temple.destructionClock) * pulse,
		    .argb = Player::k_Colours.at(static_cast<size_t>(temple.owner)),
		    .texture = texture,
		    .alphaTexture = alpha,
		    .uvOffset = volume_glow::UvOffset({heart_look::k_ShellSlideAcross, heart_look::k_ShellSlideUp}, since),
		    .textureFraction = {heart_look::k_ShellTextureAcross, heart_look::k_ShellTextureUp},
		    .specularRgb = heart_look::k_ShellSpecular,
		};
		// Centred on the heart, the same size on each axis and not turned
		const auto model =
		    glm::translate(transform.position) * glm::scale(glm::vec3(heart_look::ShellScale(temple.destructionClock)));
		SubmitGlowShell(static_cast<uint16_t>(TranslucentView(desc.viewId)), resources::HashIdentifier(MeshId::Creed), model,
		                shell, desc.camera->GetProjectionMatrix(), false, zsort::Depth(transform.position, eye));
	});
}
