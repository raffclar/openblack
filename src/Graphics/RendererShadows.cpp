/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the shadows of the sun and the moon. The original game bakes its shadows for a sun that
// does not move; with the experiment on, the shadows follow the sun and the moon.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>

#include <bgfx/bgfx.h>
#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "3D/OceanInterface.h"
#include "Camera/Camera.h"
#include "ECS/Systems/UnderwaterSystemInterface.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/Mesh.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/SunShadows.h"
#include "Graphics/Underwater.h"
#include "Graphics/VertexBuffer.h"
#include "Locator.h"
#include "Renderer.h"

using namespace openblack;
using namespace openblack::graphics;
namespace shadows = openblack::graphics::sun_shadows;

namespace
{
/// How dark a full shadow of a thing makes the scene in the sun, and in the moon's light
constexpr float k_SunDarkness = 0.42f;
constexpr float k_MoonDarkness = 0.25f;
/// How dark the hills' shadow makes the land and the water
constexpr float k_HillDarkness = 0.75f;

/// A camera that looks from the light at a cascade of its shadows
class LightCamera final: public Camera
{
public:
	LightCamera(const shadows::LightView& view, glm::vec3 eye, glm::vec3 middle)
	    : Camera(middle)
	    , _view(view.view)
	{
		SetOrigin(eye);
		SetFocus(middle);
		_projectionMatrix = view.projection;
		_projectionMatrixReversedZ = view.projection;
	}

	[[nodiscard]] glm::mat4 GetViewMatrix(Interpolation /*interpolation*/) const override { return _view; }

private:
	glm::mat4 _view;
};

/// Binds the sea's triangle over the whole screen for the next draw
void BindScreenTriangle()
{
	const auto& mesh = Locator::oceanSystem::value().GetMesh();
	mesh.GetIndexBuffer().Bind(mesh.GetIndexBuffer().GetCount(), 0);
	mesh.GetVertexBuffer().Bind();
}
} // namespace

void Renderer::BeginShadows(const DrawSceneDesc& desc) const
{
	_shadowsFrame.reset();
	if (!_underwaterFrame.has_value() || !Locator::underwaterSystem::has_value() || !Locator::oceanSystem::has_value() ||
	    _shaderManager->GetShader("ShadowApply") == nullptr)
	{
		return;
	}
	const auto& system = Locator::underwaterSystem::value();
	if (system.GetEffectOff() == underwater::Effect::Shadows)
	{
		return;
	}
	const auto& frame = *_underwaterFrame;
	if (frame.lightColour.w < 0.5f || frame.above.y <= 0.0f)
	{
		return;
	}
	if (_sunShadows == nullptr)
	{
		_sunShadows = std::make_unique<SunShadowView>();
	}
	const glm::vec3 light = glm::normalize(glm::vec3(frame.above));
	const glm::vec3 focus = Locator::camera::has_value() ? Locator::camera::value().GetFocus() : desc.camera->GetFocus();
	const auto clock = _sunShadows->Clock().Advance(system.GetSeconds(), light, {focus.x, focus.z});
	// The things' shadows are as dark as the light is strong, and softer in the moon's light
	const float strength = std::clamp(glm::length(glm::vec3(frame.lightColour)) / 1.2f, 0.0f, 1.0f);
	const float darkness = glm::mix(k_SunDarkness, k_MoonDarkness, frame.light.w) * strength;
	const bool homogeneous = bgfx::getCaps()->homogeneousDepth;
	if (clock.update)
	{
		for (uint8_t cascade = 0; cascade < 2; ++cascade)
		{
			const auto view = shadows::CascadeView(light, focus, shadows::k_CascadeWidths.at(cascade), homogeneous);
			_sunShadows->environmentMatrices.at(clock.newest).at(cascade) = view.projection * view.view;
		}
	}
	const auto moving = shadows::CascadeView(light, focus, shadows::k_CascadeWidths.front(), homogeneous);
	_sunShadows->movingMatrix = moving.projection * moving.view;
	_shadowsFrame = SunShadowFrame {.clock = clock, .light = light, .focus = focus, .darkness = darkness};
}

