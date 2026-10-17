/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the water in the rivers, the leaves and twigs that drift on it, and the splashes where the
// hand taps the water. The original game has none of these.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <algorithm>
#include <array>
#include <limits>
#include <optional>

#include <bgfx/bgfx.h>
#include <glm/common.hpp>
#include <glm/mat4x4.hpp>

#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "3D/OceanInterface.h"
#include "Camera/Camera.h"
#include "ECS/Systems/StreamWaterSystemInterface.h"
#include "ECS/Systems/UnderwaterSystemInterface.h"
#include "ECS/Systems/WaterWakeSystemInterface.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/Mesh.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/Underwater.h"
#include "Graphics/UnderwaterStreamView.h"
#include "Graphics/UnderwaterTap.h"
#include "Graphics/VertexBuffer.h"
#include "Locator.h"
#include "Renderer.h"

using namespace openblack;
using namespace openblack::graphics;
namespace streams = openblack::graphics::underwater::streams;

namespace
{
/// Whether an effect of the experiment is on
bool EffectOn(underwater::Effect effect)
{
	return !Locator::underwaterSystem::has_value() || Locator::underwaterSystem::value().GetEffectOff() != effect;
}

/// The flow map's place for the shaders (see u_streamMap): the low corner of its first texel and 1 over its size
glm::vec4 StreamMapUniform(const streams::FlowMap& map)
{
	if (map.texels.empty())
	{
		return glm::vec4(0.0f);
	}
	const glm::vec2 size = glm::vec2(map.size) * map.spacing;
	return {map.origin - 0.5f * map.spacing, 1.0f / size};
}

/// The box on the screen, in pixels, that holds a box of the world, or none when part of it is behind the eye
std::optional<std::array<uint16_t, 4>> ScreenBox(const glm::mat4& viewProj, glm::vec3 low, glm::vec3 high, glm::u16vec2 screen)
{
	glm::vec2 least {std::numeric_limits<float>::max()};
	glm::vec2 most {std::numeric_limits<float>::lowest()};
	for (int i = 0; i < 8; ++i)
	{
		const glm::vec3 corner {(i & 1) != 0 ? high.x : low.x, (i & 2) != 0 ? high.y : low.y, (i & 4) != 0 ? high.z : low.z};
		const glm::vec4 clip = viewProj * glm::vec4(corner, 1.0f);
		if (clip.w <= 0.01f)
		{
			return std::nullopt;
		}
		const glm::vec2 ndc = glm::vec2(clip) / clip.w;
		least = glm::min(least, ndc);
		most = glm::max(most, ndc);
	}
	least = glm::clamp(least, glm::vec2(-1.0f), glm::vec2(1.0f));
	most = glm::clamp(most, glm::vec2(-1.0f), glm::vec2(1.0f));
	if (least.x >= most.x || least.y >= most.y)
	{
		return std::array<uint16_t, 4> {0, 0, 0, 0};
	}
	// Pixels go down the screen from the top
	const glm::vec2 size {screen};
	const float x0 = std::floor((least.x * 0.5f + 0.5f) * size.x);
	const float x1 = std::ceil((most.x * 0.5f + 0.5f) * size.x);
	const float y0 = std::floor((0.5f - most.y * 0.5f) * size.y);
	const float y1 = std::ceil((0.5f - least.y * 0.5f) * size.y);
	return std::array<uint16_t, 4> {static_cast<uint16_t>(x0), static_cast<uint16_t>(y0), static_cast<uint16_t>(x1 - x0),
	                                static_cast<uint16_t>(y1 - y0)};
}
} // namespace

void Renderer::BindStreamMap(const ShaderProgram& program, uint8_t stage, const Texture2D& fallback) const
{
	glm::vec4 map {0.0f};
	const Texture2D* flow = nullptr;
	if (Locator::streamWaterSystem::has_value() && EffectOn(underwater::Effect::Streams))
	{
		if (_streamView == nullptr)
		{
			_streamView = std::make_unique<UnderwaterStreamView>();
		}
		const auto& system = Locator::streamWaterSystem::value();
		flow = _streamView->FlowMap(system.GetFlowMap(), system.GetRevision());
		if (flow != nullptr)
		{
			map = StreamMapUniform(system.GetFlowMap());
		}
	}
	program.SetTextureSampler("s_streamFlow", stage, flow != nullptr ? *flow : fallback);
	program.SetUniformValue("u_streamMap", &map);
}

