/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BuildingReplan.h"

#include <algorithm>

#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Construction.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/PlannedBuildingRules.h"
#include "ECS/Registry.h"

namespace openblack::ecs
{

using components::Abode;
using components::BuildProgress;
using components::PlannedAbode;
using components::SpellDispenser;
using components::Town;
using components::Transform;

std::optional<building_replan::Replan> building_replan::Of(const Registry& registry, entt::entity building)
{
	if (!registry.Valid(building))
	{
		return std::nullopt;
	}
	const auto* transform = registry.TryGet<const Transform>(building);
	if (transform == nullptr)
	{
		return std::nullopt;
	}
	Replan replan;
	bool notPlannedAgain = false;
	if (const auto* abode = registry.TryGet<const Abode>(building))
	{
		const auto& towns = registry.Context().towns;
		const auto town = towns.find(abode->townId);
		if (town == towns.end() || !registry.Valid(town->second))
		{
			return std::nullopt;
		}
		replan.townId = abode->townId;
		replan.info = abode->info;
		notPlannedAgain = abode->notPlannedAgain;
	}
	else if (const auto* dispenser = registry.TryGet<const SpellDispenser>(building))
	{
		// A miracle dispenser keeps its town by the town's entity
		const auto* town = registry.Valid(dispenser->town) ? registry.TryGet<const Town>(dispenser->town) : nullptr;
		if (town == nullptr)
		{
			return std::nullopt;
		}
		replan.townId = town->id;
		replan.info = dispenser->building;
		notPlannedAgain = dispenser->notPlannedAgain;
	}
	else
	{
		return std::nullopt;
	}
	if (replan.info == AbodeInfo::None || notPlannedAgain)
	{
		return std::nullopt;
	}
	replan.position = transform->position;
	replan.yAngle = planned_building::YAngleOf(transform->rotation);
	replan.scale = transform->scale.x;
	replan.wasBuilt = !registry.AllOf<BuildProgress>(building);
	return replan;
}

bool building_replan::TakesPlaceOf(const Replan& gone, uint32_t townId, AbodeInfo info, glm::vec3 position)
{
	// A thing nearer than this to the place, across the ground, stands at the place, in metres
	constexpr float k_SamePlace = 0.01f;
	return townId == gone.townId && info == gone.info && glm::distance(glm::xz(position), glm::xz(gone.position)) < k_SamePlace;
}

entt::entity building_replan::PlanLeftAt(const Registry& registry, entt::entity building)
{
	const auto replan = Of(registry, building);
	if (!replan.has_value())
	{
		return entt::null;
	}
	const auto& towns = registry.Context().towns;
	const auto town = towns.find(replan->townId);
	if (town == towns.end() || !registry.Valid(town->second))
	{
		return entt::null;
	}
	const auto& planned = registry.Get<const Town>(town->second).planned;
	const auto found = std::ranges::find_if(planned, [&](entt::entity plan) {
		if (!registry.Valid(plan))
		{
			return false;
		}
		const auto* abode = registry.TryGet<const PlannedAbode>(plan);
		const auto* transform = registry.TryGet<const Transform>(plan);
		return abode != nullptr && transform != nullptr &&
		       TakesPlaceOf(*replan, abode->townId, abode->info, transform->position);
	});
	return found != planned.end() ? *found : entt::null;
}

} // namespace openblack::ecs
