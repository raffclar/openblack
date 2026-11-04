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

#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <bgfx/bgfx.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>

#include "3D/InfluenceCircle.h"
#include "3D/L3DMesh.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "ECS/Components/Sky.h"
#include "ECS/Registry.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "EngineConfig.h"
#include "Graphics/CreatureReflection.h"
#include "Graphics/DebugLines.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/GroundBlobs.h"
#include "Graphics/HandLight.h"
#include "Graphics/HandReflection.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/Moon.h"
#include "Graphics/RendererDetail.h"
#include "Graphics/SeaRows.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/ShadowTexture.h"
#include "Graphics/Sun.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/Rereads.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Scenery/ObjectDrawList.h"
#include "Windowing/WindowingInterface.h"
#include "entt/locator/locator.hpp"

using namespace openblack;
using namespace openblack::graphics;
using namespace openblack::graphics::detail;
using namespace openblack::ecs::systems;

void Renderer::DrawCelestialMesh(RenderPass viewId, const CelestialDraw& draw) const
{
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto& textures = Locator::resources::value().GetTextures();
	if (!meshes.Contains(draw.meshId) || !textures.Contains(draw.textureId) || !textures.Contains(draw.alphaTextureId))
	{
		return;
	}
	const auto* program = _shaderManager->GetShader("Celestial");
	const auto texture = textures.Handle(draw.textureId);
	const auto alphaTexture = textures.Handle(draw.alphaTextureId);
	const auto mesh = meshes.Handle(draw.meshId);
	for (const auto& subMesh : mesh->GetSubMeshes())
	{
		for (const auto& primitive : subMesh->GetPrimitives())
		{
			bgfx::setTransform(glm::value_ptr(draw.model));
			program->SetTextureSampler("s_diffuse", 0, *texture);
			program->SetTextureSampler("s_alpha", 1, *alphaTexture);
			program->SetUniformValue("u_colour", &draw.colour);
			program->SetUniformValue("u_celestial", &draw.celestial);
			if (subMesh->GetMesh().IsIndexed())
			{
				subMesh->GetMesh().GetIndexBuffer().Bind(primitive.indicesCount, primitive.indicesOffset);
			}
			subMesh->GetMesh().GetVertexBuffer().Bind();
			bgfx::setState(draw.state);
			program->Submit(static_cast<bgfx::ViewId>(viewId));
		}
	}
}

namespace
{
/// Added to what is behind it
constexpr uint64_t k_AdditiveState =
    BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE);

/// Far out to the north west, facing the island
glm::mat4 SunModel(const glm::vec3& position)
{
	return glm::translate(position) * glm::rotate(-3.0f * glm::pi<float>() / 4.0f, glm::vec3(0.0f, 1.0f, 0.0f));
}
} // namespace

