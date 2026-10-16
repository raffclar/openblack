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

#include <cmath>
#include <cstdint>

#include <array>
#include <optional>
#include <vector>

#include <bgfx/bgfx.h>
#include <glm/common.hpp>
#include <glm/ext/matrix_transform.hpp>
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
	const auto& system = Locator::underwaterSystem::value();
	const float seaLevel = system.GetSeaLevel();
	const float eyeDepth = underwater::EyeDepth(eye.y, seaLevel);
	const auto scatter = underwater::AmbientAtDepth(underwater::ScatterColour(skyColour, light), eyeDepth);
	const float time = system.GetSeconds();
	const float lightShows = glm::length(light.colour) > 0.0f ? 1.0f : 0.0f;
	_underwaterFrame = UnderwaterUniforms {
	    .params = {1.0f, time, seaLevel, eyeDepth},
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
	frame.eyeNearOrUnder = underwater::IsActive(eye.y, seaLevel);
	// The eye's w says whether the eye is under the surface
	frame.eye.w = eye.y < seaLevel ? 1.0f : 0.0f;
	const auto& waves = system.GetWaves();
	frame.waves = {off == Effect::Waves ? 0.0f : waves.scale, waves.storm, waves.windAngle,
	               off == Effect::Waves ? 0.0f : waves.swash};
	frame.flags2 = {on(Effect::Whitecaps), on(Effect::Reef), 0.0f, off == Effect::WaterMask ? 1.0f : 0.0f};
	frame.flags3 = {on(Effect::WetRock), on(Effect::SprayMist), 0.0f, 0.0f};
	auto tableWaves = waves;
	tableWaves.scale = off == Effect::Waves ? 0.0f : waves.scale;
	frame.waveTable = underwater::WaveTable(tableWaves);
	if (off == Effect::Waves)
	{
		for (auto& wave : frame.waveTable)
		{
			wave.z = 0.0f;
		}
	}
	// The far sea: the waves filtered for each pixel, the long waves and the reflection of the far scene
	const float pixelAngle = 2.0f * std::tan(desc.camera->GetVerticalFieldOfView() * 0.5f) / static_cast<float>(size.y);
	frame.sea = {on(Effect::Filtering), on(Effect::FarWaves), pixelAngle, 0.0f};
	frame.farWaves = underwater::FarWaveTable(tableWaves);
	if (off == Effect::FarWaves || off == Effect::Waves)
	{
		for (auto& wave : frame.farWaves)
		{
			wave.z = 0.0f;
		}
	}
	const auto& current = system.GetCurrent();
	const bool still = off == Effect::Current;
	frame.current = still ? glm::vec4(0.0f) : glm::vec4(current.flow, current.swirl, current.gust);
	const auto floaters = system.GetFloaters();
	for (size_t i = 0; i < frame.floaters.size(); ++i)
	{
		frame.floaters.at(i) = i < floaters.size() ? glm::vec4(floaters[i], 1.0f) : glm::vec4(0.0f);
	}

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
		program.SetUniformValue("u_uwWaves", &frame.waves);
		program.SetUniformValue("u_uwFlags2", &frame.flags2);
		program.SetUniformValue("u_uwFlags3", &frame.flags3);
		program.SetUniformValue("u_uwCurrent", &frame.current);
	};

	// The scene through the water: one triangle over the screen, the sea's own
	const auto* post = _shaderManager->GetShader("UnderwaterPost");
	const auto& mesh = Locator::oceanSystem::value().GetMesh();
	mesh.GetIndexBuffer().Bind(mesh.GetIndexBuffer().GetCount(), 0);
	mesh.GetVertexBuffer().Bind();
	setUniforms(*post);
	post->SetUniformValue("u_uwSky", &frame.sky);
	post->SetUniformArray("u_uwWaveTable", frame.waveTable.data(), static_cast<uint16_t>(frame.waveTable.size()));
	post->SetUniformArray("u_uwFloaters", frame.floaters.data(), static_cast<uint16_t>(frame.floaters.size()));
	post->SetUniformValue("u_uwSea", &frame.sea);
	post->SetUniformValue("u_uwEnvironment", &frame.environment);
	post->SetUniformArray("u_uwFarWaves", frame.farWaves.data(), static_cast<uint16_t>(frame.farWaves.size()));
	if (frame.sea.w > 0.5f)
	{
		post->SetTextureSampler("s_environment", 5, _underwater->EnvironmentColour());
		post->SetTextureSampler("s_environmentDepth", 6, _underwater->EnvironmentDepth());
		post->SetTextureSampler("s_environmentSky", 7, _underwater->EnvironmentSky());
	}
	post->SetTextureSampler("s_sceneColour", 0, target.GetColorAttachment());
	post->SetTextureSampler("s_sceneDepth", 1, target.GetDepthStencilAttachment());
	const auto& system = Locator::underwaterSystem::value();
	const auto& shoreMap = _underwater->Seabed(system.GetSeabed(), system.GetSeabedRevision());
	post->SetTextureSampler("s_shore", 2, shoreMap);
	// The original game's coast alpha, in the blocks' textures: the water shows only as much as the original sea
	if (Locator::terrainSystem::has_value())
	{
		const auto& island = Locator::terrainSystem::value();
		std::array<float, 32 * 32> layers {};
		const auto& blocks = island.GetBlocks();
		for (size_t i = 0; i < blocks.size(); ++i)
		{
			const auto at = blocks[i].GetBlockPosition();
			if (at.x >= 0 && at.y >= 0 && at.x < 32 && at.y < 32)
			{
				layers.at(static_cast<size_t>(at.y * 32 + at.x)) = static_cast<float>(i + 1);
			}
		}
		post->SetTextureSampler("s_blockTextures", 3, island.GetBlockTextures());
		post->SetTextureSampler("s_blockLayers", 4, _underwater->BlockLayers(layers, system.GetSeabedRevision()));
	}
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
		// The bits of debris among them, blended over the scene
		_underwater->BindMotes();
		setUniforms(*motes);
		const glm::vec4 debris {frame.flags2.x, frame.flags2.y, 1.0f, 0.0f};
		motes->SetUniformValue("u_uwFlags2", &debris);
		motes->SetTextureSampler("s_sceneDepth", 1, target.GetDepthStencilAttachment());
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA));
		motes->Submit(postView);
	}

	// The spray at the foot of the cliffs, above and under the water
	if (const auto* spray = _shaderManager->GetShader("UnderwaterSpray");
	    spray != nullptr && frame.flags.y > 0.5f && system.GetEffectOff() != underwater::Effect::Spray &&
	    _underwater->BindSpray(system.GetSeabed().cliffFoot, system.GetSeabedRevision()))
	{
		setUniforms(*spray);
		spray->SetUniformValue("u_uwSky", &frame.sky);
		spray->SetTextureSampler("s_sceneDepth", 1, target.GetDepthStencilAttachment());
		spray->SetTextureSampler("s_shore", 2, shoreMap);
		spray->SetTextureSampler("s_sprayNoise", 3, _underwater->SprayNoise());
		spray->SetUniformArray("u_uwWaveTable", frame.waveTable.data(), static_cast<uint16_t>(frame.waveTable.size()));
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA));
		spray->Submit(postView);
	}
	// The waterspouts over the open sea in a strong storm
	if (const auto* spout = _shaderManager->GetShader("UnderwaterSpout");
	    spout != nullptr && system.GetEffectOff() != underwater::Effect::Waterspouts)
	{
		const auto spouts = system.GetWaterspouts();
		std::vector<glm::vec4> instances;
		instances.reserve(spouts.size());
		for (size_t i = 0; i < spouts.size(); ++i)
		{
			instances.emplace_back(spouts[i].position, spouts[i].strength, static_cast<float>(i));
		}
		if (_underwater->BindWaterspouts(instances))
		{
			setUniforms(*spout);
			spout->SetUniformValue("u_uwSky", &frame.sky);
			spout->SetTextureSampler("s_sceneDepth", 1, target.GetDepthStencilAttachment());
			spout->SetTextureSampler("s_sprayNoise", 3, _underwater->SprayNoise());
			bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA));
			spout->Submit(postView);
		}
	}
	// The air under the water at the foot of the cliffs
	if (const auto* bubbles = _shaderManager->GetShader("UnderwaterBubbles");
	    bubbles != nullptr && system.GetEffectOff() != underwater::Effect::Bubbles &&
	    _underwater->BindBubbles(system.GetSeabed().cliffFoot, system.GetSeabedRevision()))
	{
		setUniforms(*bubbles);
		bubbles->SetTextureSampler("s_sceneDepth", 1, target.GetDepthStencilAttachment());
		bubbles->SetTextureSampler("s_shore", 2, shoreMap);
		bubbles->SetTextureSampler("s_sprayNoise", 3, _underwater->SprayNoise());
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA));
		bubbles->Submit(postView);
	}
}