void Renderer::DrawStreams(const FrameBuffer& target, const Camera& camera) const
{
	const auto* program = _shaderManager->GetShader("UnderwaterStream");
	if (program == nullptr || !_underwaterFrame.has_value() || _underwater == nullptr ||
	    !Locator::streamWaterSystem::has_value() || !Locator::terrainSystem::has_value() ||
	    !EffectOn(underwater::Effect::Streams) || _underwaterFrame->eye.w > 0.5f)
	{
		return;
	}
	const auto& system = Locator::streamWaterSystem::value();
	const auto& network = system.GetNetwork();
	if (network.segments.empty())
	{
		return;
	}
	if (_streamView == nullptr)
	{
		_streamView = std::make_unique<UnderwaterStreamView>();
	}
	const auto* flow = _streamView->FlowMap(system.GetFlowMap(), system.GetRevision());
	if (flow == nullptr)
	{
		return;
	}
	const auto* stats = bgfx::getStats();
	const glm::u16vec2 screen {stats->width, stats->height};
	// Only the part of the screen over the rivers is drawn
	const auto& map = system.GetFlowMap();
	const glm::vec2 mapLow = map.origin;
	const glm::vec2 mapHigh = map.origin + glm::vec2(map.size - 1) * map.spacing;
	const auto box =
	    ScreenBox(camera.GetViewProjectionMatrix(), {mapLow.x, -5.0f, mapLow.y}, {mapHigh.x, 160.0f, mapHigh.y}, screen);
	if (box.has_value() && ((*box)[2] == 0 || (*box)[3] == 0))
	{
		return;
	}

	const auto& frame = *_underwaterFrame;
	const auto& island = Locator::terrainSystem::value();
	const auto& underwater = Locator::underwaterSystem::value();
	program->SetUniformValue("u_uwParams", &frame.params);
	program->SetUniformValue("u_uwLightColour", &frame.lightColour);
	program->SetUniformValue("u_uwScatter", &frame.scatter);
	program->SetUniformValue("u_uwEye", &frame.eye);
	program->SetUniformValue("u_uwAbove", &frame.above);
	program->SetUniformValue("u_uwWaves", &frame.waves);
	program->SetUniformValue("u_uwSky", &frame.sky);
	program->SetUniformValue("u_uwFlags2", &frame.flags2);
	const auto extent = island.GetExtent();
	const glm::vec4 land {extent.minimum, 1.0f / (extent.maximum - extent.minimum)};
	program->SetUniformValue("u_streamLand", &land);
	const bool sky = frame.sea.w > 0.5f;
	const glm::vec4 params {streams::k_UnitsPerMetre, streams::k_MostDepth, streams::k_Gravity, sky ? 1.0f : 0.0f};
	program->SetUniformValue("u_streamParams", &params);
	// The river's look (see underwater::streams::RiverProfile)
	constexpr auto& k_Profile = streams::k_River;
	const std::array<glm::vec4, 2> profile {
	    glm::vec4(k_Profile.absorption, k_Profile.clarity),
	    glm::vec4(k_Profile.scatter, k_Profile.reflection),
	};
	const glm::vec4 profileWaves {k_Profile.waveScale, k_Profile.windResponse, 0.0f, 0.0f};
	program->SetUniformArray("u_streamProfile", profile.data(), static_cast<uint16_t>(profile.size()));
	program->SetUniformValue("u_streamProfileWaves", &profileWaves);
	std::array<glm::vec4, streams::k_MostMouths> mouths {};
	const auto mouthCount = std::min(network.mouths.size(), mouths.size());
	for (size_t i = 0; i < mouthCount; ++i)
	{
		const auto& mouth = network.mouths[i];
		mouths.at(i) = {mouth.position, mouth.direction * mouth.speed};
	}
	const glm::vec4 info {static_cast<float>(mouthCount), streams::k_LeastSpeed, streams::k_MostSpeed, 0.0f};
	program->SetUniformValue("u_streamInfo", &info);
	program->SetUniformArray("u_streamMouths", mouths.data(), static_cast<uint16_t>(mouths.size()));
	const glm::vec4 mapUniform = StreamMapUniform(map);
	program->SetUniformValue("u_streamMap", &mapUniform);

	program->SetTextureSampler("s_sceneColour", 0, target.GetColorAttachment());
	program->SetTextureSampler("s_sceneDepth", 1, target.GetDepthStencilAttachment());
	program->SetTextureSampler("s_landAlpha", 2, island.GetLandAlphaFramebuffer().GetColorAttachment());
	program->SetTextureSampler("s_streamFlow", 3, *flow);
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
	program->SetTextureSampler("s_blockTextures", 4, island.GetBlockTextures());
	program->SetTextureSampler("s_blockLayers", 5, _underwater->BlockLayers(layers, underwater.GetSeabedRevision()));
	if (sky)
	{
		program->SetTextureSampler("s_environmentSky", 7, _underwater->EnvironmentSky());
	}
	BindWakeField(*program);

	const auto& mesh = Locator::oceanSystem::value().GetMesh();
	mesh.GetIndexBuffer().Bind(mesh.GetIndexBuffer().GetCount(), 0);
	mesh.GetVertexBuffer().Bind();
	if (box.has_value())
	{
		bgfx::setScissor((*box)[0], (*box)[1], (*box)[2], (*box)[3]);
	}
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA));
	program->Submit(static_cast<bgfx::ViewId>(RenderPass::UnderwaterPost));
}

