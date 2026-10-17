/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the haze of the air's light and the air's sky near the horizon. The original game has a
// haze by distance only.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <limits>
#include <vector>

#include <LNDFile.h>
#include <bgfx/bgfx.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec4.hpp>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/OceanInterface.h"
#include "3D/SkyDome.h"
#include "Camera/Camera.h"
#include "ECS/Components/Sky.h"
#include "ECS/Registry.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/UnderwaterSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/Mesh.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/Underwater.h"
#include "Graphics/VertexBuffer.h"
#include "Graphics/VolumetricClouds.h"
#include "Graphics/VolumetricFog.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;
namespace atmos = openblack::graphics::atmosphere;
namespace gi = openblack::graphics::global_illumination;

namespace
{
/// The cool, silver tint of the moon's light (as the clouds and the fog have it)
constexpr glm::vec3 k_MoonTint {0.72f, 0.84f, 1.1f};
/// The sky's directions that the processor sums into the sky's light
constexpr uint16_t k_SkySamples = 32;
/// The seconds between two sums of the sky's light on the processor at most, while the light moves
constexpr float k_SkySeconds = 0.5f;
/// The steps along each ray of the haze's grid, with and without the shadows
constexpr float k_FullSteps = 12.0f;
constexpr float k_ReducedSteps = 6.0f;
/// The haze is this much brighter than the original game's haze at the horizon: a bright, airy haze that lifts the far
/// land rather than greys it
constexpr float k_HazeBrightness = 1.3f;

void BindScreenTriangle()
{
	const auto& mesh = Locator::oceanSystem::value().GetMesh();
	mesh.GetIndexBuffer().Bind(mesh.GetIndexBuffer().GetCount(), 0);
	mesh.GetVertexBuffer().Bind();
}

/// Whether the sky's light must be worked out again for the light, the air and the eye's height
bool Stale(const std::optional<AtmosphereView::SkyCache>& cache, glm::vec3 towardsLight, float turbidity, float altitude,
           float seconds)
{
	if (!cache.has_value())
	{
		return true;
	}
	const bool moved = glm::dot(cache->towardsLight, towardsLight) < 0.99999f ||
	                   std::abs(cache->turbidity - turbidity) > 0.05f || std::abs(cache->altitude - altitude) > 0.05f;
	return moved && seconds - cache->seconds >= k_SkySeconds;
}
} // namespace

