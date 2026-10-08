/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The map's mist banks (CREATE_MIST -> Mist, a mist object drawn as a camera-facing smoke dome)

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include <LNDFile.h>
#include <bgfx/bgfx.h>
#include <entt/core/hashed_string.hpp>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/matrix.hpp>

#include "3D/Billboard.h"
#include "3D/FrameAnim.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandLight.h"
#include "3D/LandLightTable.h"
#include "3D/SkyInterface.h"
#include "Camera/Camera.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Registry.h"
#include "ECS/Systems/RenderFrameSystemInterface.h"
#include "Game.h"
#include "Graphics/ArgbColour.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/Mists.h"
#include "Graphics/ModelLight.h"
#include "Graphics/RegionOnScreen.h"
#include "Graphics/RenderModes.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/Texture2D.h"
#include "Graphics/VertexBuffer.h"
#include "Graphics/ZSort.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
constexpr float k_MistSphereScale = 0.55f; ///< the visibility sphere's share of the mesh radius

/// The original's on-screen region test, shared (Graphics/RegionOnScreen.h)
using openblack::graphics::region_on_screen::SphereInView;

/// The radius of the sphere sent to the on-screen test: the mesh's radius x the size x 0.55. (approximate) the radius
/// taken as the bounding box's half diagonal (what the original's bounding box leaves there; 29.3 for the 20-unit dome
/// of mist.l3d)
float MistSphereRadius(const L3DMesh& mesh, float size)
{
	return glm::length(mesh.GetBoundingBox().Size()) * 0.5f * size * k_MistSphereScale;
}
} // namespace

void mists::Submit(const MistDesc& mist)
{
	Locator::renderFrameSystem::value().SubmitMist(mist);
}

bool mists::InView(const glm::vec3& position, float size)
{
	if (!Locator::camera::has_value() || !Locator::skySystem::has_value())
	{
		return true;
	}
	return SphereInView(Locator::camera::value().GetViewProjectionMatrix(Camera::Interpolation::Current), position,
	                    MistSphereRadius(Locator::skySystem::value().GetCloudMesh(), size));
}

std::vector<std::pair<float, uint32_t>> Renderer::CollectMists(const Camera& camera) const
{
	auto& registry = Locator::entitiesRegistry::value();
	// the frame's game time step (game_clock::FrameGameMs): game time, whole ms, 0 while paused, so the animation stops
	// while the game is paused. Collected once a frame for the main view (PreDraw)
	const bool paused = !Locator::time::has_value() || game_clock::IsPaused();
	const float milliseconds = paused ? 0.0f : static_cast<float>(game_clock::FrameGameMs());

	std::vector<std::pair<float, uint32_t>> order;
	_frameMists.clear();
	const auto submitted = Locator::renderFrameSystem::value().TakeSubmittedMists();
	const auto& mesh = Locator::skySystem::value().GetCloudMesh();
	const auto& textures = Locator::resources::value().GetTextures();
	constexpr entt::id_type k_Smoke = entt::hashed_string("raw/smoke").value();
	constexpr entt::id_type k_SmokeAlpha = entt::hashed_string("raw/smokea").value();
	if (mesh.GetNumSubMeshes() == 0 || !textures.Contains(k_Smoke) || !textures.Contains(k_SmokeAlpha))
	{
		return order;
	}
	const auto origin = camera.GetOrigin();
	// only a mist whose sphere touches the screen goes to the Z-sorter, so only that one
	// is drawn (and only its animation counter advances). The sphere is centred on the object's position, of radius
	// MistSphereRadius
	const auto viewProjection = camera.GetViewProjectionMatrix(Camera::Interpolation::Current);
	registry.Each<ecs::components::Mist, const ecs::components::Transform>(
	    [&](entt::entity entity, ecs::components::Mist& mist, const ecs::components::Transform& transform) {
		    if (!SphereInView(viewProjection, transform.position, MistSphereRadius(mesh, mist.size)))
		    {
			    return;
		    }
		    // counter += trunc(frame ms * 0.255), and the modulo only once it passes 900. The
		    // original truncates every frame and loses the fraction (4 instead of 4.08 with 16 ms frames); the
		    // fraction is kept here so that the animation does not slow down (or stop) at the uncapped frame rates of
		    // openblack (no vsync by default: under 4 ms a frame the original's step would be 0), like Clouds.cpp
		    // (frame_anim::MistAdvance)
		    frame_anim::MistClock clock {mist.counter, mist.counterRemainder};
		    frame_anim::MistAdvance(clock, milliseconds);
		    mist.counter = clock.counter;
		    mist.counterRemainder = clock.remainder;
		    // the Z-sorter key: |position - camera|^2, (x^2 + y^2) + z^2
		    order.emplace_back(zsort::Key(transform.position, origin), static_cast<uint32_t>(_frameMists.size()));
		    _frameMists.push_back({transform.position, mist.size, mist.colour, mist.edgeShrink, mist.k, mist.counter});
	    },
	    entt::exclude<ecs::components::Unavailable>);
	for (const auto& mist : submitted)
	{
		if (SphereInView(viewProjection, mist.position, MistSphereRadius(mesh, mist.size)))
		{
			order.emplace_back(zsort::Key(mist.position, origin), static_cast<uint32_t>(_frameMists.size()));
			_frameMists.push_back(mist);
		}
	}
	return order;
}

