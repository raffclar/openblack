/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the light in the shade: the grid of light probes over the island, the shade from the
// scene's depth and the sky's light. The original game bakes its light into the land.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <array>
#include <optional>

#include <bgfx/bgfx.h>
#include <glm/vec4.hpp>

#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "3D/OceanInterface.h"
#include "Camera/Camera.h"
#include "ECS/Systems/UnderwaterSystemInterface.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/Mesh.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/UnderwaterView.h"
#include "Graphics/VertexBuffer.h"
#include "Locator.h"
#include "Renderer.h"

using namespace openblack;
using namespace openblack::graphics;
namespace gi = openblack::graphics::global_illumination;

namespace
{
/// How far the shade from the scene's depth looks for what hides the sky, in world units
constexpr float k_OcclusionRadius = 2.5f;

void BindScreenTriangle()
{
	const auto& mesh = Locator::oceanSystem::value().GetMesh();
	mesh.GetIndexBuffer().Bind(mesh.GetIndexBuffer().GetCount(), 0);
	mesh.GetVertexBuffer().Bind();
}

/// The sky's harmonics for each colour, as the shaders take them
std::array<glm::vec4, 3> SkyUniform(const gi::ShL1& sh)
{
	std::array<glm::vec4, 3> channels {};
	for (int channel = 0; channel < 3; ++channel)
	{
		channels.at(static_cast<size_t>(channel)) = {sh.c[0][channel], sh.c[1][channel], sh.c[2][channel], sh.c[3][channel]};
	}
	return channels;
}
} // namespace

void Renderer::BindLighting(const ShaderProgram& program, uint8_t stage) const
{
	const bool on = _atmosphereFrame.has_value() && _atmosphereFrame->lighting;
	const bool probes = on && _lightProbes != nullptr && _lightProbes->newest.has_value() &&
	                    _atmosphereFrame->lightDetail != underwater::LightDetail::Basic && _lightProbes->blended.drawn;
	const glm::vec4 params {probes ? 1.0f : 0.0f, on ? 1.0f : 0.0f, gi::k_ProbeSpacing,
	                        static_cast<float>(gi::k_ProbesPerSide)};
	program.SetUniformValue("u_lightParams", &params);
	if (!on)
	{
		return;
	}
	const auto sky = SkyUniform(_atmosphereFrame->sky);
	program.SetUniformArray("u_lightSky", sky.data(), static_cast<uint16_t>(sky.size()));
	const glm::vec4 openSky {gi::ShIrradiance(_atmosphereFrame->sky, {0.0f, 1.0f, 0.0f}), gi::k_ColourShare};
	program.SetUniformValue("u_lightOpenSky", &openSky);
	if (probes)
	{
		program.SetTextureSampler("s_lightProbes", stage, _lightProbes->blended.Texture());
	}
}

