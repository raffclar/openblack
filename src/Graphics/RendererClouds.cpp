/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the volumetric clouds. The original game draws its clouds as billboards; with the
// experiment on, these clouds take their place.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <array>
#include <iterator>
#include <limits>
#include <span>
#include <vector>

#include <bgfx/bgfx.h>
#include <glm/common.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "3D/LandIslandInterface.h"
#include "3D/OceanInterface.h"
#include "Camera/Camera.h"
#include "ECS/Systems/UnderwaterSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/Systems/WindSystemInterface.h"
#include "Graphics/Atmosphere.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/Mesh.h"
#include "Graphics/PlanetWeather.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/Underwater.h"
#include "Graphics/VertexBuffer.h"
#include "Graphics/VolumetricClouds.h"
#include "Locator.h"
#include "Renderer.h"

using namespace openblack;
using namespace openblack::graphics;
namespace clouds = openblack::graphics::volumetric_clouds;

namespace
{
/// How much of the last frames' clouds each frame keeps
constexpr float k_HistoryShare = 0.92f;
/// How strong the shafts of light through the gaps in the clouds are on the screen. None: the volumetric fog gives
/// the shafts of light, through the clouds' shadow.
constexpr float k_ShaftStrength = 0.0f;
/// How dark the clouds' shadow is on the land and the water: a light shadow
constexpr float k_ShadowStrength = 0.5f;
/// The light of the sky on the clouds, from the colour of the haze
constexpr float k_SkyLight = 0.95f;

/// Binds the sea's triangle over the whole screen for the next draw
void BindScreenTriangle()
{
	const auto& mesh = Locator::oceanSystem::value().GetMesh();
	mesh.GetIndexBuffer().Bind(mesh.GetIndexBuffer().GetCount(), 0);
	mesh.GetVertexBuffer().Bind();
}

glm::mat4 ViewProjection(const Camera& camera)
{
	return camera.GetProjectionMatrix() * camera.GetViewMatrix(Camera::Interpolation::Current);
}
} // namespace

