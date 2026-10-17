/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AbodeSurroundings.h"

#include <cmath>

#include <algorithm>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "3D/MapCoords.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ScriptHighlightSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

std::optional<glm::vec3> abode_surroundings::FindPoint(std::span<const graphics::L3DMesh::EffectPoint> points, uint32_t kind)
{
	const auto found = std::ranges::find(points, kind, &graphics::L3DMesh::EffectPoint::kind);
	if (found == points.end())
	{
		return std::nullopt;
	}
	return found->position;
}

glm::vec3 abode_surroundings::PlacePoint(const glm::mat4& placement, glm::vec3 point)
{
	return glm::vec3(placement * glm::vec4(point, 1.0f));
}

bool abode_surroundings::InSameCell(glm::vec3 a, glm::vec3 b)
{
	const auto cell = [](glm::vec3 p) {
		return glm::vec2(std::floor(p.x / map_coords::k_CellSize), std::floor(p.z / map_coords::k_CellSize));
	};
	return cell(a) == cell(b);
}

namespace
{

/// The building's row, when it is a building that puts things round itself: a field puts nothing
const GAbodeInfo* SurroundedInfoOf(entt::entity abode)
{
	const auto* info = world_objects::AbodeInfoOf(abode);
	return info != nullptr && info->abodeType != AbodeType::Field ? info : nullptr;
}

/// Where the building's tip sign stands in the world, when its model has a sign point
std::optional<glm::vec3> TipSignPlace(entt::entity abode, const GAbodeInfo& info)
{
	if (!Locator::resources::has_value())
	{
		return std::nullopt;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto id = resources::HashIdentifier(info.meshId);
	if (!meshes.Contains(id))
	{
		return std::nullopt;
	}
	const auto point = abode_surroundings::FindPoint(meshes.Handle(id)->GetEffectPoints(), abode_surroundings::k_TipSignPoint);
	if (!point.has_value())
	{
		return std::nullopt;
	}
	return abode_surroundings::PlacePoint(world_objects::PlacementOf(abode), *point);
}

} // namespace

void abode_surroundings::Create(entt::entity abode)
{
	const auto* info = SurroundedInfoOf(abode);
	if (info == nullptr || !HasTipSign(static_cast<uint32_t>(info->didYouKnow)) || !Locator::scriptHighlightSystem::has_value())
	{
		return;
	}
	const auto place = TipSignPlace(abode, *info);
	if (!place.has_value())
	{
		return;
	}
	// A tip sign, of no challenge, that gives the building's tip and stands on the land
	auto& highlights = Locator::scriptHighlightSystem::value();
	const auto sign = highlights.Create(static_cast<uint32_t>(HighlightInfo::DidYouKnowSign), *place, 0);
	if (sign == entt::null)
	{
		return;
	}
	highlights.SetProperties(sign, static_cast<uint32_t>(info->didYouKnow), static_cast<uint32_t>(info->dykCategory));
	highlights.SetDrawHeight(sign, 0.0f);
}

void abode_surroundings::Delete(entt::entity abode)
{
	const auto* info = SurroundedInfoOf(abode);
	if (info == nullptr || !HasTipSign(static_cast<uint32_t>(info->didYouKnow)))
	{
		return;
	}
	const auto place = TipSignPlace(abode, *info);
	if (!place.has_value())
	{
		return;
	}
	// Every scroll or sign in that cell goes, whoever put it there
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> signs;
	registry.Each<const ScriptHighlight, const Transform>(
	    [&signs, &place](entt::entity entity, const ScriptHighlight& /*unused*/, const Transform& transform) {
		    if (InSameCell(transform.position, *place))
		    {
			    signs.push_back(entity);
		    }
	    });
	for (const auto sign : signs)
	{
		world_objects::Remove(sign);
	}
}
