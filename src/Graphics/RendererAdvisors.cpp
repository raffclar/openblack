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
#include <cstring>

#include <algorithm>
#include <array>
#include <limits>
#include <span>
#include <vector>

#include <bgfx/bgfx.h>
#include <entt/core/hashed_string.hpp>
#include <glm/vec4.hpp>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/Mesh.h"
#include "Graphics/RenderModes.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/Texture2D.h"
#include "Graphics/VertexBuffer.h"
#include "Graphics/ZSort.h"
#include "Help/AdvisorModel.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
using AdvisorSystem = ecs::systems::AdvisorSystemInterface;

constexpr auto k_Smoke = entt::hashed_string("raw/smoke");
constexpr auto k_SmokeAlpha = entt::hashed_string("raw/smokea");
constexpr auto k_Rainbow = entt::hashed_string("raw/rainbow");
constexpr auto k_RainbowAlpha = entt::hashed_string("raw/rainbowa");
/// The rainbow trail's material's alpha a fragment needs, of 255
constexpr float k_TrailAlphaRef = 5.0f;

struct QuadVertex
{
	glm::vec3 position;
	glm::vec2 uv;
	uint32_t abgr;
};

const bgfx::VertexLayout& QuadLayout()
{
	static const auto k_Layout = [] {
		bgfx::VertexLayout layout;
		layout.begin()
		    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
		    .end();
		return layout;
	}();
	return k_Layout;
}

/// 0xAARRGGBB as the bytes red, green, blue, alpha
uint32_t Abgr(uint32_t argb)
{
	return (argb & 0xFF00FF00u) | ((argb >> 16u) & 0xFFu) | ((argb & 0xFFu) << 16u);
}

/// The mode a material is drawn in while the whole model fades: its blended form
render_modes::Mode FadingMode(render_modes::Mode mode)
{
	switch (mode)
	{
	case render_modes::Mode::Textured:
		return render_modes::Mode::TexturedAlpha;
	case render_modes::Mode::AlphaTextured:
		return render_modes::Mode::AlphaTexturedAlpha;
	case render_modes::Mode::TexturedChroma:
		return render_modes::Mode::TexturedChromaAlpha;
	default:
		return mode;
	}
}

/// A sprite's four corners in the plane of the screen at its place, and its cell's texture coordinates: top left, top
/// right, bottom right and bottom left
void AddSprite(const AdvisorSystem::Sprite& sprite, const glm::vec3& right, const glm::vec3& up,
               std::vector<QuadVertex>& vertices)
{
	const float c = std::cos(sprite.angle);
	const float s = std::sin(sprite.angle);
	const std::array<glm::vec2, 4> corners {
	    glm::vec2(-sprite.halfWidth, sprite.halfHeight), glm::vec2(sprite.halfWidth, sprite.halfHeight),
	    glm::vec2(sprite.halfWidth, -sprite.halfHeight), glm::vec2(-sprite.halfWidth, -sprite.halfHeight)};
	constexpr std::array<glm::vec2, 4> k_CornerUv {glm::vec2(0.0f, 0.0f), glm::vec2(0.125f, 0.0f), glm::vec2(0.125f, 0.125f),
	                                               glm::vec2(0.0f, 0.125f)};
	const glm::vec2 cell(static_cast<float>(sprite.cell % 8) * 0.125f, static_cast<float>(sprite.cell / 8) * 0.125f);
	std::array<QuadVertex, 4> quad {};
	for (size_t i = 0; i < quad.size(); ++i)
	{
		// Across the sprite turns to (cos, -sin) on the screen, and up it to (sin, cos)
		const glm::vec2 local = corners.at(i);
		const float across = local.x * c + local.y * s;
		const float upward = -local.x * s + local.y * c;
		quad.at(i) = {sprite.position + right * across + up * upward, cell + k_CornerUv.at(i), Abgr(sprite.argb)};
	}
	for (const size_t i : {0u, 1u, 2u, 0u, 2u, 3u})
	{
		vertices.push_back(quad.at(i));
	}
}