void Renderer::BeginClouds(const DrawSceneDesc& desc) const
{
	_cloudsFrame.reset();
	if (!_underwaterFrame.has_value() || !Locator::underwaterSystem::has_value() || !Locator::oceanSystem::has_value() ||
	    _shaderManager->GetShader("CloudMarch") == nullptr)
	{
		return;
	}
	const auto& system = Locator::underwaterSystem::value();
	if (system.GetEffectOff() == underwater::Effect::Clouds)
	{
		return;
	}
	if (_clouds == nullptr)
	{
		_clouds = std::make_unique<VolumetricCloudsView>();
	}
	// The original clouds show until the noise is made
	if (!_clouds->Ready())
	{
		return;
	}
	const auto& frame = *_underwaterFrame;
	const glm::vec3 eye {frame.eye};

	// The one wind of the experiment, at the clouds
	glm::vec2 cloudWind {0.0f};
	if (Locator::windSystem::has_value())
	{
		const auto& wind = Locator::windSystem::value().GetWind();
		cloudWind = clouds::CloudWind(wind::Direction(wind), wind.speed);
	}
	// The weather at the eye and the storms over the land
	clouds::CloudWeather target;
	std::vector<clouds::StormInput> storms;
	if (Locator::weatherSystem::has_value())
	{
		auto& weatherSystem = Locator::weatherSystem::value();
		const auto weather = weatherSystem.GetWeatherSmooth(desc.camera->GetOrigin());
		target = clouds::WeatherFor(static_cast<float>(weather.overcast) / 100.0f, static_cast<float>(weather.rain) / 100.0f,
		                            cloudWind);
		for (const auto& storm : weatherSystem.GetActiveStorms())
		{
			if (storm.dead)
			{
				continue;
			}
			storms.push_back({.serial = storm.serial,
			                  .centre = {storm.currentPosition.x, storm.currentPosition.z},
			                  .innerRadius = storm.currentInnerRadius,
			                  .outerRadius = storm.outerRadius,
			                  .strength = std::clamp(storm.currentStrength, 0.0f, 1.0f) *
			                              clouds::StormCloudShare(storm.effect.overcast, storm.effect.rain),
			                  .rain = std::clamp(static_cast<float>(storm.effect.rain) / 100.0f, 0.0f, 1.0f)});
		}
	}
	else
	{
		target = clouds::WeatherFor(0.0f, 0.0f, cloudWind);
	}
	const auto chosen = clouds::ChosenWeather(static_cast<clouds::CoverChoice>(system.GetCloudCover()), target);

	// The weather eases by the time of this frame, at most a quarter of a second
	auto& climate = _clouds->Climate();
	const float seconds = system.GetSeconds();
	if (!_clouds->lastSeconds.has_value())
	{
		climate.Reset(chosen);
	}
	const float elapsed = _clouds->lastSeconds.has_value() ? std::clamp(seconds - *_clouds->lastSeconds, 0.0f, 0.25f) : 0.0f;
	_clouds->lastSeconds = seconds;
	climate.Step(elapsed, chosen, storms);
	const auto current = climate.Current();

	clouds::CloudUniforms uniforms;
	uniforms.layer.w = frame.params.z;
	uniforms.cover = {current.cover, current.type, current.rain, 0.0f};
	uniforms.drift = {climate.Drift(), climate.Evolution(), static_cast<float>(_clouds->frame % 4096u)};
	uniforms.light = {frame.above.x, frame.above.y, frame.above.z, frame.light.w};
	// The sun's light goes orange and red near the horizon. The moon's light is cool and silver.
	constexpr glm::vec3 k_MoonTint {0.72f, 0.84f, 1.1f};
	const float moonShare = frame.light.w;
	// EXPERIMENT (not for merge): with the haze on, the light through the air comes from the air's model, in the
	// colours of the screen. The clouds' shade takes the air's upper sky while the sun is low, not the red horizon.
	const bool air = _atmosphereFrame.has_value() && _atmosphereFrame->haze;
	const glm::vec3 throughAir = air ? _atmosphereFrame->lightTintOnScreen : clouds::SunThroughAir(frame.above.y);
	const glm::vec3 tint = glm::mix(throughAir, k_MoonTint, moonShare);
	uniforms.lightColour = {glm::vec3(frame.lightColour) * tint, frame.lightColour.w};
	const glm::vec3 original = glm::vec3(frame.sky) * k_SkyLight;
	const glm::vec3 skyLight =
	    air ? atmosphere::CloudSkyLight(original, _atmosphereFrame->skyFromAbove, _atmosphereFrame->sunHeight) : original;
	// The flash of the sky makes the whole sky bright for a moment. The clouds keep the light of the sky from before the
	// flash, so that each flash lights its own part of the cloud more than the rest.
	const float skyFlash = system.GetSeaFlash().x;
	if (skyFlash <= 0.0f || !_clouds->steadySky.has_value())
	{
		_clouds->steadySky = skyLight;
	}
	uniforms.ambient = {*_clouds->steadySky, skyFlash};
	uniforms.eye = {eye, frame.eye.w > 0.5f ? 0.0f : 1.0f};
	// Each flash of the storms lights the clouds from inside, along a channel up through a thunderstorm cell's tower
	std::array<cloud_lightning::FlashLight, cloud_lightning::k_MaxFlashes> flashLights {};
	const auto flashes = system.GetCloudFlashes();
	std::ranges::transform(flashes, flashLights.begin(), [&eye](const cloud_lightning::FlashEvent& flash) {
		return cloud_lightning::LightFor(flash, eye, cumulonimbus::k_Base, cumulonimbus::k_Tropopause - cumulonimbus::k_Base);
	});
	uniforms.flashes = cloud_lightning::Pack(std::span(flashLights).first(flashes.size()));
	// From high up the clouds and their shadow reach farther, so the layer and the shadow have no edge round the eye
	// EXPERIMENT (not for merge): the height over the curved world, also with the eye far round the planet
	const float mapSize = static_cast<float>(LandIslandInterface::k_MapCellsPerSide) * LandIslandInterface::k_CellSize;
	const float eyeHeight = atmosphere::EyeOverWorld(eye, frame.params.z, mapSize, desc.camera->IsIslandFar());
	const float window = clouds::ShadowWindowFor(eyeHeight);
	uniforms.shadow = {clouds::ShadowOrigin({eye.x, eye.z}, window), window, k_ShadowStrength};
	uniforms.layer.z = clouds::FarthestCloudFor(eyeHeight);
	clouds::PackStorms(climate.Storms(), uniforms);
	// The thunderstorm cells of the storms, from the same storms and time as the lightning's strikes use. Their anvils
	// spread out downwind.
	std::vector<cumulonimbus::StormSite> sites;
	std::ranges::transform(climate.Storms(), std::back_inserter(sites), [](const clouds::StormCell& cell) {
		return cumulonimbus::StormSite {.serial = cell.serial,
		                                .centre = cell.centre,
		                                .outerRadius = cell.outerRadius,
		                                .strength = clouds::CloudClimate::Strength(cell),
		                                .rain = clouds::CloudClimate::Rain(cell)};
	});
	// The planet's weather map, round the middle of the map, as the clouds drift over it. The layer's cover is the
	// game's cover over the island, so the map's cover over the island is the one that the shaders compare with.
	if (_clouds->PlanetReady())
	{
		const auto overIsland =
		    planet_weather::WeatherAt(planet_weather::DirectionAt(-climate.Drift(), clouds::k_PlanetRadius));
		uniforms.planet = {planet_weather::k_MapMiddle, overIsland.cover, 1.0f};
	}
	uniforms.cells = cumulonimbus::Pack(cumulonimbus::CellsFor(sites, seconds).All(), current.wind);
	_cloudsFrame = uniforms;
}