void Renderer::DrawMoon(RenderPass viewId) const
{
	if (!Locator::camera::has_value() || !Locator::skySystem::has_value())
	{
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto moonEntity = Locator::skySystem::value().GetMoon();
	const auto [moon, body, glowLook] =
	    registry.TryGet<ecs::components::Moon, ecs::components::CelestialBody, ecs::components::CelestialGlow>(moonEntity);
	// A warm-up draws the moon and its glow even before the moon is placed or when it does not show, then at no size
	const bool placed = moon != nullptr && moon->placement.has_value();
	if (moon == nullptr || body == nullptr || (!placed && !_warmingUp))
	{
		return;
	}
	// The moon keeps its place beside the player's camera and faces it. The sea shows it mirrored, and the glow of a
	// copy of it with its height mirrored through sea level.
	const bool inTheSea = viewId == RenderPass::ReflectionSky;
	const auto& camera = Locator::camera::value();
	const auto centre = camera.GetOrigin() + (placed ? moon->placement->offset : glm::vec3(0.0f));
	const auto view = camera.GetViewMatrix(Camera::Interpolation::Current);
	const auto inverseView = glm::inverse(view);
	const auto basis = moon::Basis(view, inverseView, centre);
	const auto& colour = moon->colour;
	// It shows less through an overcast
	const float alpha = moon->strength / 255.0f;
	if (alpha <= 0.0f && !_warmingUp)
	{
		return;
	}

	// Its glow, added to the sky
	const auto drawGlow = [&](const moon::Glow& glow) {
		const auto& textures = Locator::resources::value().GetTextures();
		bgfx::VertexLayout layout;
		layout.begin()
		    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
		    .end();
		constexpr auto k_GlowVertices = static_cast<uint32_t>(moon::k_GlowIndices.size());
		if (glowLook == nullptr || !textures.Contains(glowLook->textureId) || !textures.Contains(glowLook->alphaTextureId) ||
		    bgfx::getAvailTransientVertexBuffer(k_GlowVertices, layout) != k_GlowVertices)
		{
			return;
		}
		struct Vertex
		{
			glm::vec3 position;
			glm::vec2 uv;
		};
		bgfx::TransientVertexBuffer buffer;
		bgfx::allocTransientVertexBuffer(&buffer, k_GlowVertices, layout);
		const auto vertices = std::span(reinterpret_cast<Vertex*>(buffer.data), k_GlowVertices);
		for (size_t i = 0; i < vertices.size(); ++i)
		{
			const auto corner = moon::k_GlowIndices.at(i);
			vertices[i] = {placed ? glow.corners.at(corner) : glm::vec3(0.0f), glow.uvs.at(corner)};
		}
		const auto* program = _shaderManager->GetShader("Celestial");
		const glm::mat4 identity(1.0f);
		const glm::vec4 glowColour {moon::GlowColour(colour), alpha};
		const glm::vec4 celestial {0.0f, 0.0f, 0.0f, 1.0f};
		bgfx::setTransform(glm::value_ptr(identity));
		program->SetTextureSampler("s_diffuse", 0, *textures.Handle(glowLook->textureId));
		program->SetTextureSampler("s_alpha", 1, *textures.Handle(glowLook->alphaTextureId));
		program->SetUniformValue("u_colour", &glowColour);
		program->SetUniformValue("u_celestial", &celestial);
		bgfx::setVertexBuffer(0, &buffer);
		bgfx::setState(k_AdditiveState | BGFX_STATE_DEPTH_TEST_GREATER);
		program->Submit(static_cast<bgfx::ViewId>(viewId));
	};

	// The moon, blended over the sky, its face turned to the real moon's phase. It leaves its depth, so the land nearer
	// than it is drawn over it and the land beyond it stays hidden. Its half sphere shows only its outside, as its
	// material is one sided: the half turned away from the camera, around a new moon, shows nothing. Mirrored in the
	// sea, the faces that turn towards the camera are the other way round.
	const auto drawMoon = [&]() {
		const auto phase = moon->phase;
		DrawCelestialMesh(viewId, {
		                              .meshId = body->meshId,
		                              .textureId = body->textureId,
		                              .alphaTextureId = body->alphaTextureId,
		                              .model = placed ? moon::Model(basis, centre, phase) : glm::mat4(0.0f),
		                              .colour = glm::vec4(colour, alpha),
		                              .celestial = {std::cos(phase), std::sin(phase), 1.0f, 1.0f},
		                              .state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_GREATER |
		                                       BGFX_STATE_BLEND_ALPHA | (inTheSea ? BGFX_STATE_CULL_CW : BGFX_STATE_CULL_CCW),
		                          });
	};

	if (inTheSea)
	{
		// In the sea, the moon mirrored with everything else by the mirrored view, then the glow of its copy, which
		// the mirrored moon in front of it hides
		drawMoon();
		drawGlow(moon::SeaGlow(view, inverseView, centre));
		return;
	}
	// In the sky, the glow first, then the moon over it
	drawGlow(moon::MakeGlow(basis, centre));
	drawMoon();
}

void Renderer::DrawSun(RenderPass viewId) const
{
	if (!Locator::skySystem::has_value())
	{
		return;
	}
	const auto [sun, body] = Locator::entitiesRegistry::value().TryGet<ecs::components::Sun, ecs::components::CelestialBody>(
	    Locator::skySystem::value().GetSun());
	// A warm-up draws the sun even before it is placed, then at no size
	if (sun == nullptr || body == nullptr || (!sun->placement.has_value() && !_warmingUp))
	{
		return;
	}
	// The sky of the temple has no sun, and the sea mirrors no sun: only the moon goes into the sea's sky
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	// A script can also hide the sun
	const bool hidden = inTemple || Locator::skySystem::value().IsSunHidden();
	if (!_warmingUp && (hidden || viewId == RenderPass::ReflectionSky))
	{
		return;
	}
	// In a warm colour, shown less through an overcast, added to the sky drawn before it, leaving no depth; the land
	// drawn after it covers it
	DrawCelestialMesh(viewId, {
	                              .meshId = body->meshId,
	                              .textureId = body->textureId,
	                              .alphaTextureId = body->alphaTextureId,
	                              .model = sun->placement.has_value() ? SunModel(sun->placement->position) : glm::mat4(0.0f),
	                              .colour = glm::vec4(sun->colour, sun->strength / 255.0f),
	                              .celestial = glm::vec4(0.0f),
	                              .state = k_AdditiveState | BGFX_STATE_DEPTH_TEST_GREATER,
	                          });
}

void Renderer::DrawSunGlare(const Camera& camera) const
{
	if (!Locator::skySystem::has_value())
	{
		return;
	}
	const auto [sun, body] = Locator::entitiesRegistry::value().TryGet<ecs::components::Sun, ecs::components::CelestialBody>(
	    Locator::skySystem::value().GetSun());
	// A warm-up draws the glare even before the sun is placed, then at no size
	if (sun == nullptr || body == nullptr || (!sun->placement.has_value() && !_warmingUp))
	{
		return;
	}
	// Inside the temple, or when a script hides the sun, the sun has no glare, and the glare does not change
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	if (!_warmingUp && (inTemple || Locator::skySystem::value().IsSunHidden()))
	{
		return;
	}
	const auto& placement = sun->placement;
	const auto model = placement.has_value() ? SunModel(placement->position) : glm::mat4(0.0f);

	// Each sample of the sun hidden from the camera dims the glare by a fifth. The land hides it where the line from the
	// camera to it meets the land. At some detail levels, what is drawn at its pixel also hides it, when it is nearer than
	// the depth that the game's depth buffer counts as hidden. The drawing answers that a frame or two later.
	int hidden = 0;
	const auto origin = camera.GetOrigin();
	const float near = camera.GetNearClip();
	const auto samples = sun::GlareSamples(placement.has_value() ? placement->position : origin, origin, near);
	const bool asksDrawing = sun::GlareTestsDepth(Locator::config::value().detailLevel);
	const auto resolution =
	    Locator::windowing::has_value() ? static_cast<glm::vec2>(Locator::windowing::value().GetSize()) : glm::vec2(0.0f);
	const auto toClip = camera.GetViewProjectionMatrix();
	// A warm-up does not look at the samples: it asks nothing of the drawing, as the next frames read the answers. It
	// draws one square of no size with no query, as a sample draws its square.
	if (_warmingUp && asksDrawing)
	{
		AskGlareSampleDrawn(camera, BGFX_INVALID_HANDLE, glm::vec2(0.0f), glm::vec2(0.0f), sun::GlareHidingDepth(near));
	}
	for (size_t i = 0; i < samples.size() && !_warmingUp; ++i)
	{
		bool isHidden = Locator::pickingSystem::has_value() &&
		                Locator::pickingSystem::value().LandOrSeaAlong(origin, samples[i], origin).has_value();
		// The sample's pixel, when it is in front of the near plane and on the screen
		const auto clip = toClip * glm::vec4(samples[i], 1.0f);
		std::optional<glm::vec2> pixel;
		if (!(clip.w < near) && resolution.x > 0.0f)
		{
			const auto x = static_cast<int32_t>((clip.x / clip.w + 1.0f) * resolution.x * 0.5f);
			const auto y = static_cast<int32_t>((1.0f - clip.y / clip.w) * resolution.y * 0.5f);
			if (x >= 0 && y >= 0 && x < static_cast<int32_t>(resolution.x) && y < static_cast<int32_t>(resolution.y))
			{
				pixel = glm::vec2(static_cast<float>(x), static_cast<float>(y));
			}
		}
		if (asksDrawing)
		{
			auto& query = _glareQueries.at(i);
			if (!isHidden && pixel.has_value() && bgfx::isValid(query) &&
			    bgfx::getResult(query) == bgfx::OcclusionQueryResult::Invisible)
			{
				isHidden = true;
			}
			if (pixel.has_value())
			{
				if (!bgfx::isValid(query))
				{
					query = bgfx::createOcclusionQuery();
				}
				AskGlareSampleDrawn(camera, query, *pixel / resolution, (*pixel + 1.0f) / resolution,
				                    sun::GlareHidingDepth(near));
			}
		}
		hidden += isHidden ? 1 : 0;
	}
	// A warm-up leaves the glare as it is, and draws it even when it does not show
	if (!_warmingUp)
	{
		const auto frameMilliseconds = static_cast<uint32_t>(Locator::time::value().GetFrameGameTime().count());
		_sunGlare = sun::EaseGlare(_sunGlare, hidden, frameMilliseconds);
		if (_sunGlare <= 0.0f)
		{
			return;
		}
	}
	// Larger, in an orange colour, over everything in the view. The overcast dims it as it dims the sun.
	float alpha = 0.0f;
	if (placement.has_value())
	{
		const auto* dome =
		    Locator::entitiesRegistry::value().TryGet<ecs::components::SkyDome>(Locator::skySystem::value().GetDome());
		alpha = sun::GlareAlpha(_sunGlare, placement->alpha, dome != nullptr ? dome->overcast : 0.0f,
		                        Locator::config::value().detail.fog);
	}
	// The game draws the glare when the frame ends, after all the things that blend, such as the clouds. Thus it goes
	// in the pass of the things that blend, with the smallest sort key, which puts it last in that pass.
	DrawCelestialMesh(TranslucentView(RenderPass::Main),
	                  {
	                      .meshId = body->meshId,
	                      .textureId = body->textureId,
	                      .alphaTextureId = body->alphaTextureId,
	                      .model = model * glm::scale(glm::vec3(sun::k_GlareScale)),
	                      .colour = {0xA0 / 255.0f, 0x6A / 255.0f, 0x35 / 255.0f, alpha / 255.0f},
	                      .celestial = glm::vec4(0.0f),
	                      .state = k_AdditiveState,
	                  });
}

void Renderer::AskGlareSampleDrawn(const Camera& camera, bgfx::OcclusionQueryHandle query, glm::vec2 topLeft,
                                   glm::vec2 bottomRight, float depth) const
{
	// A square over the sample's pixel at the depth that counts as hidden, drawn after the scene against its depth and
	// writing nothing: none of it shows when the scene there is nearer
	struct Corner
	{
		glm::vec4 position;
		glm::vec4 colour;
	};
	// The corners' layout never changes, so it is made once
	static const auto layout = [] {
		bgfx::VertexLayout made;
		made.begin()
		    .add(bgfx::Attrib::Position, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Float)
		    .end();
		return made;
	}();
	if (bgfx::getAvailTransientVertexBuffer(6, layout) < 6)
	{
		return;
	}
	const auto at = [&camera, depth](glm::vec2 screen) {
		const auto [eye, nearPoint] = camera.OnNearPlane(screen);
		return glm::vec4(eye + (nearPoint - eye) * (depth / camera.GetNearClip()), 1.0f);
	};
	const auto a = at(topLeft);
	const auto b = at({bottomRight.x, topLeft.y});
	const auto c = at(bottomRight);
	const auto d = at({topLeft.x, bottomRight.y});
	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, 6, layout);
	auto* corners = reinterpret_cast<Corner*>(buffer.data);
	const glm::vec4 colour(1.0f);
	corners[0] = {a, colour};
	corners[1] = {b, colour};
	corners[2] = {c, colour};
	corners[3] = {a, colour};
	corners[4] = {c, colour};
	corners[5] = {d, colour};
	bgfx::setVertexBuffer(0, &buffer);
	bgfx::setState(BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA);
	const auto* program = _shaderManager->GetShader("DebugLine");
	bgfx::submit(static_cast<bgfx::ViewId>(TranslucentView(RenderPass::Main)), toBgfx(program->GetRawHandle()), query);
}

