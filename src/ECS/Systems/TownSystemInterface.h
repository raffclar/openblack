/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>

#include <entt/fwd.hpp>
#include <glm/fwd.hpp>
#include <glm/vec2.hpp>

namespace openblack::ecs::systems
{
class TownSystemInterface
{
public:
	[[nodiscard]] virtual entt::entity FindAbodeWithSpace(entt::entity townEntity) const = 0;
	[[nodiscard]] virtual entt::entity FindClosestTown(const glm::vec3& point) const = 0;
	virtual void AddHomelessVillagerToTown(entt::entity townEntity, entt::entity villagerEntity) = 0;

	/// What stands in the way of ground kept clear: for a town's gathering place, anything fixed but a tree; for a villager
	/// to sit down, anything at all
	enum class ClearAreaFilter : uint8_t
	{
		BlocksTown,
		AnyObject,
	};
	/// Whether nothing of the kind stands within a radius of a point, the things' own sizes counted, the cells about it
	/// looked at in a spiral; the thing ignored doesn't count
	[[nodiscard]] virtual bool CheckForClearArea(glm::vec2 point, float radius, ClearAreaFilter filter,
	                                             entt::entity ignore) const = 0;
	/// The first clear area of a radius in a spiral out from a point in steps, as far as a search radius; none if none is
	[[nodiscard]] virtual std::optional<glm::vec2> FindClearArea(glm::vec2 point, float searchRadius, float step, float radius,
	                                                             ClearAreaFilter filter, entt::entity ignore) const = 0;
	/// Where a town's people gather: worked out once, between its buildings or near one
	virtual glm::vec2 GetCongregationPos(entt::entity town) = 0;
	/// A new building standing on a town's gathering place moves it
	virtual void BuildingCreated(entt::entity town, glm::vec2 position, float radius) = 0;

	/// Whether a town is in an emergency: for a while after one was last called
	[[nodiscard]] virtual bool IsInStateOfEmergency(entt::entity town) const = 0;
	/// Calls an emergency in a town: the first call has its people gather
	virtual void SetInStateOfEmergency(entt::entity town) = 0;
	/// Once a game turn: a burning store or village centre calls an emergency, and one that has run its time ends
	virtual void ProcessTurn() = 0;
};
} // namespace openblack::ecs::systems
