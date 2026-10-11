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
#include "Camera/Camera.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HeartbeatSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Graphics/LightColumn.h"
#include "Graphics/ShaderManager.h"
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

uint32_t ColourOf(PlayerNames player)
{
	return ecs::components::Player::k_Colours.at(static_cast<size_t>(player)) & 0xFFFFFFu;
}
} // namespace

void Renderer::DrawLightColumns(const DrawSceneDesc& desc) const
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
	const auto eye = desc.camera->GetOrigin();
	const auto* program = _shaderManager->GetShader("Object");
	const auto translucent = TranslucentPassOf(desc.viewId);

	const auto draw = [&](const glm::vec3& position, PlayerNames owner, uint8_t alpha) {
		const auto model = light_column::Model(position, eye);
		L3DMeshSubmitDesc submitDesc = {};
		submitDesc.viewId = translucent;
		submitDesc.program = program;
		submitDesc.modelMatrices = &model;
		submitDesc.matrixCount = 1;
		submitDesc.state = k_AddedState;
		submitDesc.alphaThreshold = 0.0f;
		submitDesc.tint = light_column::Tint(ColourOf(owner), alpha);
		// The column is its owner's colour alone: neither the land's light and colour nor the distance haze are added to
		// it, and the creatures' and the hand's shadows don't fall on it
		submitDesc.landLightAndHaze = false;
		submitDesc.groundShadows = false;
		submitDesc.sortDepth = zsort::Depth(position, eye);
		DrawMesh(column, submitDesc, std::numeric_limits<uint8_t>::max());
	};

	// Over each village centre, at the middle of its model, in its town's player's colour
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
		    draw(glm::vec3(model * glm::vec4(centre, 1.0f)), owner->second, light_column::k_TownAlpha);
	    });

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
		draw(transform.position, temple.owner, light_column::TempleAlpha(pulse));
	});
}