namespace
{
/// A camera that looks along one face of the cube of the far scene, from the cube's middle
class EnvironmentCamera final: public Camera
{
public:
	EnvironmentCamera(glm::vec3 centre, uint8_t face)
	    : Camera(centre + underwater::EnvironmentFaceAxesOf(face).forward * 10.0f)
	{
		const auto axes = underwater::EnvironmentFaceAxesOf(face);
		_view = glm::lookAt(centre, centre + axes.forward, axes.up);
		SetOrigin(centre);
		SetFocus(centre + axes.forward * 10.0f);
		// A quarter turn across each face, so that the six faces meet with no gap
		constexpr float k_FaceDegrees = 90.0f;
		constexpr float k_Near = 1.0f;
		SetProjectionMatrixPerspective(k_FaceDegrees, 1.0f, k_Near);
	}

	[[nodiscard]] glm::mat4 GetViewMatrix(Interpolation /*interpolation*/) const override { return _view; }

private:
	glm::mat4 _view {1.0f};
};
} // namespace

void Renderer::DrawUnderwaterEnvironment(const DrawSceneDesc& desc) const
{
	if (!_underwaterFrame.has_value() || _underwater == nullptr || !Locator::underwaterSystem::has_value() ||
	    Locator::underwaterSystem::value().GetEffectOff() == underwater::Effect::Environment)
	{
		return;
	}
	auto& frame = *_underwaterFrame;
	const auto centre = underwater::EnvironmentCentre(glm::vec3(frame.eye), frame.params.z);
	const uint8_t face = _underwater->NextEnvironmentFace();
	const auto target = _underwater->EnvironmentFace(face);
	const EnvironmentCamera camera(centre, face);

	// The views of the cube: its sky first, cleared to the colour of the haze, then the scene over a copy of the sky
	constexpr auto k_Size = underwater::k_EnvironmentSize;
	const auto skyView = static_cast<bgfx::ViewId>(RenderPass::UnderwaterEnvironmentSky);
	const auto sceneView = static_cast<bgfx::ViewId>(RenderPass::UnderwaterEnvironment);
	bgfx::setViewFrameBuffer(skyView, _underwater->EnvironmentSkyFace(face));
	bgfx::setViewFrameBuffer(sceneView, target);
	for (const auto id : {skyView, sceneView})
	{
		bgfx::setViewRect(id, 0, 0, k_Size, k_Size);
	}
	// The sky goes into a cube of its own. The view of the scene copies it as the background of the scene.
	bgfx::blit(sceneView, _underwater->EnvironmentColourTexture(), 0, 0, 0, face, _underwater->EnvironmentSkyTexture(), 0, 0, 0,
	           face, k_Size, k_Size, 1);
	const auto channel = [&frame](int i) {
		return static_cast<uint32_t>(glm::clamp(frame.sky[i], 0.0f, 1.0f) * 255.0f + 0.5f);
	};
	const uint32_t clear = (channel(0) << 24u) | (channel(1) << 16u) | (channel(2) << 8u) | 0xffu;
	bgfx::setViewClear(static_cast<bgfx::ViewId>(RenderPass::UnderwaterEnvironmentSky), BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
	                   clear, 0.0f, 0);
	// The scene's view clears the depth that the sky's dome wrote: the cube's depth then gives how far away the scene is,
	// and none where the cube shows the sky
	bgfx::setViewClear(static_cast<bgfx::ViewId>(RenderPass::UnderwaterEnvironment), BGFX_CLEAR_DEPTH, 0, 0.0f, 0);

	DrawSceneDesc faceDesc = desc;
	faceDesc.viewId = RenderPass::UnderwaterEnvironment;
	faceDesc.camera = &camera;
	faceDesc.frameBuffer = nullptr;
	faceDesc.drawWater = false;
	faceDesc.drawSprites = false;
	faceDesc.drawBoundingBoxes = false;
	faceDesc.wireframe = false;
	faceDesc.drawHand = false;
	faceDesc.smallBumpMapStrength = 0.0f;
	DrawPass(faceDesc);

	frame.environment = {centre, 0.0f};
	frame.sea.w = _underwater->EnvironmentReady() ? 1.0f : 0.0f;
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
	program.SetUniformValue("u_uwWaves", &frame.waves);
	program.SetUniformValue("u_uwFlags3", &frame.flags3);
	// The snow does not show under the sea: its samplers hold the sea bed and the sand
	const glm::vec4 noSnow {0.0f};
	program.SetUniformValue("u_snow", &noSnow);
	const auto& system = Locator::underwaterSystem::value();
	program.SetTextureSampler("s_snowDepth", 11, _underwater->Seabed(system.GetSeabed(), system.GetSeabedRevision()));
	// The ground under the sea is the block textures' layer after the blocks' own
	if (Locator::terrainSystem::has_value())
	{
		const glm::vec4 seaFloor {static_cast<float>(Locator::terrainSystem::value().GetBlocks().size()), 0.0f, 0.0f, 0.0f};
		program.SetUniformValue("u_uwSeaFloor", &seaFloor);
	}
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
	program->SetUniformValue("u_uwWaves", &frame.waves);
	program->SetTextureSampler("s_sand", 0, *textures.Handle(underwater::k_SandTextureId.value()));
	program->SetTextureSampler("s_landLight", 1, GetLandLightTexture());
	// The same shore map and sea floor texture as the land's blocks, so the two match where they meet
	program->SetTextureSampler("s_shore", 2, _underwater->Seabed(system.GetSeabed(), system.GetSeabedRevision()));
	const auto& island = Locator::terrainSystem::value();
	program->SetTextureSampler("s_floor", 3, island.GetBlockTextures());
	const glm::vec4 seaFloor {static_cast<float>(island.GetBlocks().size()), 0.0f, 0.0f, 0.0f};
	program->SetUniformValue("u_uwSeaFloor", &seaFloor);
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_GREATER |
	               BGFX_STATE_MSAA);
	// After every block of the land, which is drawn the nearest first
	program->Submit(static_cast<bgfx::ViewId>(landViewId), UINT32_MAX);
}