void Renderer::DrawSkyDomePass(const DrawSceneDesc& drawDesc) const
{
	if (!drawDesc.drawSky || !Locator::skySystem::has_value())
	{
		return;
	}
	const auto* dome =
	    Locator::entitiesRegistry::value().TryGet<const ecs::components::SkyDome>(Locator::skySystem::value().GetDome());
	const bool lowResolution = Locator::config::value().detail.lowResTextures;
	// A warm-up makes the target and draws a band of no rows when the dome has none this frame: it changes no texel. The
	// low resolution dome is made once and stays.
	// After a restore of the device the dome's picture is gone. The renderer blends the rows of the picture again at their
	// own sky types, and then the rows of this frame.
	const bool lowDomeMade = lowResolution && _skyDomeFrameBuffer && _skyDomeRows == sky_dome::k_LowRows && !_skyDomeLost;
	const auto frameRows = dome != nullptr ? dome->frameRows.Get() : std::span<const sky_dome::Rows> {};
	std::vector<sky_dome::Rows> again;
	if (dome != nullptr && _skyDomeLost)
	{
		again = _skyDomePicture.Again();
		again.insert(again.end(), frameRows.begin(), frameRows.end());
	}
	const auto rowsToDraw = _skyDomeLost ? std::span<const sky_dome::Rows>(again) : frameRows;
	if (dome == nullptr || (!lowResolution && rowsToDraw.empty() && !_warmingUp) || (lowDomeMade && !_warmingUp))
	{
		return;
	}
	const auto* pictures = Locator::resources::value().GetTextures().Find(dome->textureId);
	if (pictures == nullptr)
	{
		return;
	}
	constexpr uint16_t k_Alignments = 3;
	const uint16_t domeRows = lowResolution ? sky_dome::k_LowRows : sky_dome::k_Rows;
	const glm::u16vec2 size {domeRows, static_cast<uint16_t>(domeRows * k_Alignments)};
	if (!_skyDomeFrameBuffer || _skyDomeRows != domeRows)
	{
		_skyDomeFrameBuffer = std::make_unique<FrameBuffer>("SkyDome", size.x, size.y, graphics::TextureFormat::RGBA8,
		                                                    std::nullopt, 1, Wrapping::ClampEdge);
		_skyDomeRows = domeRows;
		_skyDomePicture = {};
	}
	_skyDomeLost = false;
	const auto viewId = SetUpLandView(RenderPass::SkyDome, *_skyDomeFrameBuffer, size);
	const auto& program = *_shaderManager->GetShader("SkyDome");
	const auto rowsAcross = static_cast<float>(domeRows);
	if (lowResolution)
	{
		// Each alignment's day picture, every second texel, never blended for the time of day
		for (uint16_t alignment = 0; alignment < k_Alignments; ++alignment)
		{
			const glm::vec4 u_skyDome {alignment * 3, sky_dome::k_LowTime, sky_dome::k_LowTime, -1.0f};
			program.SetTextureSampler("s_diffuse", 0, *pictures);
			program.SetUniformValue("u_skyDome", &u_skyDome);
			const auto top = static_cast<float>(alignment) * rowsAcross;
			SubmitLandQuad(viewId, program, {0.0f, top}, {rowsAcross, top + rowsAcross}, {0.0f, 0.0f}, {1.0f, 1.0f},
			               BGFX_STATE_WRITE_RGB);
		}
		return;
	}
	if (rowsToDraw.empty())
	{
		program.SetTextureSampler("s_diffuse", 0, *pictures);
		SubmitLandQuad(viewId, program, {0.0f, 0.0f}, {rowsAcross, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f}, BGFX_STATE_WRITE_RGB);
	}
	_skyDomePicture.Drew(rowsToDraw);
	for (const auto& rows : rowsToDraw)
	{
		const auto times = sky_dome::TimePair(rows.skyType);
		const auto first = static_cast<float>(rows.first);
		const auto last = static_cast<float>(rows.first + rows.count);
		for (uint16_t alignment = 0; alignment < k_Alignments; ++alignment)
		{
			// The pictures are laid out a layer for each time of day within each alignment
			const glm::vec4 u_skyDome {alignment * 3, times.lower, times.upper, times.weight};
			program.SetTextureSampler("s_diffuse", 0, *pictures);
			program.SetUniformValue("u_skyDome", &u_skyDome);
			const auto top = static_cast<float>(alignment) * rowsAcross;
			SubmitLandQuad(viewId, program, {0.0f, top + first}, {rowsAcross, top + last}, {0.0f, first / rowsAcross},
			               {1.0f, last / rowsAcross}, BGFX_STATE_WRITE_RGB);
		}
	}
}