void Renderer::DrawLightProbes(const DrawSceneDesc& /*desc*/, bool newShadows) const
{
	if (!_atmosphereFrame.has_value() || !_atmosphereFrame->lighting ||
	    _atmosphereFrame->lightDetail == underwater::LightDetail::Basic || _underwater == nullptr ||
	    !Locator::underwaterSystem::has_value() || !Locator::terrainSystem::has_value() || !_underwaterFrame.has_value())
	{
		return;
	}
	const auto* traceProgram = _shaderManager->GetShader("LightProbes");
	const auto* blendProgram = _shaderManager->GetShader("LightProbesBlend");
	if (traceProgram == nullptr || blendProgram == nullptr)
	{
		return;
	}
	if (_lightProbes == nullptr)
	{
		_lightProbes = std::make_unique<LightProbesView>();
	}
	auto& probes = *_lightProbes;
	const auto& system = Locator::underwaterSystem::value();
	const float seconds = system.GetSeconds();
	const auto size = LightProbesView::Size();

	// A new set of shadows begins the grid again, into the older grid; the first grid begins at once
	if (!probes.sweep.Running() && (newShadows || !probes.newest.has_value()))
	{
		probes.building = probes.newest.has_value() ? static_cast<uint8_t>(1 - *probes.newest) : 0;
		probes.sweep.Start();
	}
	if (const auto band = probes.sweep.Next())
	{
		const auto pass = static_cast<bgfx::ViewId>(RenderPass::LightProbes);
		auto& grid = *probes.grids.at(probes.building);
		bgfx::setViewFrameBuffer(pass, grid.Target());
		bgfx::setViewRect(pass, 0, band->firstRow, size.x, band->rows);
		bgfx::setViewClear(pass, BGFX_CLEAR_NONE);
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
		traceProgram->SetTextureSampler("s_shore", 0, _underwater->Seabed(system.GetSeabed(), system.GetSeabedRevision()));
		traceProgram->SetTextureSampler("s_blockTextures", 1, island.GetBlockTextures());
		traceProgram->SetTextureSampler("s_blockLayers", 2, _underwater->BlockLayers(layers, system.GetSeabedRevision()));
		BindCloudShadow(*traceProgram, 11);
		BindSunVisibility(*traceProgram, 12);
		BindLighting(*traceProgram, 15);
		const glm::vec4 sun {_atmosphereFrame->lightAtGround, 0.0f};
		traceProgram->SetUniformValue("u_lightSun", &sun);
		// The ground faces the air's sun, which climbs high by day; the hills' shadows are the game's own
		const glm::vec4 towards {glm::vec3(_atmosphereFrame->light), _underwaterFrame->params.z};
		traceProgram->SetUniformValue("u_lightTowards", &towards);
		BindScreenTriangle();
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
		traceProgram->Submit(pass);
		grid.drawn = true;
		if (!probes.sweep.Running())
		{
			probes.blendedInFull.reset();
			probes.olderDone = probes.newest.has_value();
			probes.newest = probes.building;
			probes.newestAt = seconds;
		}
	}
	if (!probes.newest.has_value())
	{
		return;
	}

	// The old grid blends into the new; while the old grid is made again only the new one shows
	const uint8_t newest = *probes.newest;
	const auto older = static_cast<uint8_t>(1 - newest);
	const bool olderUsable = probes.olderDone && !(probes.sweep.Running() && probes.building == older);
	const float blend = olderUsable ? gi::ProbeBlend(seconds - probes.newestAt) : 1.0f;
	if (blend >= 1.0f && probes.blendedInFull == newest)
	{
		return;
	}
	probes.blendedInFull = blend >= 1.0f ? std::optional<uint8_t>(newest) : std::nullopt;
	const auto pass = static_cast<bgfx::ViewId>(RenderPass::LightProbesBlend);
	bgfx::setViewFrameBuffer(pass, probes.blended.Target());
	bgfx::setViewRect(pass, 0, 0, size.x, size.y);
	bgfx::setViewClear(pass, BGFX_CLEAR_NONE);
	blendProgram->SetTextureSampler("s_probesOld", 0, probes.grids.at(olderUsable ? older : newest)->Texture());
	blendProgram->SetTextureSampler("s_probesNew", 1, probes.grids.at(newest)->Texture());
	const glm::vec4 blendUniform {blend, size.x, size.y, 0.0f};
	blendProgram->SetUniformValue("u_lightBlend", &blendUniform);
	BindScreenTriangle();
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
	blendProgram->Submit(pass);
	probes.blended.drawn = true;
}

void Renderer::DrawAmbientOcclusion(const DrawSceneDesc& desc, const FrameBuffer& sceneTarget) const
{
	if (!_atmosphereFrame.has_value() || !_atmosphereFrame->lighting ||
	    _atmosphereFrame->lightDetail != underwater::LightDetail::Full || !Locator::underwaterSystem::has_value() ||
	    !Locator::underwaterSystem::value().IsEffectOn(underwater::Effect::AmbientOcclusion) || _clouds == nullptr ||
	    !_clouds->Ready())
	{
		if (_lightProbes != nullptr && _lightProbes->occlusion != nullptr)
		{
			_lightProbes->occlusion->drawn = false;
		}
		return;
	}
	const auto* program = _shaderManager->GetShader("AmbientOcclusion");
	if (program == nullptr)
	{
		return;
	}
	if (_lightProbes == nullptr)
	{
		_lightProbes = std::make_unique<LightProbesView>();
	}
	const auto* stats = bgfx::getStats();
	const glm::u16vec2 screen {stats->width, stats->height};
	const glm::u16vec2 half {static_cast<uint16_t>((screen.x + 1) / 2), static_cast<uint16_t>((screen.y + 1) / 2)};
	auto& occlusion = _lightProbes->occlusion;
	if (occlusion == nullptr || occlusion->Width() != half.x || occlusion->Height() != half.y)
	{
		occlusion = std::make_unique<OwnedTarget>("Ambient Occlusion", half.x, half.y, bgfx::TextureFormat::RGBA16F, false);
	}
	const auto pass = static_cast<bgfx::ViewId>(RenderPass::AmbientOcclusion);
	bgfx::setViewFrameBuffer(pass, occlusion->Target());
	bgfx::setViewRect(pass, 0, 0, half.x, half.y);
	bgfx::setViewClear(pass, BGFX_CLEAR_NONE);
	_shaderManager->SetCamera(RenderPass::AmbientOcclusion, *desc.camera);
	program->SetTextureSampler("s_sceneDepth", 0, sceneTarget.GetDepthStencilAttachment());
	program->SetTextureSampler("s_aoNoise", 1, _clouds->BlueNoise());
	const float pixelsPerUnit = static_cast<float>(screen.y) * 0.5f * desc.camera->GetProjectionMatrix()[1][1];
	const glm::vec4 params {k_OcclusionRadius, pixelsPerUnit, screen.x, screen.y};
	program->SetUniformValue("u_aoParams", &params);
	BindScreenTriangle();
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
	program->Submit(pass);
	occlusion->drawn = true;
}