void Renderer::DrawReef(RenderPass landViewId) const
{
	const auto* program = _shaderManager->GetShader("Reef");
	if (program == nullptr || _underwater == nullptr || !_underwaterFrame.has_value() ||
	    !Locator::underwaterSystem::has_value() || _underwaterFrame->flags2.y < 0.5f)
	{
		return;
	}
	const auto& textures = Locator::resources::value().GetTextures();
	if (!textures.Contains(underwater::k_CoralTextureId.value()))
	{
		return;
	}
	const auto& system = Locator::underwaterSystem::value();
	const auto& frame = *_underwaterFrame;
	for (const auto kind : {seabed::ReefPlant::Kind::Coral, seabed::ReefPlant::Kind::Seaweed})
	{
		if (!_underwater->BindReef(system.GetReef(), system.GetSeabedRevision(), kind))
		{
			continue;
		}
		program->SetUniformValue("u_uwParams", &frame.params);
		program->SetUniformValue("u_uwLight", &frame.light);
		program->SetUniformValue("u_uwLightColour", &frame.lightColour);
		program->SetUniformValue("u_uwAbove", &frame.above);
		program->SetUniformValue("u_uwWaves", &frame.waves);
		program->SetUniformValue("u_uwCurrent", &frame.current);
		program->SetTextureSampler("s_coral", 0, *textures.Handle(underwater::k_CoralTextureId.value()));
		program->SetTextureSampler("s_landLight", 1, GetLandLightTexture());
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_GREATER |
		               BGFX_STATE_MSAA);
		program->Submit(static_cast<bgfx::ViewId>(landViewId), UINT32_MAX);
	}
}
