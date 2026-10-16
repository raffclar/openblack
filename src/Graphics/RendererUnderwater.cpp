/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the view under the sea. The original game has no view under the sea.

#define LOCATOR_IMPLEMENTATIONS

#include <array>
#include <optional>

#include <bgfx/bgfx.h>
#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>

#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "3D/OceanInterface.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "ECS/Components/Sky.h"
#include "ECS/Registry.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/UnderwaterSystemInterface.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/Mesh.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/Underwater.h"
#include "Graphics/VertexBuffer.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
/// The views that draw the main scene, which go into the underwater target while the view is under the sea
constexpr std::array k_SceneViews {RenderPass::Sky, RenderPass::Land, RenderPass::Main, RenderPass::Translucent};
/// The views drawn over the scene into the window, which the main scene's target must not take
constexpr std::array k_OverViews {RenderPass::GestureHintCollar0, RenderPass::GestureHintCollar1,
                                  RenderPass::GestureHintCollar2, RenderPass::GestureHints,
                                  RenderPass::CinemaBars,         RenderPass::Advisors};

/// The sun's and the moon's light on the sea this frame, from where they stand and how strongly they show
underwater::WaterLight SkyLights(const glm::vec3& eye)
{
	if (!Locator::skySystem::has_value())
	{
		return {};
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& sky = Locator::skySystem::value();
	std::optional<underwater::SkyLight> sunLight;
	if (const auto* sun = registry.TryGet<const ecs::components::Sun>(sky.GetSun());
	    sun != nullptr && sun->placement.has_value())
	{
		// The sun's colour is dark, as the game blends its square over the sky: its light on the water is brighter
		constexpr float k_SunLight = 1.7f;
		sunLight = underwater::SkyLight {
		    .towards = sun->placement->position - eye, .colour = sun->colour * k_SunLight, .strength = sun->strength / 255.0f};
	}
	std::optional<underwater::SkyLight> moonLight;
	if (const auto* moon = registry.TryGet<const ecs::components::Moon>(sky.GetMoon());
	    moon != nullptr && moon->placement.has_value())
	{
		moonLight = underwater::SkyLight {.towards = moon->placement->offset,
		                                  .colour = moon->colour,
		                                  .strength = moon->strength / 255.0f * underwater::PhaseBrightness(moon->phase)};
	}
	return underwater::BlendLights(sunLight, moonLight);
}
} // namespace

bool Renderer::BeginUnderwater(DrawSceneDesc& desc) const
{
	_underwaterFrame.reset();
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	const bool enabled = Locator::underwaterSystem::has_value() && Locator::underwaterSystem::value().IsEnabled();
	const auto* postProgram = _shaderManager->GetShader("UnderwaterPost");
	const glm::vec3 eye = glm::inverse(desc.camera->GetViewMatrix(Camera::Interpolation::Current))[3];
	// The experiment's water takes the place of the sea, above the surface as well as under it
	const bool active = enabled && !inTemple && !_warmingUp && desc.viewId == RenderPass::Main && desc.drawIsland &&
	                    desc.drawWater && postProgram != nullptr && Locator::terrainSystem::has_value();
	if (!active)
	{
		return false;
	}
	if (_underwater == nullptr)
	{
		_underwater = std::make_unique<UnderwaterView>();
	}
	const auto* stats = bgfx::getStats();
	const glm::u16vec2 size {stats->width, stats->height};
	if (size.x == 0 || size.y == 0)
	{
		return false;
	}

	// The light under the water this frame, and the water's colour at the eye's depth
	const auto light = SkyLights(eye);
	const glm::vec3 skyColour = glm::vec3(_haze[1]) / 255.0f;
	const float eyeDepth = underwater::EyeDepth(eye.y);
	const auto scatter = underwater::AmbientAtDepth(underwater::ScatterColour(skyColour, light), eyeDepth);
	const float time = static_cast<float>(desc.time % 3600000u) / 1000.0f;
	const float lightShows = glm::length(light.colour) > 0.0f ? 1.0f : 0.0f;
	_underwaterFrame = UnderwaterUniforms {
	    .params = {1.0f, time, underwater::k_SeaLevel, eyeDepth},
	    .light = {light.towards, light.moonShare},
	    .lightColour = {light.colour, lightShows},
	    .scatter = {scatter, 0.0f},
	    .eye = {eye, 0.0f},
	    .sky = {skyColour, desc.camera->GetNearClip()},
	};
	// The effects, with the one turned off to measure it
	using underwater::Effect;
	const auto off = Locator::underwaterSystem::value().GetEffectOff();
	const auto on = [off](Effect effect) { return off == effect ? 0.0f : 1.0f; };
	auto& frame = *_underwaterFrame;
	frame.flags = {on(Effect::Reflections), on(Effect::Foam), on(Effect::Refraction), on(Effect::Shafts)};
	frame.above = {light.aboveTowards, on(Effect::Caustics)};
	frame.motes = off != Effect::Motes;
	frame.eyeNearOrUnder = underwater::IsActive(eye.y);
	// The eye's w says whether the eye is under the surface
	frame.eye.w = eye.y < underwater::k_SeaLevel ? 1.0f : 0.0f;

	// The scene goes into the target, with no sea and no reflection of it, and is drawn through the water after
	desc.frameBuffer = &_underwater->Target(size);
	desc.drawWater = false;
	_underwaterBound = true;
	return true;
}