void Renderer::DrawEffectMist(graphics::RenderPass viewId, const Camera& camera, const mists::MistDesc& mist) const
{
	const auto& mesh = Locator::skySystem::value().GetCloudMesh();
	if (mesh.GetNumSubMeshes() == 0)
	{
		return;
	}
	// a mist drawn at once: the same sphere (mesh radius x size x 0.55) through the on-screen test, then the Draw. Its
	// animation counter was advanced by mist_atoms::SubmitFrame with the same test (mists::InView)
	if (!SphereInView(camera.GetViewProjectionMatrix(Camera::Interpolation::Current), mist.position,
	                  MistSphereRadius(mesh, mist.size)))
	{
		return;
	}
	DrawMist(viewId, camera, mist);
}

void Renderer::DrawMist(graphics::RenderPass viewId, const Camera& camera, uint32_t index) const
{
	if (index >= _frameMists.size())
	{
		return;
	}
	DrawMist(viewId, camera, _frameMists[index]);
}

void Renderer::DrawMist(graphics::RenderPass viewId, const Camera& camera, const mists::MistDesc& mist) const
{
	const auto& mesh = Locator::skySystem::value().GetCloudMesh();
	const auto& textures = Locator::resources::value().GetTextures();
	constexpr entt::id_type k_Smoke = entt::hashed_string("raw/smoke").value();
	constexpr entt::id_type k_SmokeAlpha = entt::hashed_string("raw/smokea").value();
	if (mesh.GetNumSubMeshes() == 0 || !textures.Contains(k_Smoke) || !textures.Contains(k_SmokeAlpha))
	{
		return;
	}
	const auto& smoke = *textures.Handle(k_Smoke);
	const auto& smokeAlpha = *textures.Handle(k_SmokeAlpha);
	const auto* program = _shaderManager->GetShader("Cloud");
	const auto origin = camera.GetOrigin();

	// Every mist object shares one matrix, built by the camera update from the world-to-camera matrix A with its columns
	// swizzled (row i = (A[3i], -A[3i+2], A[3i+1])) and then inverted in place (on both camera paths). The inverse of
	// that orthonormal matrix is its transpose, whose rows are right, -forward and up; with row vectors
	// (x' = m0 x + m3 y + m6 z) row k is the image of local axis k, so in glm it is mat3(right, -forward, up): a
	// billboard. Local X = screen right, local Y (the dome's axis) towards the camera, local Z = screen up, so the dome
	// always shows face-on as a disc of the smoke frame and is never seen edge-on (checked by emulation for several
	// cameras): billboard::MistBasis
	const auto cameraFrame = billboard::CameraFrame::From(camera);
	const auto& rotation = billboard::MistBasis(cameraFrame);
	const bool landLight = IsLandLit() && Locator::terrainSystem::has_value();
	const auto view = camera.GetViewMatrix(Camera::Interpolation::Current);
	const auto alpha = static_cast<float>(mist.colour >> 24u);
	if (alpha <= 0.0f)
	{
		return;
	}
	glm::vec3 rgb(static_cast<float>((mist.colour >> 16u) & 0xFFu), static_cast<float>((mist.colour >> 8u) & 0xFFu),
	              static_cast<float>(mist.colour & 0xFFu));
	glm::vec3 specular(0.0f);
	// the matrix cells are scaled by the size, except that with the effect flag only row 0
	// (the image of local X) keeps the size and rows 1 and 2 (local Y and Z) take the shrunk one, so the dome is
	// squashed along its own axis (depth) and screen height, not uniformly: it keeps its width
	glm::vec3 scale(mist.size);
	// the normal branch lights with whatever light is current (the frame light) and the current ambient; the effect
	// branch moves both for its draw (model_light::ScopedLight / ScopedAmbient)
	std::optional<model_light::ScopedLight> effectLight;
	std::optional<model_light::ScopedAmbient> effectAmbient;
	if (mist.edgeShrink)
	{
		// effect branch: round seen from straight below or above, k times wider than tall near the horizon; lit from
		// straight above with ambient 210, no land light, and the atlas rows 2-3 (V + 0.25, frame_anim::MistCellUv; the
		// normal branch has no such offset, so it uses rows 0-1)
		scale.y = scale.z = billboard::MistShrunkSize(mist.size, mist.k, mist.position - origin);
		// saves the current light and sets one at (0, 500000, 0), ambient 210; both go back after the draw
		effectLight.emplace(glm::vec3(0.0f, 500000.0f, 0.0f));
		effectAmbient.emplace(model_light::k_MistAmbient);
		// the effect branch leaves the object's specular as its colour was set: the draw uses it as the vertices'
		// specular, added after the texture stage (the storm clouds' glow)
		specular = glm::vec3(static_cast<float>((mist.specular >> 16u) & 0xFFu),
		                     static_cast<float>((mist.specular >> 8u) & 0xFFu), static_cast<float>(mist.specular & 0xFFu));
	}
	else if (landLight)
	{
		// land_light::At gives the land light and the cells' colour, then graphics::haze::ApplyObject (at the origin)
		// darkens that light and adds the haze colour to the colour, the object's specular
		auto sample = land_light::At(*_landLight, glm::vec2(mist.position.x, mist.position.z));
		const uint32_t hazed = haze::ApplyObject(_haze, haze::Depth(view, mist.position), sample.specular, &sample.diffuse);
		specular = glm::vec3(static_cast<float>((hazed >> 16u) & 0xFFu), static_cast<float>((hazed >> 8u) & 0xFFu),
		                     static_cast<float>(hazed & 0xFFu));
		const glm::vec3 light(static_cast<float>((sample.diffuse >> 16u) & 0xFFu),
		                      static_cast<float>((sample.diffuse >> 8u) & 0xFFu), static_cast<float>(sample.diffuse & 0xFFu));
		// the colour times that light, byte by byte (c l / 255, the colour's alpha kept), then the models'
		// light and ambient 90
		const uint32_t lit = argb_colour::MultiplyRgbKeepAlpha(
		    mist.colour,
		    argb_colour::Argb(static_cast<uint32_t>(light.r), static_cast<uint32_t>(light.g), static_cast<uint32_t>(light.b)));
		rgb = glm::vec3(static_cast<float>(argb_colour::Red(lit)), static_cast<float>(argb_colour::Green(lit)),
		                static_cast<float>(argb_colour::Blue(lit)));
	}
	// one whole cell, no blend (frame_anim::MistCell, MistCellUv)
	const auto cell = frame_anim::MistCellUv(frame_anim::MistCell(mist.counter), mist.edgeShrink);
	const glm::vec4 u_cloud(cell.x, cell.y, static_cast<float>(model_light::Ambient()) / 256.0f, 0.0f);
	const glm::vec4 u_cloudColour(rgb / 255.0f, alpha / 255.0f);
	const glm::vec4 u_cloudSpecular(specular / 255.0f, 0.0f);
	const auto model = glm::translate(mist.position) * glm::mat4(rotation) * glm::scale(scale);
	// model_light::LightInMeshSpace: the light of a vertex is the local normal against the light's
	// position brought into the mesh's own space, so a non-uniform scale tilts it
	const glm::vec4 u_cloudLight(model_light::LightInMeshSpace(model), 0.0f);
	for (const auto& subMesh : mesh.GetSubMeshes())
	{
		for (const auto& prim : subMesh->GetPrimitives())
		{
			bgfx::setTransform(&model);
			program->SetTextureSampler("s_diffuse", 0, smoke);
			program->SetTextureSampler("s_alpha", 1, smokeAlpha);
			program->SetUniformValue("u_cloud", &u_cloud);
			program->SetUniformValue("u_cloudColour", &u_cloudColour);
			program->SetUniformValue("u_cloudLight", &u_cloudLight);
			program->SetUniformValue("u_cloudSpecular", &u_cloudSpecular);
			if (subMesh->GetMesh().IsIndexed())
			{
				subMesh->GetMesh().GetIndexBuffer().Bind(prim.indicesCount, prim.indicesOffset);
			}
			subMesh->GetMesh().GetVertexBuffer().Bind();
			// the smoke material: mode 6, two-sided
			bgfx::setState(render_modes::State(render_modes::materials::k_Smoke));
			bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()));
		}
	}
}