/// Textured world triangles of the advisors' own: their halo and smoke, and their trails
void SubmitQuads(const ShaderManager& shaders, RenderPass view, std::span<const QuadVertex> vertices, entt::id_type texture,
                 entt::id_type alpha, float alphaRef, uint64_t state, uint32_t depth)
{
	const auto& textures = Locator::resources::value().GetTextures();
	const auto count = static_cast<uint32_t>(vertices.size());
	if (count == 0 || !textures.Contains(texture) || !textures.Contains(alpha) ||
	    bgfx::getAvailTransientVertexBuffer(count, QuadLayout()) < count)
	{
		return;
	}
	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, count, QuadLayout());
	std::memcpy(buffer.data, vertices.data(), vertices.size_bytes());
	const auto* program = shaders.GetShader("AdvisorQuad");
	program->SetTextureSampler("s_diffuse", 0, *textures.Handle(texture));
	program->SetTextureSampler("s_alpha", 1, *textures.Handle(alpha));
	const glm::vec4 ref {alphaRef, 0.0f, 0.0f, 0.0f};
	program->SetUniformValue("u_quadAlphaRef", &ref);
	bgfx::setVertexBuffer(0, &buffer);
	bgfx::setState(state);
	program->Submit(static_cast<uint16_t>(view), depth);
}
} // namespace