void Renderer::SetCloudUniforms(const ShaderProgram& program, const clouds::CloudUniforms& uniforms) const
{
	program.SetUniformValue("u_cloudLayer", &uniforms.layer);
	SetWorldCurve(program);
	program.SetUniformValue("u_cloudCover", &uniforms.cover);
	program.SetUniformValue("u_cloudDrift", &uniforms.drift);
	program.SetUniformValue("u_cloudLight", &uniforms.light);
	program.SetUniformValue("u_cloudLightColour", &uniforms.lightColour);
	program.SetUniformValue("u_cloudAmbient", &uniforms.ambient);
	program.SetUniformValue("u_cloudEye", &uniforms.eye);
	program.SetUniformArray("u_cloudFlashes", uniforms.flashes.data(), static_cast<uint16_t>(uniforms.flashes.size()));
	// EXPERIMENT (not for merge): the sky's dome, which writes its depth, is the sky
	const glm::vec4 noDome {0.0f, 0.0f, 1e9f, 1e9f};
	program.SetUniformValue("u_hazeDome", _atmosphereFrame.has_value() ? &_atmosphereFrame->skyDome : &noDome);
	program.SetUniformArray("u_cloudStorms", uniforms.storms.data(), static_cast<uint16_t>(uniforms.storms.size()));
	program.SetUniformArray("u_cloudCells", uniforms.cells.data(), static_cast<uint16_t>(uniforms.cells.size()));
	program.SetTextureSampler("s_cloudBase", 0, _clouds->BaseNoise());
	program.SetTextureSampler("s_cloudDetail", 1, _clouds->DetailNoise());
	program.SetTextureSampler("s_cloudWeather", 2, _clouds->WeatherNoise());
	program.SetUniformValue("u_cloudPlanet", &uniforms.planet);
	if (_clouds->PlanetReady())
	{
		program.SetTextureSampler("s_cloudPlanet", 8, _clouds->PlanetWeather());
	}
}