void Renderer::DrawSunShadows(const DrawSceneDesc& desc, const FrameBuffer& sceneTarget) const
{
	if (!_shadowsFrame.has_value() || _sunShadows == nullptr || _underwater == nullptr ||
	    !Locator::underwaterSystem::has_value())
	{
		return;
	}
	const auto* visibilityProgram = _shaderManager->GetShader("SunVisibility");
	const auto* blendProgram = _shaderManager->GetShader("SunVisibilityBlend");
	const auto* applyProgram = _shaderManager->GetShader("ShadowApply");
	if (visibilityProgram == nullptr || blendProgram == nullptr || applyProgram == nullptr)
	{
		return;
	}
	const auto& shadowFrame = *_shadowsFrame;
	const auto& clock = shadowFrame.clock;
	const auto& system = Locator::underwaterSystem::value();
	auto& view = *_sunShadows;
	constexpr auto k_Visibility = shadows::k_VisibilityTexels;

	// The map of the sun's visibility from the height map, when the light has moved
	if (clock.update)
	{
		const auto pass = static_cast<bgfx::ViewId>(RenderPass::SunVisibility);
		bgfx::setViewFrameBuffer(pass, view.VisibilityTarget(clock.newest));
		bgfx::setViewRect(pass, 0, 0, k_Visibility, k_Visibility);
		bgfx::setViewClear(pass, BGFX_CLEAR_NONE);
		visibilityProgram->SetTextureSampler("s_shore", 0, _underwater->Seabed(system.GetSeabed(), system.GetSeabedRevision()));
		const glm::vec4 sunLight {shadowFrame.light, _underwaterFrame->params.z};
		visibilityProgram->SetUniformValue("u_sunLight", &sunLight);
		BindScreenTriangle();
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
		visibilityProgram->Submit(pass);
	}
	// The old map blends into the new
	{
		const auto pass = static_cast<bgfx::ViewId>(RenderPass::SunVisibilityBlend);
		bgfx::setViewFrameBuffer(pass, view.VisibilityTarget(2));
		bgfx::setViewRect(pass, 0, 0, k_Visibility, k_Visibility);
		bgfx::setViewClear(pass, BGFX_CLEAR_NONE);
		blendProgram->SetTextureSampler("s_visibilityOld", 0, view.Visibility(static_cast<uint8_t>(1 - clock.newest)));
		blendProgram->SetTextureSampler("s_visibilityNew", 1, view.Visibility(clock.newest));
		const glm::vec4 blend {clock.blend, 0.0f, 0.0f, 0.0f};
		blendProgram->SetUniformValue("u_sunBlend", &blend);
		BindScreenTriangle();
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
		blendProgram->Submit(pass);
		_sunVisibilityReady = true;
	}

	// The light's maps: the environment's two cascades when they are made again, and the moving things each frame
	const auto drawFromLight = [&](RenderPass pass, bgfx::FrameBufferHandle target, float width, bool vegetation) {
		const auto id = static_cast<bgfx::ViewId>(pass);
		bgfx::setViewFrameBuffer(id, target);
		bgfx::setViewRect(id, 0, 0, shadows::k_MapTexels, shadows::k_MapTexels);
		bgfx::setViewClear(id, BGFX_CLEAR_DEPTH, 0, 0.0f, 0);
		const auto lightView =
		    shadows::CascadeView(shadowFrame.light, shadowFrame.focus, width, bgfx::getCaps()->homogeneousDepth);
		const LightCamera camera(lightView, shadowFrame.focus + shadowFrame.light * shadows::k_LightDistance,
		                         shadowFrame.focus);
		DrawSceneDesc lightDesc = desc;
		lightDesc.viewId = pass;
		lightDesc.camera = &camera;
		lightDesc.frameBuffer = nullptr;
		lightDesc.drawSky = false;
		lightDesc.drawWater = false;
		lightDesc.drawIsland = false;
		lightDesc.drawSprites = false;
		lightDesc.drawBoundingBoxes = false;
		lightDesc.drawVegetation = vegetation;
		lightDesc.drawHand = !vegetation;
		lightDesc.wireframe = false;
		lightDesc.cullBack = false;
		lightDesc.smallBumpMapStrength = 0.0f;
		DrawPass(lightDesc);
	};
	if (clock.update)
	{
		for (uint8_t cascade = 0; cascade < 2; ++cascade)
		{
			const auto pass = cascade == 0 ? RenderPass::SunShadowEnvironment0 : RenderPass::SunShadowEnvironment1;
			drawFromLight(pass, view.EnvironmentTarget(clock.newest, cascade), shadows::k_CascadeWidths.at(cascade), true);
		}
		view.environmentReady.at(clock.newest) = true;
	}
	drawFromLight(RenderPass::SunShadowMoving, view.MovingTarget(), shadows::k_CascadeWidths.front(), false);

	// The things' shadows over the scene's colour
	const auto* stats = bgfx::getStats();
	const glm::u16vec2 screen {stats->width, stats->height};
	const auto pass = static_cast<bgfx::ViewId>(RenderPass::ShadowApply);
	bgfx::setViewFrameBuffer(pass, view.SceneColour(sceneTarget.GetColorAttachment().GetNativeHandle(), screen));
	bgfx::setViewRect(pass, 0, 0, screen.x, screen.y);
	bgfx::setViewClear(pass, BGFX_CLEAR_NONE);
	_shaderManager->SetCamera(RenderPass::ShadowApply, *desc.camera);
	const uint8_t newest = clock.newest;
	const auto older = static_cast<uint8_t>(1 - newest);
	const bool olderReady = view.environmentReady.at(older);
	applyProgram->SetTextureSampler("s_sceneDepth", 0, sceneTarget.GetDepthStencilAttachment());
	applyProgram->SetTextureSampler("s_shadowOld0", 1, view.Environment(olderReady ? older : newest, 0));
	applyProgram->SetTextureSampler("s_shadowOld1", 2, view.Environment(olderReady ? older : newest, 1));
	applyProgram->SetTextureSampler("s_shadowNew0", 3, view.Environment(newest, 0));
	applyProgram->SetTextureSampler("s_shadowNew1", 4, view.Environment(newest, 1));
	applyProgram->SetTextureSampler("s_shadowMoving", 5, view.Moving());
	applyProgram->SetUniformValue("u_shadowOld0", &view.environmentMatrices.at(older).at(0));
	applyProgram->SetUniformValue("u_shadowOld1", &view.environmentMatrices.at(older).at(1));
	applyProgram->SetUniformValue("u_shadowNew0", &view.environmentMatrices.at(newest).at(0));
	applyProgram->SetUniformValue("u_shadowNew1", &view.environmentMatrices.at(newest).at(1));
	applyProgram->SetUniformValue("u_shadowMoving", &view.movingMatrix);
	const glm::vec4 params {clock.blend, shadowFrame.darkness, olderReady ? 1.0f : 0.0f, 1.0f};
	applyProgram->SetUniformValue("u_shadowParams", &params);
	BindScreenTriangle();
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_DST_COLOR, BGFX_STATE_BLEND_ZERO));
	applyProgram->Submit(pass);
}

void Renderer::BindSunVisibility(const ShaderProgram& program, uint8_t stage) const
{
	if (!_shadowsFrame.has_value() || _sunShadows == nullptr || !_sunVisibilityReady)
	{
		const glm::vec4 off {0.0f};
		program.SetUniformValue("u_sunVisibility", &off);
		return;
	}
	const glm::vec4 on {1.0f, k_HillDarkness, 0.0f, 0.0f};
	program.SetUniformValue("u_sunVisibility", &on);
	program.SetTextureSampler("s_sunVisibility", stage, _sunShadows->Visibility(2));
}
