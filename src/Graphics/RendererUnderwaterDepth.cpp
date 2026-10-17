/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the foam that builds up on the experiment's water, and the silt in the water. The
// original game has neither.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <optional>

#include <bgfx/bgfx.h>
#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "3D/OceanInterface.h"
#include "Camera/Camera.h"
#include "ECS/Systems/UnderwaterSystemInterface.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/Mesh.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/Underwater.h"
#include "Graphics/UnderwaterFoam.h"
#include "Graphics/VertexBuffer.h"
#include "Locator.h"
#include "Renderer.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
/// Whether the foam field shows with the effect that is off
bool FoamFieldOn(underwater::Effect off)
{
	return off != underwater::Effect::FoamVolume && off != underwater::Effect::Foam;
}
} // namespace

void Renderer::StepUnderwaterFoam(const DrawSceneDesc& desc) const
{
	if (!_underwaterFrame.has_value() || _underwater == nullptr || !Locator::underwaterSystem::has_value() ||
	    !Locator::oceanSystem::has_value())
	{
		return;
	}
	const auto& system = Locator::underwaterSystem::value();
	const auto* program = _shaderManager->GetShader("UnderwaterFoam");
	if (program == nullptr || !FoamFieldOn(system.GetEffectOff()))
	{
		return;
	}
	const auto& frame = *_underwaterFrame;
	// The field follows the place that the camera looks at
	const glm::mat4 toWorld = glm::inverse(desc.camera->GetViewMatrix(Camera::Interpolation::Current));
	const glm::vec3 eye {toWorld[3]};
	const glm::vec3 forward = glm::normalize(glm::vec3(toWorld[2]));
	const auto focus = underwater::FoamFocus(eye, forward, frame.params.z);
	const auto step = _underwater->Foam().Advance(frame.params.y, focus, system.GetSeabedRevision());
	if (!step.has_value())
	{
		return;
	}

	const auto view = static_cast<bgfx::ViewId>(RenderPass::UnderwaterFoam);
	bgfx::setViewFrameBuffer(view, _underwater->FoamTarget(step->target));
	bgfx::setViewRect(view, 0, 0, underwater::k_FoamTexels, underwater::k_FoamTexels);
	bgfx::setViewClear(view, BGFX_CLEAR_NONE);

	program->SetUniformValue("u_uwParams", &frame.params);
	program->SetUniformValue("u_uwWaves", &frame.waves);
	program->SetUniformValue("u_uwPhase", &frame.phase);
	program->SetUniformValue("u_uwCurrent", &frame.current);
	program->SetUniformValue("u_uwSea", &frame.sea);
	program->SetUniformArray("u_uwWaveTable", frame.waveTable.data(), static_cast<uint16_t>(frame.waveTable.size()));
	program->SetUniformArray("u_uwFarWaves", frame.farWaves.data(), static_cast<uint16_t>(frame.farWaves.size()));
	const glm::vec4 field {step->origin, 1.0f, underwater::k_FoamWindow};
	program->SetUniformValue("u_uwFoam", &field);
	const glm::vec4 stepUniform {step->seconds, step->clear ? 1.0f : 0.0f, step->previousOrigin};
	program->SetUniformValue("u_uwFoamStep", &stepUniform);
	program->SetTextureSampler("s_foamPrevious", 0, _underwater->FoamTexture(static_cast<uint8_t>(1 - step->target)));
	program->SetTextureSampler("s_shore", 2, _underwater->Seabed(system.GetSeabed(), system.GetSeabedRevision()));

	// The sea's triangle over the whole target
	const auto& mesh = Locator::oceanSystem::value().GetMesh();
	mesh.GetIndexBuffer().Bind(mesh.GetIndexBuffer().GetCount(), 0);
	mesh.GetVertexBuffer().Bind();
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
	program->Submit(view);
}

void Renderer::BindUnderwaterDepth(const ShaderProgram& program, uint8_t foamStage, std::optional<uint8_t> noiseStage) const
{
	if (!_underwaterFrame.has_value() || _underwater == nullptr || !Locator::underwaterSystem::has_value())
	{
		return;
	}
	const auto& system = Locator::underwaterSystem::value();
	const auto off = system.GetEffectOff();
	const auto& clock = _underwater->Foam();
	const bool foamOn = FoamFieldOn(off) && clock.Ready();
	const glm::vec4 field {clock.Origin(), foamOn ? 1.0f : 0.0f, underwater::k_FoamWindow};
	program.SetUniformValue("u_uwFoam", &field);
	if (foamOn)
	{
		program.SetTextureSampler("s_foam", foamStage, _underwater->FoamTexture(clock.Latest()));
	}
	if (noiseStage.has_value())
	{
		const auto& frame = *_underwaterFrame;
		const bool sedimentOn = off != underwater::Effect::Sediment;
		const glm::vec4 sediment {sedimentOn ? 1.0f : 0.0f, underwater::SedimentStir(frame.waves.w, frame.waves.y), 0.0f, 0.0f};
		program.SetUniformValue("u_uwSediment", &sediment);
		program.SetTextureSampler("s_sedimentNoise", *noiseStage, _underwater->SprayNoise());
	}
}