void Renderer::DrawDrifters() const
{
	const auto* program = _shaderManager->GetShader("UnderwaterDrift");
	if (program == nullptr || !_underwaterFrame.has_value() || !Locator::streamWaterSystem::has_value() ||
	    !EffectOn(underwater::Effect::Streams))
	{
		return;
	}
	const auto& system = Locator::streamWaterSystem::value();
	const auto drifters = system.GetDrifters();
	const auto count = static_cast<uint32_t>(drifters.size());
	constexpr uint16_t k_Stride = sizeof(glm::vec4) * 2;
	if (count == 0 || bgfx::getAvailInstanceDataBuffer(count, k_Stride) < count)
	{
		return;
	}
	if (_streamView == nullptr)
	{
		_streamView = std::make_unique<UnderwaterStreamView>();
	}
	bgfx::InstanceDataBuffer instances;
	bgfx::allocInstanceDataBuffer(&instances, count, k_Stride);
	auto* data = reinterpret_cast<glm::vec4*>(instances.data);
	for (uint32_t i = 0; i < count; ++i)
	{
		const auto& drifter = drifters[i];
		// Each fades in as it starts and out at the end of its life; a third are twigs
		const float kind = static_cast<float>(drifter.seed % 1000u) / 1000.0f;
		const float fade =
		    std::clamp(drifter.age / 1.5f, 0.0f, 1.0f) * std::clamp((drifter.life - drifter.age) / 3.0f, 0.0f, 1.0f);
		const float length = (kind < 0.7f ? 0.14f + 0.1f * kind : 0.35f) * streams::k_UnitsPerMetre;
		data[i * 2] = {drifter.position.x, system.DrifterHeight(drifter), drifter.position.y, drifter.angle};
		data[i * 2 + 1] = {length, kind, fade, 0.0f};
	}
	const auto& frame = *_underwaterFrame;
	_streamView->BindQuad();
	bgfx::setInstanceDataBuffer(&instances);
	program->SetUniformValue("u_uwLightColour", &frame.lightColour);
	program->SetUniformValue("u_uwAbove", &frame.above);
	program->SetUniformValue("u_uwSky", &frame.sky);
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_GEQUAL |
	               BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA));
	program->Submit(static_cast<bgfx::ViewId>(RenderPass::UnderwaterPost));
}

void Renderer::DrawTaps(const FrameBuffer& target) const
{
	const auto* program = _shaderManager->GetShader("UnderwaterTap");
	if (program == nullptr || !_underwaterFrame.has_value() || _underwater == nullptr ||
	    !Locator::waterWakeSystem::has_value() || !EffectOn(underwater::Effect::TapSplash))
	{
		return;
	}
	const auto taps = Locator::waterWakeSystem::value().GetFrame().taps;
	const auto count = static_cast<uint32_t>(taps.size());
	constexpr uint16_t k_Stride = sizeof(glm::vec4) * 3;
	if (count == 0 || bgfx::getAvailInstanceDataBuffer(count, k_Stride) < count)
	{
		return;
	}
	if (_streamView == nullptr)
	{
		_streamView = std::make_unique<UnderwaterStreamView>();
	}
	bgfx::InstanceDataBuffer instances;
	bgfx::allocInstanceDataBuffer(&instances, count, k_Stride);
	auto* data = reinterpret_cast<glm::vec4*>(instances.data);
	for (uint32_t i = 0; i < count; ++i)
	{
		const auto& tap = taps[i];
		data[i * 3] = {tap.position, tap.start};
		data[i * 3 + 1] = {tap.splash.sheetHeight, tap.splash.sheetRadius, tap.splash.dropSpeed,
		                   static_cast<float>(tap.splash.drops)};
		data[i * 3 + 2] = {tap.splash.seed, tap.splash.spray};
	}
	const auto& frame = *_underwaterFrame;
	_streamView->BindTapMesh();
	bgfx::setInstanceDataBuffer(&instances);
	program->SetUniformValue("u_uwParams", &frame.params);
	program->SetUniformValue("u_uwLightColour", &frame.lightColour);
	program->SetUniformValue("u_uwScatter", &frame.scatter);
	program->SetUniformValue("u_uwEye", &frame.eye);
	program->SetUniformValue("u_uwAbove", &frame.above);
	program->SetUniformValue("u_uwWaves", &frame.waves);
	program->SetUniformValue("u_uwSky", &frame.sky);
	const glm::vec4 tapParams {streams::k_Gravity, streams::k_UnitsPerMetre, 0.0f, 0.0f};
	program->SetUniformValue("u_tapParams", &tapParams);
	program->SetTextureSampler("s_sceneDepth", 1, target.GetDepthStencilAttachment());
	program->SetTextureSampler("s_sprayNoise", 3, _underwater->SprayNoise());
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA));
	program->Submit(static_cast<bgfx::ViewId>(RenderPass::UnderwaterPost));
}
