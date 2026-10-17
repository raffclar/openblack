/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the volumetric fog. The original game has a haze by distance only.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <limits>
#include <vector>

#include <bgfx/bgfx.h>
#include <glm/common.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "3D/OceanInterface.h"
#include "Camera/Camera.h"
#include "ECS/Components/FogPocket.h"
#include "ECS/Registry.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/UnderwaterSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/Systems/WindSystemInterface.h"
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

using namespace openblack;
using namespace openblack::graphics;
namespace fog = openblack::graphics::volumetric_fog;

namespace
{
/// How much of the last frames' fog each frame keeps
constexpr float k_HistoryShare = 0.85f;
/// How much of the fog shows over the sky, so that the sky and the clouds stay clear
constexpr float k_SkyShare = 0.45f;
/// The light of the sky in the fog, from the colour of the haze
constexpr float k_SkyLight = 0.9f;

void BindScreenTriangle()
{
	const auto& mesh = Locator::oceanSystem::value().GetMesh();
	mesh.GetIndexBuffer().Bind(mesh.GetIndexBuffer().GetCount(), 0);
	mesh.GetVertexBuffer().Bind();
}
} // namespace

void Renderer::BeginFog(const DrawSceneDesc& desc) const
{
	_fogFrame.reset();
	if (!_underwaterFrame.has_value() || !Locator::underwaterSystem::has_value() || !Locator::oceanSystem::has_value() ||
	    _shaderManager->GetShader("FogScatter") == nullptr)
	{
		return;
	}
	const auto& system = Locator::underwaterSystem::value();
	if (system.GetEffectOff() == underwater::Effect::Fog)
	{
		return;
	}
	if (_fog == nullptr)
	{
		_fog = std::make_unique<VolumetricFogView>();
	}
	// The fog's mist comes from the clouds' noise
	if (_clouds == nullptr)
	{
		_clouds = std::make_unique<VolumetricCloudsView>();
	}
	if (!_clouds->Ready())
	{
		return;
	}
	const auto& frame = *_underwaterFrame;
	auto& view = *_fog;
	const float seconds = system.GetSeconds();
	const float elapsed = view.lastSeconds.has_value() ? std::clamp(seconds - *view.lastSeconds, 0.0f, 0.25f) : 0.0f;
	view.lastSeconds = seconds;

	// The time of day, the rain and the storm at the camera
	float hour = 12.0f;
	if (Locator::skySystem::has_value())
	{
		hour = Locator::skySystem::value().GetClock().GetScriptTime();
	}
	float rain = 0.0f;
	if (Locator::weatherSystem::has_value())
	{
		rain = static_cast<float>(Locator::weatherSystem::value().GetWeatherSmooth(desc.camera->GetOrigin()).rain) / 100.0f;
	}
	view.wetness = fog::StepWetness(view.wetness, std::clamp(rain, 0.0f, 1.0f), elapsed);
	const auto state = fog::FogFor(hour, view.wetness, system.GetWaves().storm);
	// The wind moves the mist by its travel in this frame
	if (Locator::windSystem::has_value())
	{
		const auto& wind = Locator::windSystem::value();
		view.drift += wind.VelocityAt({desc.camera->GetOrigin().x, desc.camera->GetOrigin().z}) * elapsed;
	}

	fog::FogUniforms uniforms;
	uniforms.layers = {state.hazeDensity, state.hazeHeight, state.groundDensity, state.groundTop};
	uniforms.drift = {view.drift, seconds, static_cast<float>(view.frame % 4096u)};
	uniforms.light = {frame.above.x, frame.above.y, frame.above.z, frame.light.w};
	// Warm at sunrise and sunset, cool and faint under the moon
	constexpr glm::vec3 k_MoonTint {0.72f, 0.84f, 1.1f};
	const glm::vec3 throughAir = _atmosphereFrame.has_value() && _atmosphereFrame->haze
	                                 ? _atmosphereFrame->lightTint
	                                 : volumetric_clouds::SunThroughAir(frame.above.y);
	const glm::vec3 tint = glm::mix(throughAir, k_MoonTint, frame.light.w);
	uniforms.lightColour = {glm::vec3(frame.lightColour) * tint, frame.lightColour.w};
	uniforms.ambient = {glm::vec3(frame.sky) * k_SkyLight, frame.params.z};
	// EXPERIMENT (not for merge): the place's own colour tints the fog
	if (_atmosphereFrame.has_value())
	{
		uniforms.ambient = {glm::vec3(uniforms.ambient) * _atmosphereFrame->placeTint, uniforms.ambient.w};
	}
	// EXPERIMENT (not for merge): the pockets of fog nearest to the eye
	if (system.IsEffectOn(underwater::Effect::FogPockets) && Locator::entitiesRegistry::has_value())
	{
		std::vector<ecs::components::FogPocket> pockets;
		Locator::entitiesRegistry::value().Each<const ecs::components::FogPocket>(
		    [&pockets](const ecs::components::FogPocket& pocket) { pockets.push_back(pocket); });
		const auto packed = fog::PackPockets(pockets, desc.camera->GetOrigin(), fog::DawnFog(hour));
		uniforms.pockets = packed.pockets;
		uniforms.pocketEdges = packed.edges;
	}
	_fogFrame = uniforms;
}