void Renderer::BeginAtmosphere(const DrawSceneDesc& desc) const
{
	_atmosphereFrame.reset();
	if (!_underwaterFrame.has_value() || !Locator::underwaterSystem::has_value() || !Locator::oceanSystem::has_value() ||
	    _shaderManager->GetShader("AtmosphereSkyView") == nullptr)
	{
		return;
	}
	using underwater::Effect;
	const auto& system = Locator::underwaterSystem::value();
	const auto& frame = *_underwaterFrame;
	if (_atmosphere == nullptr)
	{
		_atmosphere = std::make_unique<AtmosphereView>();
	}
	auto& view = *_atmosphere;
	const float seconds = system.GetSeconds();
	const float elapsed = view.lastSeconds.has_value() ? std::clamp(seconds - *view.lastSeconds, 0.0f, 0.25f) : 0.0f;
	view.lastSeconds = seconds;

	// The air: salt over the sea, more after rain and in a storm
	float rain = 0.0f;
	if (Locator::weatherSystem::has_value())
	{
		rain = static_cast<float>(Locator::weatherSystem::value().GetWeatherSmooth(desc.camera->GetOrigin()).rain) / 100.0f;
	}
	view.wetness = volumetric_fog::StepWetness(view.wetness, std::clamp(rain, 0.0f, 1.0f), elapsed);
	const auto air = atmos::AirFor(system.GetWaves().storm, view.wetness);
	const float seaLevel = frame.params.z;
	const glm::vec3 eye = frame.eye;
	const float altitude = atmos::AltitudeOf(eye.y - seaLevel);
	const glm::vec3 gameLight =
	    glm::length(glm::vec3(frame.above)) > 0.0f ? glm::normalize(glm::vec3(frame.above)) : glm::vec3(0.0f, 1.0f, 0.0f);
	// The air's light comes from the game's sun or moon round the horizon. The sun's height comes from the hour (see
	// atmosphere::AirSunElevation); the moon keeps its own height.
	float hour = 12.0f;
	if (Locator::skySystem::has_value())
	{
		hour = Locator::skySystem::value().GetClock().GetScriptTime();
	}
	const float elevation = glm::mix(atmos::AirSunElevation(hour), gameLight.y, frame.light.w);
	const glm::vec2 round {gameLight.x, gameLight.z};
	const glm::vec2 across = glm::length(round) > 1e-5f ? glm::normalize(round) : glm::vec2(1.0f, 0.0f);
	const float flat = std::sqrt(std::max(1.0f - elevation * elevation, 0.0f));
	const glm::vec3 towardsLight {across.x * flat, elevation, across.y * flat};

	// The sky's light on the processor, again only when the light, the air or the height has changed enough
	if (Stale(view.cache, towardsLight, air.turbidity, altitude, seconds))
	{
		view.cache = AtmosphereView::SkyCache {
		    .towardsLight = towardsLight,
		    .turbidity = air.turbidity,
		    .altitude = altitude,
		    .seconds = seconds,
		    .horizon = atmos::HorizonRadiance(towardsLight, altitude, air),
		    .sky = gi::SkySh(towardsLight, altitude, air, k_SkySamples),
		};
	}
	const auto& cache = *view.cache;

	AtmosphereFrame atmosphere;
	atmosphere.haze = system.IsEffectOn(Effect::Haze);
	atmosphere.lighting = system.IsEffectOn(Effect::GlobalIllumination);
	atmosphere.hazeDetail = underwater::HazeDetail(system.GetQuality());
	atmosphere.lightDetail = underwater::LightingDetail(system.GetQuality());
	atmosphere.params = {air.turbidity, atmos::k_HazeScale, altitude, seaLevel};
	atmosphere.light = {towardsLight, atmosphere.haze ? 1.0f : 0.0f};
	atmosphere.sunHeight = atmos::AirSunElevation(hour);
	// The place's own colour tints the haze: the land's cells round the point that the camera looks at
	{
		const auto& island = Locator::terrainSystem::value();
		const glm::vec3 focus = Locator::camera::has_value() ? Locator::camera::value().GetFocus() : desc.camera->GetFocus();
		constexpr int k_Reach = 3;
		constexpr float k_Step = 30.0f;
		std::vector<glm::vec3> colours;
		colours.reserve((2 * k_Reach + 1) * (2 * k_Reach + 1));
		for (int z = -k_Reach; z <= k_Reach; ++z)
		{
			for (int x = -k_Reach; x <= k_Reach; ++x)
			{
				const glm::vec2 at = glm::vec2(focus.x, focus.z) + glm::vec2(x, z) * k_Step;
				const auto cellX = static_cast<int>(at.x / LandIslandInterface::k_CellSize);
				const auto cellZ = static_cast<int>(at.y / LandIslandInterface::k_CellSize);
				if (cellX < 0 || cellZ < 0 || cellX >= LandIslandInterface::k_MapCellsPerSide ||
				    cellZ >= LandIslandInterface::k_MapCellsPerSide)
				{
					continue;
				}
				const auto& cell = island.GetCell(glm::u16vec2(cellX, cellZ));
				// The game keeps a cell's colour with red and blue swapped
				colours.emplace_back(glm::vec3(cell.b, cell.g, cell.r) / 255.0f);
			}
		}
		atmosphere.placeTint = atmos::PlaceTint(colours, atmos::k_PlaceShare);
	}
	// The air's horizon is as bright as the original game's haze, with some of its colour
	const glm::vec3 tint = glm::mix(glm::vec3(1.0f), k_MoonTint, frame.light.w);
	const glm::vec3 original = glm::vec3(frame.sky);
	atmosphere.sun = {atmos::ScreenScale(cache.horizon, original, tint, atmos::k_OriginalHue), 0.0f};
	atmosphere.sun = {glm::vec3(atmosphere.sun) * atmosphere.placeTint * k_HazeBrightness, 0.0f};
	atmosphere.horizon = {cache.horizon * glm::vec3(atmosphere.sun), 0.0f};
	// The clouds and the fog take the air's colour of the light, at the brightness that they have without the haze
	const glm::vec3 airLight = atmos::LightAtGround(towardsLight.y, air);
	const float brightness = atmos::Luminance(volumetric_clouds::SunThroughAir(gameLight.y));
	atmosphere.lightTint = airLight * (brightness / std::max(atmos::Luminance(airLight), 1e-4f));
	atmosphere.lightAtGround = atmos::LightAtGround(towardsLight.y, air);
	// The game's own sun or moon in the sky, for its glare and for the light through the leaves
	atmosphere.sunDisc = {gameLight, frame.lightColour.w * glm::mix(1.0f, 0.35f, frame.light.w)};
	atmosphere.sunColour = {glm::vec3(frame.lightColour) * atmosphere.lightTint, 0.0f};
	atmosphere.sky = cache.sky;
	// The sky's dome: its middle across the land, its half width and its height. The shaders take it as a half ball
	// of this size; a point near its skin is on the dome (sky_dome.sh)
	if (Locator::skySystem::has_value())
	{
		const auto* dome =
		    Locator::entitiesRegistry::value().TryGet<const ecs::components::SkyDome>(Locator::skySystem::value().GetDome());
		const auto* mesh = dome != nullptr ? Locator::resources::value().GetMeshes().Find(dome->meshId) : nullptr;
		if (mesh != nullptr)
		{
			const auto box = mesh->GetBoundingBox();
			const float wide = 0.5f * std::min(box.Size().x, box.Size().z) * sky_dome::k_Scale;
			const float high = box.maxima.y * sky_dome::k_Scale;
			atmosphere.skyDome = {sky_dome::k_Centre.x, sky_dome::k_Centre.z, wide, high};
		}
	}
	const auto medium = atmos::MediumAt(altitude, air);
	atmosphere.lowTierDensity = atmos::Luminance(medium.extinction) * atmos::k_HazeScale / atmos::k_UnitsPerKilometre;
	_atmosphereFrame = atmosphere;

	const auto setUniforms = [&atmosphere](const ShaderProgram& program) {
		program.SetUniformValue("u_atmosParams", &atmosphere.params);
		program.SetUniformValue("u_atmosLight", &atmosphere.light);
		program.SetUniformValue("u_atmosSun", &atmosphere.sun);
	};
	// The texture of the light that gets through, when the air's dust has changed
	const auto* transmittanceProgram = _shaderManager->GetShader("AtmosphereTransmittance");
	const auto* skyProgram = _shaderManager->GetShader("AtmosphereSkyView");
	if (transmittanceProgram != nullptr &&
	    (!view.transmittanceFor.has_value() || std::abs(*view.transmittanceFor - air.turbidity) > 0.05f))
	{
		const auto pass = static_cast<bgfx::ViewId>(RenderPass::AtmosphereTransmittance);
		bgfx::setViewFrameBuffer(pass, view.transmittance.Target());
		bgfx::setViewRect(pass, 0, 0, view.transmittance.Width(), view.transmittance.Height());
		bgfx::setViewClear(pass, BGFX_CLEAR_NONE);
		setUniforms(*transmittanceProgram);
		const glm::vec4 target {view.transmittance.Width(), view.transmittance.Height(), 0.0f, 0.0f};
		transmittanceProgram->SetUniformValue("u_atmosTarget", &target);
		BindScreenTriangle();
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
		transmittanceProgram->Submit(pass);
		view.transmittanceFor = air.turbidity;
		view.transmittance.drawn = true;
	}
	// The sky round the eye, each frame
	{
		const auto pass = static_cast<bgfx::ViewId>(RenderPass::AtmosphereSkyView);
		bgfx::setViewFrameBuffer(pass, view.skyView.Target());
		bgfx::setViewRect(pass, 0, 0, view.skyView.Width(), view.skyView.Height());
		bgfx::setViewClear(pass, BGFX_CLEAR_NONE);
		setUniforms(*skyProgram);
		skyProgram->SetTextureSampler("s_atmosTransmittance", 13, view.transmittance.Texture());
		const glm::vec4 target {view.skyView.Width(), view.skyView.Height(), 0.0f, 0.0f};
		skyProgram->SetUniformValue("u_atmosTarget", &target);
		BindScreenTriangle();
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
		skyProgram->Submit(pass);
		view.skyView.drawn = true;
	}
}