void Renderer::DrawClouds(const DrawSceneDesc& desc, const FrameBuffer& sceneTarget) const
{
	if (!_cloudsFrame.has_value() || _clouds == nullptr || !_underwaterFrame.has_value())
	{
		return;
	}
	const auto* shadowProgram = _shaderManager->GetShader("CloudShadow");
	// The rays through the thunderstorm cells take a shader of their own: the cells' code would slow down the rays when
	// no cell is there
	const auto cellShown = [](const glm::vec4& cell) { return cell.w > 0.0f; };
	const bool cells = std::ranges::any_of(std::span(_cloudsFrame->cells).first(cumulonimbus::k_MaxCells), cellShown);
	const auto* plainMarch = _shaderManager->GetShader("CloudMarch");
	const auto* cellsMarch = _shaderManager->GetShader("CloudMarchCells");
	const auto* marchProgram = cells && cellsMarch != nullptr ? cellsMarch : plainMarch;
	const auto* resolveProgram = _shaderManager->GetShader("CloudResolve");
	const auto* compositeProgram = _shaderManager->GetShader("CloudComposite");
	if (shadowProgram == nullptr || marchProgram == nullptr || resolveProgram == nullptr || compositeProgram == nullptr)
	{
		return;
	}
	const auto& uniforms = *_cloudsFrame;
	const auto* stats = bgfx::getStats();
	const glm::u16vec2 screen {stats->width, stats->height};
	const auto size = _clouds->Fit(screen);
	_clouds->Flip();
	++_clouds->frame;

	// The map of the shadow, seen from above, before the land's shade reads it
	{
		const auto view = static_cast<bgfx::ViewId>(RenderPass::CloudShadow);
		constexpr auto k_Side = static_cast<uint16_t>(clouds::k_ShadowTexels);
		bgfx::setViewFrameBuffer(view, _clouds->ShadowTarget());
		bgfx::setViewRect(view, 0, 0, k_Side, k_Side);
		bgfx::setViewClear(view, BGFX_CLEAR_NONE);
		SetCloudUniforms(*shadowProgram, uniforms);
		shadowProgram->SetUniformValue("u_cloudShadow", &uniforms.shadow);
		BindScreenTriangle();
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
		shadowProgram->Submit(view);
	}

	// The rays, at half the screen's size, against the scene's depth
	{
		const auto view = static_cast<bgfx::ViewId>(RenderPass::CloudMarch);
		bgfx::setViewFrameBuffer(view, _clouds->MarchTarget());
		bgfx::setViewRect(view, 0, 0, size.x, size.y);
		bgfx::setViewClear(view, BGFX_CLEAR_NONE);
		_shaderManager->SetCamera(RenderPass::CloudMarch, *desc.camera);
		// The light of the sky comes from the cube of the sky that the water reflects, once it is drawn
		const bool skyReady = _underwater != nullptr && _underwaterFrame->sea.w > 0.5f;
		const glm::vec4 screenSize {static_cast<float>(screen.x), static_cast<float>(screen.y), skyReady ? 1.0f : 0.0f, 0.0f};
		const auto submitMarch = [&](const ShaderProgram& program) {
			SetCloudUniforms(program, uniforms);
			program.SetTextureSampler("s_cloudBlueNoise", 3, _clouds->BlueNoise());
			program.SetTextureSampler("s_sceneDepth", 4, sceneTarget.GetDepthStencilAttachment());
			if (skyReady)
			{
				program.SetTextureSampler("s_cloudSky", 5, _underwater->EnvironmentSky());
			}
			program.SetUniformValue("u_cloudScreen", &screenSize);
			BindScreenTriangle();
			bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
			program.Submit(view);
		};
		// The warm-up draws the other shader of the rays first, so that the first storm makes no new pipeline
		const auto* otherMarch = marchProgram == plainMarch ? cellsMarch : plainMarch;
		if (_warmingUp && otherMarch != nullptr)
		{
			submitMarch(*otherMarch);
		}
		submitMarch(*marchProgram);
	}

	// The blend with the frames before, where each cloud was in the last frame
	const auto viewProj = ViewProjection(*desc.camera);
	{
		const auto view = static_cast<bgfx::ViewId>(RenderPass::CloudResolve);
		bgfx::setViewFrameBuffer(view, _clouds->HistoryTarget());
		bgfx::setViewRect(view, 0, 0, size.x, size.y);
		bgfx::setViewClear(view, BGFX_CLEAR_NONE);
		_shaderManager->SetCamera(RenderPass::CloudResolve, *desc.camera);
		SetCloudUniforms(*resolveProgram, uniforms);
		resolveProgram->SetTextureSampler("s_cloudCurrent", 3, _clouds->MarchColour());
		resolveProgram->SetTextureSampler("s_cloudCurrentDepth", 4, _clouds->MarchDepth());
		resolveProgram->SetTextureSampler("s_cloudHistory", 5, _clouds->PreviousHistory());
		const auto previous = _clouds->previousViewProj.value_or(viewProj);
		resolveProgram->SetUniformValue("u_cloudPreviousViewProj", &previous);
		const glm::vec2 drift {uniforms.drift.x, uniforms.drift.y};
		const glm::vec2 moved = drift - _clouds->previousDrift.value_or(drift);
		_clouds->previousDrift = drift;
		const glm::vec4 resolve {_clouds->HistoryValid() ? 1.0f : 0.0f, k_HistoryShare, moved};
		resolveProgram->SetUniformValue("u_cloudResolve", &resolve);
		BindScreenTriangle();
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
		resolveProgram->Submit(view);
	}
	_clouds->previousViewProj = viewProj;
	_clouds->SetHistoryValid(true);

	// The clouds over the scene. Under the water they go into the scene's target, so that the water bends them with the
	// rest of the sky. Above it they go over the water, which the post pass has drawn.
	const bool under = _underwaterFrame->eye.w > 0.5f;
	const auto compositeView = under ? RenderPass::Translucent : RenderPass::UnderwaterOver;
	SetCloudUniforms(*compositeProgram, uniforms);
	compositeProgram->SetTextureSampler("s_cloudHistory", 3, _clouds->History());
	compositeProgram->SetTextureSampler("s_cloudCurrentDepth", 4, _clouds->MarchDepth());
	// The scene's own target cannot be read while it is drawn into: under the water the four pixels count by place alone
	if (!under)
	{
		compositeProgram->SetTextureSampler("s_sceneDepth", 5, sceneTarget.GetDepthStencilAttachment());
	}
	// The haze between the eye and the clouds: the same haze as over the scene at the clouds' distance
	BindAtmosphere(*compositeProgram, 1.0f);
	const bool hazeGrid = BindHazeGrid(*compositeProgram, 6);
	const glm::vec4 composite {static_cast<float>(size.x), static_cast<float>(size.y), under ? 0.0f : k_ShaftStrength,
	                           hazeGrid ? 1.0f : 0.0f};
	compositeProgram->SetUniformValue("u_cloudComposite", &composite);
	BindScreenTriangle();
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_SRC_ALPHA));
	// Over the water, after the haze and the fog, which find the scene by its depth, and before the rest that blends;
	// under the water, first of all that blends
	compositeProgram->Submit(static_cast<bgfx::ViewId>(compositeView),
	                         under ? std::numeric_limits<uint32_t>::max() : std::numeric_limits<uint32_t>::max() - 2);
}

void Renderer::DrawCloudsInEnvironment(const glm::vec3& centre) const
{
	const auto* program = _shaderManager->GetShader("CloudCube");
	if (!_cloudsFrame.has_value() || _clouds == nullptr || program == nullptr)
	{
		return;
	}
	auto uniforms = *_cloudsFrame;
	// The rays start at the middle of the cube
	uniforms.eye = {centre, 1.0f};
	SetCloudUniforms(*program, uniforms);
	BindScreenTriangle();
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_SRC_ALPHA));
	program->Submit(static_cast<bgfx::ViewId>(RenderPass::UnderwaterEnvironmentSky));
}

void Renderer::BindCloudShadow(const ShaderProgram& program, uint8_t stage) const
{
	if (!_cloudsFrame.has_value() || _clouds == nullptr)
	{
		const glm::vec4 none {0.0f};
		program.SetUniformValue("u_cloudShadow", &none);
		return;
	}
	program.SetUniformValue("u_cloudShadow", &_cloudsFrame->shadow);
	program.SetTextureSampler("s_cloudShadow", stage, _clouds->Shadow());
}
