/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The renderer is one of the locator's implementations
#define LOCATOR_IMPLEMENTATIONS

#include <limits>
#include <unordered_map>

#include <bgfx/bgfx.h>
#include <glm/gtx/transform.hpp>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/GamePlayers.h"
#include "ECS/PlayerColour.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HeartbeatSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/ScriptHighlightSystemInterface.h"
#include "ECS/Systems/SpellIconSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "EngineConfig.h"
#include "Graphics/LightColumn.h"
#include "Graphics/MeshDetail.h"
#include "Graphics/ObjectDrawWay.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/ViewFrustum.h"
#include "Graphics/ZSort.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
/// Added over what is behind by its texture's alpha times its strength, tested against the depth but leaving none, and
/// only from the front: the game draws the column's faces one-sided, whatever its model's material says
constexpr uint64_t k_AddedState = BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA |
                                  BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE) | BGFX_STATE_CULL_CCW;

/// Added over what is behind by its texture's alpha times its alpha, tested against the depth but leaving none. The
/// model's material draws both sides of its faces.
constexpr uint64_t k_PulseState = BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA |
                                  BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE);

uint32_t ColourOf(PlayerNames player)
{
	return ecs::game_players::Colour(player) & 0xFFFFFFu;
}
} // namespace

void Renderer::DrawTownColumns(const DrawSceneDesc& desc) const
{
	if (desc.viewId != RenderPass::Main || !Locator::influenceSystem::has_value())
	{
		return;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto columnId = resources::HashIdentifier(MeshId::SpellColumn);
	if (!meshes.Contains(columnId))
	{
		return;
	}
	using namespace ecs::components;
	const auto& column = *meshes.Handle(columnId);
	const auto& influence = Locator::influenceSystem::value();
	// The view of the main pass. A column out of it puts no pixel on the screen, so it is not drawn. A warm-up draws all
	// of them.
	const auto seenBy = view_frustum::FromViewProjection(desc.camera->GetProjectionMatrix() *
	                                                     desc.camera->GetViewMatrix(Camera::Interpolation::Current));
	const auto* testedView = _warmingUp || column.IsBoned() ? nullptr : &seenBy;
	// A warm-up draws a column that adds nothing, as the first column of a land draws it
	if (_warmingUp)
	{
		DrawLightShaft(desc, column, desc.camera->GetOrigin(), glm::vec4(0.0f));
	}

	// Over each village centre, at the middle of its model, in its town's player's colour. The columns never fade: the
	// game turns off the fade of their objects when it makes them.
	std::unordered_map<uint32_t, PlayerNames> owners;
	desc.entities.Each<const Town>([&owners](const Town& town) { owners.emplace(town.id, town.owner); });
	desc.entities.Each<const Abode, const Transform, const ecs::components::Mesh>(
	    [&](const Abode& abode, const Transform& transform, const ecs::components::Mesh& mesh) {
		    if (abode.type != AbodeNumber::TownCentre)
		    {
			    return;
		    }
		    const auto owner = owners.find(abode.townId);
		    if (owner == owners.end() || !light_column::Shown(owner->second, influence.IsBorderShown(owner->second)) ||
		        !meshes.Contains(mesh.id))
		    {
			    return;
		    }
		    const auto centre = meshes.Handle(mesh.id)->GetBoundingBox().Center();
		    const auto model = glm::translate(transform.position) * glm::mat4(transform.rotation) * glm::scale(transform.scale);
		    DrawLightShaft(desc, column, glm::vec3(model * glm::vec4(centre, 1.0f)),
		                   light_column::Tint(ColourOf(owner->second), light_column::k_TownAlpha), testedView);
	    });
}

void Renderer::DrawTempleColumns(const DrawSceneDesc& desc) const
{
	// The temple's column is seen only from outside, so it is not drawn while the player is inside the temple
	if (desc.viewId != RenderPass::Main || !Locator::influenceSystem::has_value() ||
	    (Locator::temple::has_value() && Locator::temple::value().Active()))
	{
		return;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto columnId = resources::HashIdentifier(MeshId::SpellColumn);
	if (!meshes.Contains(columnId))
	{
		return;
	}
	using namespace ecs::components;
	const auto& column = *meshes.Handle(columnId);
	const auto& influence = Locator::influenceSystem::value();
	const auto seenBy = view_frustum::FromViewProjection(desc.camera->GetProjectionMatrix() *
	                                                     desc.camera->GetViewMatrix(Camera::Interpolation::Current));
	const auto* testedView = _warmingUp || column.IsBoned() ? nullptr : &seenBy;
	// Over each temple, where it stands, beating with its player's heart
	const float turnFraction = Locator::time::has_value() ? Locator::time::value().GetTurnFraction() : 0.0f;
	desc.entities.Each<const Temple, const Transform>([&](const Temple& temple, const Transform& transform) {
		if (!light_column::Shown(temple.owner, influence.IsBorderShown(temple.owner)) ||
		    !light_column::TempleShown(temple.destroying, temple.destructionClock))
		{
			return;
		}
		const float pulse =
		    Locator::heartbeatSystem::has_value() ? Locator::heartbeatSystem::value().Pulse(temple.owner, turnFraction) : 0.0f;
		DrawLightShaft(desc, column, transform.position,
		               light_column::Tint(ColourOf(temple.owner), light_column::TempleAlpha(pulse)), testedView);
	});
}

void Renderer::DrawLightShaft(const DrawSceneDesc& desc, const L3DMesh& mesh, const glm::vec3& position, const glm::vec4& tint,
                              const view_frustum::Frustum* seenBy, bool fades) const
{
	const auto eye = desc.camera->GetOrigin();
	const auto model = light_column::Model(position, eye);
	const auto box = mesh.GetBoundingBox();
	if (seenBy != nullptr && !view_frustum::SeesBox(*seenBy, model, box.minima, box.maxima))
	{
		return;
	}
	// The bands go by the middle of the shaft's box. The shaft draws every part of its mesh in every band, thus only the
	// end of the bands changes the draw. The warm-up draws the shaft at the eye, where the bands do not end.
	if (fades && !_warmingUp)
	{
		const auto centre = glm::vec3(model * glm::vec4(box.Center(), 1.0f));
		const float reach = mesh_detail::Reach(0.0f, mesh_detail::ScaledRadius(box.Size(), light_column::k_ObjectSize),
		                                       Locator::config::value().detail.modelDetail);
		const auto choice = mesh_detail::Choose(mesh_detail::ViewDepth(centre, eye, desc.camera->GetForward()), reach, true);
		if (!object_draw::DrawBanded(choice, 0, true).has_value())
		{
			return;
		}
	}
	L3DMeshSubmitDesc submitDesc = {};
	submitDesc.viewId = TranslucentPassOf(desc.viewId);
	submitDesc.program = _shaderManager->GetShader("Object");
	submitDesc.modelMatrices = &model;
	submitDesc.matrixCount = 1;
	submitDesc.state = k_AddedState;
	submitDesc.alphaThreshold = 0.0f;
	submitDesc.tint = tint;
	// The shaft is its colour alone: neither the land's light and colour nor the distance haze are added to it, and the
	// creatures' and the hand's shadows don't fall on it
	submitDesc.landLightAndHaze = false;
	submitDesc.groundShadows = false;
	submitDesc.sortDepth = zsort::ModelDepth(position, eye);
	DrawMesh(mesh, submitDesc, std::numeric_limits<uint8_t>::max());
}

void Renderer::DrawScrollBeams(const DrawSceneDesc& desc) const
{
	if (desc.viewId != RenderPass::Main || !Locator::scriptHighlightSystem::has_value())
	{
		return;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto beamId = resources::HashIdentifier(MeshId::SpellBlastCentre);
	if (!meshes.Contains(beamId))
	{
		return;
	}
	const auto& beam = *meshes.Handle(beamId);
	const float turnFraction = Locator::time::has_value() ? Locator::time::value().GetTurnFraction() : 0.0f;
	const float pulseShare =
	    ecs::script_highlights::PulseShare(Locator::scriptHighlightSystem::value().GetPulse(), turnFraction);

	// A started scroll stands in a beam the same shape as the temple's column, rising from its model's middle. A beam
	// with no strength this frame adds nothing, thus the renderer does not send it. The beam's object keeps the fade
	// that the game gives every object, thus the draw stops where the beam's distance bands end.
	desc.entities.Each<const ecs::components::ScriptHighlight>([&](const ecs::components::ScriptHighlight& highlight) {
		if (highlight.hidden || !ecs::script_highlights::BeamShown(highlight.kind, highlight.active))
		{
			return;
		}
		const uint32_t colour = ecs::script_highlights::BeamColour(highlight.kind, pulseShare);
		if ((colour >> 24u) == 0)
		{
			return;
		}
		DrawLightShaft(desc, beam, highlight.centre,
		               light_column::Tint(colour & 0xFFFFFFu, static_cast<uint8_t>(colour >> 24u)), nullptr, true);
	});
}

void Renderer::DrawIconPulses(const DrawSceneDesc& desc) const
{
	if (desc.viewId != RenderPass::Main || !Locator::spellIconSystem::has_value())
	{
		return;
	}
	const auto pulses = Locator::spellIconSystem::value().ChargePulses();
	if (pulses.empty())
	{
		return;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto pulseId = resources::HashIdentifier(MeshId::SpellPulseIn);
	if (!meshes.Contains(pulseId))
	{
		return;
	}
	const auto& mesh = *meshes.Handle(pulseId);
	const auto eye = desc.camera->GetOrigin();
	const auto forward = desc.camera->GetForward();
	const auto box = mesh.GetBoundingBox();
	// The pulse goes through the object draw by its distance bands. The game never gives the pulse's object a size, so
	// the bands go by the size of its mesh alone, however large the pulse is drawn.
	const float reach =
	    mesh_detail::Reach(0.0f, mesh_detail::ScaledRadius(box.Size(), 1.0f), Locator::config::value().detail.modelDetail);
	for (const auto& pulse : pulses)
	{
		// The pulse draws every part of its mesh in every band. In the fade band, the alpha of the fade takes the place
		// of the pulse's own alpha. After the fade band, the pulse does not draw.
		auto alpha = static_cast<uint8_t>(pulse.argb >> 24u);
		if (!_warmingUp)
		{
			const auto centre = glm::vec3(pulse.model * glm::vec4(box.Center(), 1.0f));
			const auto banded = object_draw::DrawBanded(
			    mesh_detail::Choose(mesh_detail::ViewDepth(centre, eye, forward), reach, true), alpha, false);
			if (!banded.has_value())
			{
				continue;
			}
			alpha = banded->alpha;
		}
		L3DMeshSubmitDesc submitDesc = {};
		submitDesc.viewId = TranslucentPassOf(desc.viewId);
		submitDesc.program = _shaderManager->GetShader("Object");
		submitDesc.modelMatrices = &pulse.model;
		submitDesc.matrixCount = 1;
		submitDesc.state = k_PulseState;
		submitDesc.alphaThreshold = 0.0f;
		submitDesc.tint = light_column::Tint(pulse.argb & 0xFFFFFFu, alpha);
		// The pulse is its colour alone, as the light shafts are: no light, colour or haze of the land, and no shadows
		submitDesc.landLightAndHaze = false;
		submitDesc.groundShadows = false;
		// The pulse's mesh is sorted whole, so the pulse takes the key of a placed model
		submitDesc.sortDepth = zsort::ModelDepth(glm::vec3(pulse.model[3]), eye);
		DrawMesh(mesh, submitDesc, std::numeric_limits<uint8_t>::max());
	}
}
