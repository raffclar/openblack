/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the field of the wakes and rings on the experiment's water, the hulls that push the water
// down, and the splashes. The original game has none of these.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <algorithm>
#include <array>

#include <bgfx/bgfx.h>

#include "3D/OceanInterface.h"
#include "ECS/Systems/UnderwaterSystemInterface.h"
#include "ECS/Systems/WaterWakeSystemInterface.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/Mesh.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/Underwater.h"
#include "Graphics/UnderwaterWakeField.h"
#include "Graphics/VertexBuffer.h"
#include "Locator.h"
#include "Renderer.h"

using namespace openblack;
using namespace openblack::graphics;
namespace wake = openblack::graphics::underwater::wake;

namespace
{
/// Whether an effect of the experiment is on
bool EffectOn(underwater::Effect effect)
{
	return !Locator::underwaterSystem::has_value() || Locator::underwaterSystem::value().GetEffectOff() != effect;
}
} // namespace

void Renderer::StepWakeField() const
{
	const auto* program = _shaderManager->GetShader("UnderwaterWake");
	if (!_underwaterFrame.has_value() || _underwater == nullptr || program == nullptr ||
	    !Locator::waterWakeSystem::has_value() || !EffectOn(underwater::Effect::WakeField))
	{
		return;
	}
	const auto frame = Locator::waterWakeSystem::value().GetFrame();
	if (frame.plan.steps == 0)
	{
		return;
	}
	if (_wakeField == nullptr)
	{
		_wakeField = std::make_unique<UnderwaterWakeField>();
	}
	const auto& system = Locator::underwaterSystem::value();
	const auto& shoreMap = _underwater->Seabed(system.GetSeabed(), system.GetSeabedRevision());
	const auto& mesh = Locator::oceanSystem::value().GetMesh();

	// The things that write into the field at each step, and the rings that start at the first step
	std::array<glm::vec4, wake::k_MostMovers * 2> movers {};
	for (size_t i = 0; i < frame.movers.size() && i < wake::k_MostMovers; ++i)
	{
		const auto& mover = frame.movers[i];
		movers.at(i * 2) = {mover.position, mover.radius, mover.depth};
		movers.at(i * 2 + 1) = {mover.velocity, mover.foam, 0.0f};
	}
	std::array<glm::vec4, wake::k_MostImpulses> rings {};
	for (size_t i = 0; i < frame.rings.size() && i < wake::k_MostImpulses; ++i)
	{
		const auto& ring = frame.rings[i];
		rings.at(i) = {ring.position, ring.radius, ring.depth};
	}
	constexpr float k_StepSeconds = 1.0f / wake::k_StepsPerSecond;
	const glm::vec4 field {frame.origin, wake::k_FieldSpacing, static_cast<float>(wake::k_FieldSide)};
	const glm::vec4 damp {wake::k_Damping * k_StepSeconds, std::exp(-k_StepSeconds / wake::k_FoamLife), k_StepSeconds, 0.0f};

	constexpr std::array k_Views {RenderPass::UnderwaterWake0, RenderPass::UnderwaterWake1};
	for (uint32_t step = 0; step < frame.plan.steps && step < k_Views.size(); ++step)
	{
		const auto view = k_Views.at(step);
		const auto viewId = static_cast<bgfx::ViewId>(view);
		_wakeField->Next().Bind(view);
		bgfx::setViewRect(viewId, 0, 0, wake::k_FieldSide, wake::k_FieldSide);
		bgfx::setViewClear(viewId, BGFX_CLEAR_NONE);
		const bool first = step == 0;
		const glm::vec4 stepInfo {first ? glm::vec2(frame.plan.shift) : glm::vec2(0.0f),
		                          first && frame.plan.reset ? 1.0f : 0.0f, _wakeField->Gravity()};
		const glm::vec4 counts {static_cast<float>(std::min(frame.movers.size(), wake::k_MostMovers)),
		                        first ? static_cast<float>(std::min(frame.rings.size(), wake::k_MostImpulses)) : 0.0f, 0.0f,
		                        0.0f};
		mesh.GetIndexBuffer().Bind(mesh.GetIndexBuffer().GetCount(), 0);
		mesh.GetVertexBuffer().Bind();
		program->SetUniformValue("u_uwParams", &_underwaterFrame->params);
		program->SetUniformValue("u_wakeField", &field);
		program->SetUniformValue("u_wakeStep", &stepInfo);
		program->SetUniformValue("u_wakeDamp", &damp);
		program->SetUniformValue("u_wakeCounts", &counts);
		program->SetUniformArray("u_wakeKernel", _wakeField->Kernel().data(),
		                         static_cast<uint16_t>(_wakeField->Kernel().size()));
		program->SetUniformArray("u_wakeMovers", movers.data(), static_cast<uint16_t>(movers.size()));
		program->SetUniformArray("u_wakeRings", rings.data(), static_cast<uint16_t>(rings.size()));
		program->SetTextureSampler("s_wakePrev", 0, _wakeField->Latest().GetColorAttachment());
		program->SetTextureSampler("s_shore", 2, shoreMap);
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
		program->Submit(viewId);
		_wakeField->Swap();
	}
}