void Renderer::EndUnderwater(const DrawSceneDesc& desc) const
{
	if (!_underwaterFrame.has_value() || desc.frameBuffer == nullptr)
	{
		// Back above the sea, the scene is drawn into the window again
		if (_underwaterBound)
		{
			for (const auto view : k_SceneViews)
			{
				bgfx::setViewFrameBuffer(static_cast<bgfx::ViewId>(view), BGFX_INVALID_HANDLE);
			}
			_underwaterBound = false;
		}
		return;
	}
	// What is drawn over the scene goes into the window
	for (const auto view : k_OverViews)
	{
		bgfx::setViewFrameBuffer(static_cast<bgfx::ViewId>(view), BGFX_INVALID_HANDLE);
	}
	const auto postView = static_cast<bgfx::ViewId>(RenderPass::UnderwaterPost);
	const auto* stats = bgfx::getStats();
	bgfx::setViewFrameBuffer(postView, BGFX_INVALID_HANDLE);
	bgfx::setViewRect(postView, 0, 0, stats->width, stats->height);
	bgfx::setViewClear(postView, BGFX_CLEAR_DEPTH, 0, 0.0f, 0);
	_shaderManager->SetViewOrder(RenderPass::UnderwaterPost, ViewOrder::Sequential);
	_shaderManager->SetCamera(RenderPass::UnderwaterPost, *desc.camera);

	const auto& frame = *_underwaterFrame;
	const auto& target = *desc.frameBuffer;
	const auto setUniforms = [&frame](const ShaderProgram& program) {
		program.SetUniformValue("u_uwParams", &frame.params);
		program.SetUniformValue("u_uwLight", &frame.light);
		program.SetUniformValue("u_uwLightColour", &frame.lightColour);
		program.SetUniformValue("u_uwScatter", &frame.scatter);
		program.SetUniformValue("u_uwEye", &frame.eye);
		program.SetUniformValue("u_uwFlags", &frame.flags);
		program.SetUniformValue("u_uwAbove", &frame.above);
	};

	// The scene through the water: one triangle over the screen, the sea's own
	const auto* post = _shaderManager->GetShader("UnderwaterPost");
	const auto& mesh = Locator::oceanSystem::value().GetMesh();
	mesh.GetIndexBuffer().Bind(mesh.GetIndexBuffer().GetCount(), 0);
	mesh.GetVertexBuffer().Bind();
	setUniforms(*post);
	post->SetUniformValue("u_uwSky", &frame.sky);
	post->SetTextureSampler("s_sceneColour", 0, target.GetColorAttachment());
	post->SetTextureSampler("s_sceneDepth", 1, target.GetDepthStencilAttachment());
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
	post->Submit(postView);

	// The motes in the water, added over it and hidden by the scene in front of them
	if (const auto* motes = _shaderManager->GetShader("UnderwaterMotes");
	    motes != nullptr && frame.motes && frame.eyeNearOrUnder)
	{
		_underwater->BindMotes();
		setUniforms(*motes);
		motes->SetTextureSampler("s_sceneDepth", 1, target.GetDepthStencilAttachment());
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_ADD);
		motes->Submit(postView);
	}
}

