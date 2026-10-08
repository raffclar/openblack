/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "RenderingSystemTemple.h"

#include <algorithm>
#include <utility>

#include <glm/gtx/transform.hpp>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/ObjectMatrix.h"
#include "3D/TempleInteriorInterface.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/TempleDrawPlan.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

namespace
{
/// The hand of the player, whose hand alone is in the temple
entt::entity PlayerHand()
{
	if (!openblack::Locator::handSystem::has_value())
	{
		return entt::null;
	}
	return openblack::Locator::handSystem::value().GetPlayerHands()[0];
}
} // namespace

RenderingSystemTemple::~RenderingSystemTemple() = default;

bool RenderingSystemTemple::AllBlended(entt::id_type meshId)
{
	// The temple's meshes don't change while the player is inside, so each is looked through once
	if (const auto known = _allBlended.find(meshId); known != _allBlended.end())
	{
		return known->second;
	}
	const auto mesh = Locator::resources::value().GetMeshes().Handle(meshId);
	const bool allBlended = std::ranges::none_of(mesh->GetSubMeshes(), [](const auto& subMesh) {
		return std::ranges::any_of(subMesh->GetPrimitives(), [](const auto& primitive) {
			return primitive.blend == graphics::L3DSubMesh::Primitive::BlendMode::Disabled;
		});
	});
	_allBlended.emplace(meshId, allBlended);
	return allBlended;
}

void RenderingSystemTemple::PrepareDraw(bool drawBoundingBox, bool drawFootpaths, bool drawStreams)
{
	// Which rooms are drawn, the doors' turns, the scrolls and the glows under the cursor change without any component
	// changing, so the draw lists are made again every frame inside the temple
	SetLayoutDirty();
	RenderingSystemCommon::PrepareDraw(drawBoundingBox, drawFootpaths, drawStreams);
}

void RenderingSystemTemple::PrepareDrawDescs(bool drawBoundingBox)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& temple = Locator::temple::value();

	// The parts, then which of them are drawn and how
	_parts.clear();
	_partEntities.clear();
	registry.Each<const Mesh, const Transform, const TempleInteriorPart>(
	    [this](entt::entity entity, const Mesh& mesh, const Transform& /* unused */, const TempleInteriorPart& part) {
		    _parts.push_back({.meshId = mesh.id, .room = part.room, .mesh = part.mesh});
		    _partEntities.push_back(entity);
	    },
	    entt::exclude<Unavailable>);
	const auto rooms = temple_draw::ChooseRooms([&temple](TempleRoom room) { return temple.IsRoomDrawn(room); });
	auto plan = temple_draw::PlanParts(_parts, rooms, temple.GetCurrentRoom());

	_drawn.clear();
	for (size_t i = 0; i < _partEntities.size(); ++i)
	{
		if (plan.drawnParts[i])
		{
			_drawn.push_back(_partEntities[i]);
		}
	}
	// The game draws the player's hand in the temple too
	const auto playerHand = PlayerHand();
	registry.Each<const Mesh, const Transform, const Hand>(
	    [this, &plan, playerHand](entt::entity entity, const Mesh& mesh, const Transform& /* unused */,
	                              const Hand& /* unused */) {
		    if (entity == playerHand)
		    {
			    ++plan.meshes[mesh.id].instances;
			    _drawn.push_back(entity);
		    }
	    },
	    entt::exclude<Unavailable>);

	auto instanceCount = static_cast<uint32_t>(_drawn.size());
	if (drawBoundingBox)
	{
		instanceCount *= 2;
	}

	// Recreate instancing uniform buffer if it is too small
	if (_renderContext.instanceUniforms.size() < instanceCount)
	{
		ResizeInstances(instanceCount);
	}

	// Determine uniform buffer offsets and instance count for draw
	uint32_t offset = 0;
	_renderContext.instancedDrawDescs.clear();
	for (const auto& [meshId, meshPlan] : plan.meshes)
	{
		if (meshPlan.instances == 0)
		{
			continue;
		}
		RenderContext::InstancedDrawDesc desc(offset, meshPlan.instances, false);
		const bool hand = meshId == Hand::k_MeshId;
		temple_draw::ApplyPlan(meshPlan, hand, !hand && AllBlended(meshId), desc);
		// The game draws each scroll with its own material, the texture its room wrote
		if (meshPlan.room.has_value())
		{
			for (const auto& scroll : temple.GetScrollTextures(*meshPlan.room))
			{
				desc.subMeshTextures.emplace_back(scroll.subMesh, scroll.texture);
			}
			for (const auto& glow : temple.GetControlGlows(*meshPlan.room))
			{
				desc.subMeshGlows.emplace_back(glow.subMesh, glow.colour);
			}
			desc.hiddenSubMeshes = temple.GetHiddenSubMeshes(*meshPlan.room);
		}
		// The creature's room slides its water's texture down it
		if (meshPlan.water)
		{
			desc.uvOffset = temple.GetWaterfallSlide();
		}
		_renderContext.instancedDrawDescs.emplace(meshId, std::move(desc));
		offset += meshPlan.instances;
	}
}

void RenderingSystemTemple::PrepareDrawUploadUniforms(bool drawBoundingBox)
{
	auto& registry = Locator::entitiesRegistry::value();

	// Each mesh's instances are written one after another from its draw list's offset
	_written.clear();
	for (const auto entity : _drawn)
	{
		const auto& [mesh, transform] = registry.Get<const Mesh, const Transform>(entity);
		const auto desc = _renderContext.instancedDrawDescs.find(mesh.id);
		if (desc == _renderContext.instancedDrawDescs.end())
		{
			continue;
		}
		auto written = std::ranges::find(_written, mesh.id, &std::pair<entt::id_type, uint32_t>::first);
		if (written == _written.end())
		{
			written = _written.emplace(_written.end(), mesh.id, 0);
		}
		auto& index = written->second;
		const auto modelMatrix = openblack::affine::Model(transform); // T(p) R S, as RenderingSystem
		const uint32_t idx = desc->second.offset + index;
		_renderContext.instanceUniforms[idx] = modelMatrix;
		if (drawBoundingBox)
		{
			const auto box = Locator::resources::value().GetMeshes().Handle(mesh.id)->GetBoundingBox();
			const auto boxMatrix = modelMatrix * glm::translate(box.Center()) * glm::scale(box.Size());
			_renderContext.instanceUniforms[idx + _renderContext.instanceUniforms.size() / 2] = boxMatrix;
		}
		++index;
	}

	UploadInstances();
}