void Renderer::BindWakeField(const ShaderProgram& program) const
{
	glm::vec4 wakeUniform {0.0f};
	glm::vec4 info {wake::k_FieldSpacing, 1.0f / static_cast<float>(wake::k_FieldSide), 0.0f, 0.0f};
	std::array<glm::vec4, wake::k_MostHulls * 3> hulls {};
	if (Locator::waterWakeSystem::has_value())
	{
		const auto frame = Locator::waterWakeSystem::value().GetFrame();
		if (_wakeField != nullptr && !frame.flat && EffectOn(underwater::Effect::WakeField))
		{
			constexpr float k_Width = wake::k_FieldSide * wake::k_FieldSpacing;
			wakeUniform = {frame.origin, 1.0f / k_Width, 1.0f};
			program.SetTextureSampler("s_wake", 8, _wakeField->Latest().GetColorAttachment());
		}
		const auto count = std::min(frame.hulls.size(), wake::k_MostHulls);
		for (size_t i = 0; i < count; ++i)
		{
			const auto& hull = frame.hulls[i];
			hulls.at(i * 3) = {hull.centre, hull.forward};
			hulls.at(i * 3 + 1) = {hull.half, hull.draft, hull.boat ? 1.0f : 0.0f};
			hulls.at(i * 3 + 2) = {hull.speed, 0.0f, 0.0f, 0.0f};
		}
		info.z = static_cast<float>(count);
		info.w = EffectOn(underwater::Effect::HullMask) ? 1.0f : 0.0f;
	}
	program.SetUniformValue("u_uwWake", &wakeUniform);
	program.SetUniformValue("u_uwWakeInfo", &info);
	program.SetUniformArray("u_uwHulls", hulls.data(), static_cast<uint16_t>(hulls.size()));
}

void Renderer::DrawSplashes(const FrameBuffer& target) const
{
	const auto* program = _shaderManager->GetShader("UnderwaterSplash");
	if (program == nullptr || !_underwaterFrame.has_value() || _underwater == nullptr ||
	    !Locator::waterWakeSystem::has_value() || !EffectOn(underwater::Effect::Splashes) || _underwaterFrame->flags.y < 0.5f)
	{
		return;
	}
	const auto frame = Locator::waterWakeSystem::value().GetFrame();
	const auto count = static_cast<uint32_t>(frame.splashes.size());
	if (count == 0)
	{
		return;
	}
	constexpr uint16_t k_Stride = sizeof(glm::vec4) * 2;
	if (bgfx::getAvailInstanceDataBuffer(count, k_Stride) < count)
	{
		return;
	}
	if (_wakeField == nullptr)
	{
		_wakeField = std::make_unique<UnderwaterWakeField>();
	}
	bgfx::InstanceDataBuffer instances;
	bgfx::allocInstanceDataBuffer(&instances, count, k_Stride);
	auto* data = reinterpret_cast<glm::vec4*>(instances.data);
	for (uint32_t i = 0; i < count; ++i)
	{
		const auto& active = frame.splashes[i];
		data[i * 2] = {active.position, active.start};
		data[i * 2 + 1] = {active.splash.crownHeight, active.splash.crownRadius, active.splash.dropSpeed,
		                   static_cast<float>(active.splash.drops)};
	}
	const auto& uniforms = *_underwaterFrame;
	_wakeField->BindSplashMesh();
	bgfx::setInstanceDataBuffer(&instances);
	program->SetUniformValue("u_uwParams", &uniforms.params);
	program->SetUniformValue("u_uwLightColour", &uniforms.lightColour);
	program->SetUniformValue("u_uwScatter", &uniforms.scatter);
	program->SetUniformValue("u_uwEye", &uniforms.eye);
	program->SetUniformValue("u_uwAbove", &uniforms.above);
	program->SetUniformValue("u_uwWaves", &uniforms.waves);
	program->SetUniformValue("u_uwSky", &uniforms.sky);
	program->SetTextureSampler("s_sceneDepth", 1, target.GetDepthStencilAttachment());
	program->SetTextureSampler("s_sprayNoise", 3, _underwater->SprayNoise());
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA));
	program->Submit(static_cast<bgfx::ViewId>(RenderPass::UnderwaterPost));
}