void Renderer::SetTerrainUnderwater(const ShaderProgram& program, bool active) const
{
	if (!active || !_underwaterFrame.has_value() || _underwater == nullptr || !Locator::underwaterSystem::has_value())
	{
		const glm::vec4 off {0.0f};
		program.SetUniformValue("u_uwParams", &off);
		return;
	}
	const auto& frame = *_underwaterFrame;
	program.SetUniformValue("u_uwParams", &frame.params);
	program.SetUniformValue("u_uwLight", &frame.light);
	program.SetUniformValue("u_uwLightColour", &frame.lightColour);
	program.SetUniformValue("u_uwEye", &frame.eye);
	program.SetUniformValue("u_uwAbove", &frame.above);
	// The snow does not show under the sea: its samplers hold the sea bed and the sand
	const glm::vec4 noSnow {0.0f};
	program.SetUniformValue("u_snow", &noSnow);
	const auto& system = Locator::underwaterSystem::value();
	program.SetTextureSampler("s_snowDepth", 11, _underwater->Seabed(system.GetSeabed(), system.GetSeabedRevision()));
	const auto& textures = Locator::resources::value().GetTextures();
	if (textures.Contains(underwater::k_SandTextureId.value()))
	{
		program.SetTextureSampler("s12_snowNoise", 12, *textures.Handle(underwater::k_SandTextureId.value()));
	}
}

void Renderer::DrawFarSeabed(RenderPass landViewId) const
{
	const auto* program = _shaderManager->GetShader("Seabed");
	if (program == nullptr || _underwater == nullptr || !_underwaterFrame.has_value() ||
	    !Locator::underwaterSystem::has_value() || !Locator::terrainSystem::has_value())
	{
		return;
	}
	const auto& textures = Locator::resources::value().GetTextures();
	if (!textures.Contains(underwater::k_SandTextureId.value()))
	{
		return;
	}
	// Which squares of the map have a block of the land
	constexpr size_t k_MapBlocks = 32;
	std::array<bool, k_MapBlocks * k_MapBlocks> present {};
	for (const auto& block : Locator::terrainSystem::value().GetBlocks())
	{
		const auto at = block.GetBlockPosition();
		if (at.x >= 0 && at.y >= 0 && at.x < static_cast<int>(k_MapBlocks) && at.y < static_cast<int>(k_MapBlocks))
		{
			present.at(static_cast<size_t>(at.x) * k_MapBlocks + static_cast<size_t>(at.y)) = true;
		}
	}
	const auto& system = Locator::underwaterSystem::value();
	if (!_underwater->BindFarSeabed(system.GetSeabed(), system.GetSeabedRevision(), present))
	{
		return;
	}
	const auto& frame = *_underwaterFrame;
	program->SetUniformValue("u_uwParams", &frame.params);
	program->SetUniformValue("u_uwLight", &frame.light);
	program->SetUniformValue("u_uwLightColour", &frame.lightColour);
	program->SetUniformValue("u_uwEye", &frame.eye);
	program->SetUniformValue("u_uwAbove", &frame.above);
	program->SetTextureSampler("s_sand", 0, *textures.Handle(underwater::k_SandTextureId.value()));
	program->SetTextureSampler("s_landLight", 1, GetLandLightTexture());
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_GREATER |
	               BGFX_STATE_MSAA);
	// After every block of the land, which is drawn the nearest first
	program->Submit(static_cast<bgfx::ViewId>(landViewId), UINT32_MAX);
}