void Renderer::DrawFog(const DrawSceneDesc& desc, const FrameBuffer& sceneTarget) const
{
	if (!_fogFrame.has_value() || _fog == nullptr || _clouds == nullptr || !_underwaterFrame.has_value())
	{
		return;
	}
	// Under the water the post pass has the water's own fog
	if (_underwaterFrame->eye.w > 0.5f)
	{
		_fog->previousViewProj.reset();
		return;
	}
	const auto* scatterProgram = _shaderManager->GetShader("FogScatter");
	const auto* sumProgram = _shaderManager->GetShader("FogIntegrate");
	const auto* applyProgram = _shaderManager->GetShader("FogApply");
	if (scatterProgram == nullptr || sumProgram == nullptr || applyProgram == nullptr)
	{
		return;
	}
	auto& view = *_fog;
	const auto& uniforms = *_fogFrame;
	view.Flip();
	++view.frame;
	const auto size = VolumetricFogView::Size();
	const auto setFogUniforms = [&uniforms](const ShaderProgram& program) {
		program.SetUniformValue("u_fogLayers", &uniforms.layers);
		program.SetUniformValue("u_fogDrift", &uniforms.drift);
		program.SetUniformValue("u_fogLight", &uniforms.light);
		program.SetUniformValue("u_fogLightColour", &uniforms.lightColour);
		program.SetUniformValue("u_fogAmbient", &uniforms.ambient);
		program.SetUniformValue("u_fogGrid", &uniforms.grid);
	};
	scatterProgram->SetUniformArray("u_fogPockets", uniforms.pockets.data(), static_cast<uint16_t>(uniforms.pockets.size()));
	scatterProgram->SetUniformArray("u_fogPocketEdges", uniforms.pocketEdges.data(),
	                                static_cast<uint16_t>(uniforms.pocketEdges.size()));

	// The fog of each cell and the light that it scatters
	const glm::mat4 viewProj = desc.camera->GetProjectionMatrix() * desc.camera->GetViewMatrix(Camera::Interpolation::Current);
	{
		const auto pass = static_cast<bgfx::ViewId>(RenderPass::FogScatter);
		bgfx::setViewFrameBuffer(pass, view.ScatterTarget());
		bgfx::setViewRect(pass, 0, 0, size.x, size.y);
		bgfx::setViewClear(pass, BGFX_CLEAR_NONE);
		_shaderManager->SetCamera(RenderPass::FogScatter, *desc.camera);
		setFogUniforms(*scatterProgram);
		scatterProgram->SetTextureSampler("s_fogPrevious", 0, view.PreviousScatter());
		scatterProgram->SetTextureSampler("s_fogNoise", 4, _clouds->DetailNoise());
		scatterProgram->SetTextureSampler("s_fogBlueNoise", 5, _clouds->BlueNoise());
		if (_underwater != nullptr && Locator::underwaterSystem::has_value())
		{
			const auto& system = Locator::underwaterSystem::value();
			scatterProgram->SetTextureSampler("s_shore", 6,
			                                  _underwater->Seabed(system.GetSeabed(), system.GetSeabedRevision()));
		}
		BindCloudShadow(*scatterProgram, 11);
		BindSunVisibility(*scatterProgram, 12);
		// The light's maps, from the shadows
		glm::vec4 shadowFlags {0.0f, 0.0f, k_HistoryShare, 0.0f};
		if (_shadowsFrame.has_value() && _sunShadows != nullptr)
		{
			const auto newest = _shadowsFrame->clock.newest;
			if (_sunShadows->environmentReady.at(newest))
			{
				scatterProgram->SetTextureSampler("s_shadowNew0", 1, _sunShadows->Environment(newest, 0));
				scatterProgram->SetTextureSampler("s_shadowNew1", 2, _sunShadows->Environment(newest, 1));
				scatterProgram->SetUniformValue("u_fogShadow0", &_sunShadows->environmentMatrices.at(newest).at(0));
				scatterProgram->SetUniformValue("u_fogShadow1", &_sunShadows->environmentMatrices.at(newest).at(1));
				shadowFlags.x = 1.0f;
			}
			scatterProgram->SetTextureSampler("s_shadowMoving", 3, _sunShadows->Moving());
			scatterProgram->SetUniformValue("u_fogShadowMoving", &_sunShadows->movingMatrix);
			shadowFlags.y = 1.0f;
		}
		// The light of the sky comes from the cube of the sky that the water reflects, once it is drawn
		if (_underwater != nullptr && _underwaterFrame->sea.w > 0.5f)
		{
			scatterProgram->SetTextureSampler("s_fogSky", 7, _underwater->EnvironmentSky());
			shadowFlags.w = 1.0f;
		}
		scatterProgram->SetUniformValue("u_fogShadows", &shadowFlags);
		const auto previous = view.previousViewProj.value_or(viewProj);
		scatterProgram->SetUniformValue("u_fogPreviousViewProj", &previous);
		const glm::vec4 previousEye {view.previousEye, view.previousViewProj.has_value() ? 1.0f : 0.0f};
		scatterProgram->SetUniformValue("u_fogPrevious", &previousEye);
		BindScreenTriangle();
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
		scatterProgram->Submit(pass);
	}
	view.previousViewProj = viewProj;
	view.previousEye = desc.camera->GetOrigin();

	// The fog summed from the eye
	{
		const auto pass = static_cast<bgfx::ViewId>(RenderPass::FogIntegrate);
		bgfx::setViewFrameBuffer(pass, view.SumTarget());
		bgfx::setViewRect(pass, 0, 0, size.x, size.y);
		bgfx::setViewClear(pass, BGFX_CLEAR_NONE);
		setFogUniforms(*sumProgram);
		sumProgram->SetTextureSampler("s_fogScatter", 0, view.Scatter());
		BindScreenTriangle();
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
		sumProgram->Submit(pass);
	}

	// The fog over the scene and the water, after the haze and before the clouds
	setFogUniforms(*applyProgram);
	applyProgram->SetTextureSampler("s_fogSum", 0, view.Sum());
	applyProgram->SetTextureSampler("s_sceneDepth", 1, sceneTarget.GetDepthStencilAttachment());
	const glm::vec4 apply {k_SkyShare, 0.0f, 0.0f, 0.0f};
	applyProgram->SetUniformValue("u_fogApply", &apply);
	// EXPERIMENT (not for merge): the sky's dome, so that the fog over the sky is the thinner share
	const glm::vec4 noDome {0.0f, 0.0f, 1e9f, 1e9f};
	applyProgram->SetUniformValue("u_hazeDome", _atmosphereFrame.has_value() ? &_atmosphereFrame->skyDome : &noDome);
	BindScreenTriangle();
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_SRC_ALPHA));
	applyProgram->Submit(static_cast<bgfx::ViewId>(RenderPass::UnderwaterOver), std::numeric_limits<uint32_t>::max() - 1);
}