void Renderer::DrawAdvisors(const DrawSceneDesc& desc) const
{
	if (!Locator::advisorSystem::has_value())
	{
		return;
	}
	const auto& advisors = Locator::advisorSystem::value();
	const auto draws = advisors.GetDraws();
	const auto trails = advisors.GetTrails();
	if (draws.empty() && trails.empty())
	{
		return;
	}
	const auto& camera = *desc.camera;
	// In the temple everything is drawn in the order it comes, in the scene's own pass
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	const auto translucentView = inTemple ? desc.viewId : TranslucentPassOf(desc.viewId);
	const glm::mat4 toWorld = glm::inverse(camera.GetViewMatrix(Camera::Interpolation::Current));
	const glm::vec3 right(toWorld[0]);
	const glm::vec3 up(toWorld[1]);

	// The advisors near the screen have a view of their own, over everything, in the order they are drawn
	if (desc.frameBuffer != nullptr)
	{
		desc.frameBuffer->Bind(RenderPass::Advisors);
	}
	bgfx::touch(static_cast<bgfx::ViewId>(RenderPass::Advisors));
	bgfx::setViewMode(static_cast<bgfx::ViewId>(RenderPass::Advisors), bgfx::ViewMode::Sequential);
	_shaderManager->SetCamera(RenderPass::Advisors, camera);

	const auto* program = _shaderManager->GetShader("Advisor");
	const auto& island = Locator::terrainSystem::value();
	const glm::vec4 islandExtent(island.GetExtent().minimum, island.GetExtent().maximum);
	const glm::vec3 worldLight(_modelLight);
	// The smoke is blended over what is behind it, tested against depth but leaving none, from both sides
	constexpr uint64_t k_SmokeState =
	    BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_MSAA;

	for (const auto& draw : draws)
	{
		const auto* model = advisors.GetModel(draw.advisor);
		// Near the screen, after the scene; in the world, opaque with the scene or, fading, among what blends there by
		// its distance
		const bool fading = draw.alpha < 255;
		const RenderPass view = draw.nearScreen ? RenderPass::Advisors : (fading ? translucentView : desc.viewId);
		const uint32_t sortDepth = draw.bones.empty() ? 0u : zsort::Depth(glm::vec3(draw.bones.front()[3]), camera.GetOrigin());
		if (model != nullptr && model->mesh && draw.alpha != 0 && !draw.bones.empty() &&
		    draw.bones.size() == model->mesh->GetBoneMatrices().size())
		{
			// Near the screen the light starts beside the camera and moves towards the world's light as the advisor
			// goes out into the world
			glm::vec3 light = worldLight;
			if (draw.nearLight)
			{
				const glm::vec3 towards = (worldLight - *draw.nearLight) * draw.inWorld;
				light = (towards + towards) + *draw.nearLight;
			}
			const glm::vec4 u_modelLight(light, 0.0f);
			const glm::vec4 u_advisorLook(static_cast<float>(draw.worldShade), draw.landLightPoint.x, draw.landLightPoint.z,
			                              static_cast<float>(draw.alpha) / 255.0f);
			std::array<glm::vec4, 2> u_pupils {};
			if (draw.pupils)
			{
				for (size_t eye = 0; eye < u_pupils.size(); ++eye)
				{
					const auto& pupil = draw.pupils->at(eye);
					u_pupils.at(eye) = glm::vec4(static_cast<float>(pupil.bone), pupil.scale, 1.0f);
				}
			}
			const glm::vec4 u_pupilCentre(draw.pupilCentre, draw.pupilDownAlongZ ? 1.0f : 0.0f, 0.0f);
			const auto& skins = model->mesh->GetSkins();
			for (const auto& subMesh : model->mesh->GetSubMeshes())
			{
				if (subMesh->IsPhysics() || subMesh->GetFlags().status != 0 || (subMesh->GetFlags().lodMask & 1) != 1)
				{
					continue;
				}
				for (const auto& primitive : subMesh->GetPrimitives())
				{
					auto mode =
					    static_cast<render_modes::Mode>(std::min(primitive.materialType, render_modes::k_ModeCount - 1));
					const float materialRef = std::round(primitive.alphaCutoutThreshold * 255.0f);
					float alphaRef = materialRef;
					if (fading)
					{
						mode = FadingMode(mode);
						// Fading, the alpha a fragment needs drops with the model's
						alphaRef = std::max(0.0f, std::trunc(materialRef * static_cast<float>(draw.alpha) / 255.0f - 5.0f));
					}
					const auto& modeDesc = render_modes::Desc(mode);
					uint64_t state =
					    BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA;
					if (modeDesc.zWrite)
					{
						state |= BGFX_STATE_WRITE_Z;
					}
					if (modeDesc.blend == render_modes::Blend::Standard)
					{
						state |= BGFX_STATE_BLEND_ALPHA;
					}
					// The meshes face clockwise
					if (!primitive.twoSided)
					{
						state |= BGFX_STATE_CULL_CCW;
					}
					const glm::vec4 u_advisorMode(modeDesc.alphaModulate ? 1.0f : 0.0f, modeDesc.alphaTest ? alphaRef : -1.0f,
					                              0.0f, 0.0f);
					const auto skin = primitive.skinID != 0xFFFFFFFF ? skins.find(primitive.skinID) : skins.end();
					if (skin == skins.end())
					{
						continue;
					}
					bgfx::setTransform(draw.bones.data(), static_cast<uint16_t>(draw.bones.size()));
					program->SetUniformValue("u_modelLight", &u_modelLight);
					program->SetUniformValue("u_islandExtent", &islandExtent);
					program->SetUniformValue("u_advisorLook", &u_advisorLook);
					program->SetUniformValue("u_advisorPupil0", &u_pupils[0]);
					program->SetUniformValue("u_advisorPupil1", &u_pupils[1]);
					program->SetUniformValue("u_advisorPupilCentre", &u_pupilCentre);
					program->SetUniformValue("u_advisorMode", &u_advisorMode);
					program->SetTextureSampler("s_diffuse", 0, *skin->second);
					program->SetTextureSampler("s_landLuminosity", 6, GetLandLuminosity());
					program->SetTextureSampler("s_landLight", 7, GetLandLightTexture());
					program->SetTextureSampler("s_landColour", 8, GetLandColour());
					subMesh->GetMesh().GetIndexBuffer().Bind(primitive.indicesCount, primitive.indicesOffset);
					subMesh->GetMesh().GetVertexBuffer().Bind();
					bgfx::setState(state);
					program->Submit(static_cast<uint16_t>(view), sortDepth);
				}
			}
		}
		// Then its halo and its puff's smoke; in the world they come before all that is sorted
		std::vector<QuadVertex> sprites;
		for (const auto& sprite : draw.sprites)
		{
			AddSprite(sprite, right, up, sprites);
		}
		SubmitQuads(*_shaderManager, draw.nearScreen ? RenderPass::Advisors : translucentView, sprites, k_Smoke.value(),
		            k_SmokeAlpha.value(), -1.0f, k_SmokeState, std::numeric_limits<uint32_t>::max());
	}

	// The trails come after all else that blends in the world, before the advisors near the screen
	std::vector<QuadVertex> trail;
	trail.reserve(trails.size());
	for (const auto& vertex : trails)
	{
		trail.push_back({vertex.position, vertex.uv, Abgr(vertex.argb)});
	}
	SubmitQuads(*_shaderManager, translucentView, trail, k_Rainbow.value(), k_RainbowAlpha.value(), k_TrailAlphaRef,
	            k_SmokeState | BGFX_STATE_WRITE_Z, 0);
}