void Renderer::BindAtmosphere(const ShaderProgram& program, float skyShare) const
{
	const bool ready =
	    _atmosphereFrame.has_value() && _atmosphere != nullptr && _atmosphere->skyView.drawn && _atmosphereFrame->haze;
	const float sunHeight = _atmosphereFrame.has_value() ? _atmosphereFrame->sunHeight : 1.0f;
	const glm::vec4 share {ready ? skyShare : 0.0f, sunHeight, 0.0f, 0.0f};
	program.SetUniformValue("u_skyAtmos", &share);
	if (!ready)
	{
		return;
	}
	const auto& atmosphere = *_atmosphereFrame;
	program.SetUniformValue("u_atmosParams", &atmosphere.params);
	program.SetUniformValue("u_atmosLight", &atmosphere.light);
	program.SetUniformValue("u_atmosSun", &atmosphere.sun);
	program.SetTextureSampler("s_atmosTransmittance", 13, _atmosphere->transmittance.Texture());
	program.SetTextureSampler("s_atmosSkyView", 14, _atmosphere->skyView.Texture());
}

void Renderer::DrawHaze(const DrawSceneDesc& desc, const FrameBuffer& sceneTarget) const
{
	if (!_atmosphereFrame.has_value() || _atmosphere == nullptr || !_atmosphereFrame->haze || !_underwaterFrame.has_value())
	{
		return;
	}
	// Under the water the post pass has the water's own fog
	if (_underwaterFrame->eye.w > 0.5f)
	{
		return;
	}
	const auto* aerialProgram = _shaderManager->GetShader("AtmosphereAerial");
	const auto* applyProgram = _shaderManager->GetShader("HazeApply");
	if (aerialProgram == nullptr || applyProgram == nullptr)
	{
		return;
	}
	auto& view = *_atmosphere;
	const auto& atmosphere = *_atmosphereFrame;
	const bool grid = atmosphere.hazeDetail != underwater::LightDetail::Basic;
	const auto setUniforms = [&atmosphere](const ShaderProgram& program) {
		program.SetUniformValue("u_atmosParams", &atmosphere.params);
		program.SetUniformValue("u_atmosLight", &atmosphere.light);
		program.SetUniformValue("u_atmosSun", &atmosphere.sun);
	};

	// The haze's grid that follows the view
	if (grid)
	{
		const bool full = atmosphere.hazeDetail == underwater::LightDetail::Full;
		const auto pass = static_cast<bgfx::ViewId>(RenderPass::AtmosphereAerial);
		bgfx::setViewFrameBuffer(pass, view.aerial.Target());
		bgfx::setViewRect(pass, 0, 0, view.aerial.Width(), view.aerial.Height());
		bgfx::setViewClear(pass, BGFX_CLEAR_NONE);
		_shaderManager->SetCamera(RenderPass::AtmosphereAerial, *desc.camera);
		setUniforms(*aerialProgram);
		aerialProgram->SetTextureSampler("s_atmosTransmittance", 13, view.transmittance.Texture());
		BindCloudShadow(*aerialProgram, 11);
		BindSunVisibility(*aerialProgram, 12);
		const glm::vec4 aerial {full ? 1.0f : 0.0f, full ? k_FullSteps : k_ReducedSteps, 0.0f, 0.0f};
		aerialProgram->SetUniformValue("u_atmosAerial", &aerial);
		aerialProgram->SetUniformValue("u_atmosGameLight", &atmosphere.sunDisc);
		BindScreenTriangle();
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
		aerialProgram->Submit(pass);
		view.aerial.drawn = true;
	}

	// The haze over the scene and the water, first of what blends over them. The clouds come after it and fade with
	// their own distance, so a cloud in front of the far sea gets no haze of the sea's distance.
	setUniforms(*applyProgram);
	if (grid)
	{
		applyProgram->SetTextureSampler("s_atmosAerial", 0, view.aerial.Texture());
	}
	applyProgram->SetTextureSampler("s_sceneDepth", 1, sceneTarget.GetDepthStencilAttachment());
	if (grid)
	{
		// The scene far away fades into the sky that the dome shows at the horizon
		applyProgram->SetTextureSampler("s_atmosSkyView", 14, view.skyView.Texture());
	}
	const glm::vec4 apply {grid ? 1.0f : 0.0f, atmosphere.lowTierDensity, atmosphere.sunHeight, 0.0f};
	applyProgram->SetUniformValue("u_hazeApply", &apply);
	applyProgram->SetUniformValue("u_hazeHorizon", &atmosphere.horizon);
	applyProgram->SetUniformValue("u_hazeDome", &atmosphere.skyDome);
	applyProgram->SetUniformValue("u_hazeSun", &atmosphere.sunDisc);
	applyProgram->SetUniformValue("u_hazeSunColour", &atmosphere.sunColour);
	BindScreenTriangle();
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_SRC_ALPHA));
	applyProgram->Submit(static_cast<bgfx::ViewId>(RenderPass::UnderwaterOver), std::numeric_limits<uint32_t>::max());
}

void Renderer::BindFoliageLight(const ShaderProgram& program) const
{
	const bool on = _atmosphereFrame.has_value() && _atmosphereFrame->lighting;
	const glm::vec4 light = on ? _atmosphereFrame->sunDisc : glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
	program.SetUniformValue("u_foliageLight", &light);
	if (on)
	{
		program.SetUniformValue("u_foliageLightColour", &_atmosphereFrame->sunColour);
	}
}
