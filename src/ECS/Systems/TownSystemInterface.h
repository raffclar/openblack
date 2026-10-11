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
#include <vector>

#include <entt/fwd.hpp>
#include <glm/fwd.hpp>
#include <glm/vec2.hpp>

#include "ECS/TownHomes.h"
#include "Enums.h"

namespace openblack::ecs::systems
{
class TownSystemInterface
{
public:
	/// The functional building of a town that suits a villager best, scoring above a least score; none if none does
	[[nodiscard]] virtual entt::entity FindAbodeWithSpace(entt::entity town, entt::entity villager, float leastScore) const = 0;
	[[nodiscard]] virtual entt::entity FindClosestTown(const glm::vec3& point) const = 0;
	/// The building of any town, any player's or none, nearest a point on the ground; the first of equals by player, the
	/// players' towns before the neutral ones
	[[nodiscard]] virtual entt::entity NearestBuilding(glm::vec2 point) const = 0;
	/// A villager joins a town, moving into the building that suits it best or joining the homeless; false for a town
	/// nobody can live in
	virtual bool AddVillagerToTown(entt::entity town, entt::entity villager) = 0;
	/// A villager moves into a building, leaving the homeless or its old home
	virtual void AddVillagerToAbode(entt::entity abode, entt::entity villager) = 0;
	/// A villager leaves its home, keeping its town, and joins the town's homeless; false with no town or when it
	/// already is one of them
	virtual bool MakeHomeless(entt::entity villager) = 0;
	/// A villager moves into a building whatever its room, from another town too: there, into a full building, it only
	/// joins the town
	virtual void MoveInAnyway(entt::entity abode, entt::entity villager) = 0;
	/// A villager put down by a building moves in when there is room for its age, else tries the building's town. Whether
	/// it moved.
	virtual bool MoveInByBuilding(entt::entity abode, entt::entity villager) = 0;
	/// A villager moves into another town's best building, or with room in the town, its village centre. Whether it did.
	virtual bool MoveIntoTown(entt::entity town, entt::entity villager) = 0;
	/// An adult whose home is too crowded, or who has none, moves into a better building of its town or is homeless
	virtual town_homes::NewHome LookForBetterHome(entt::entity villager) = 0;
	/// Whether a town has room for another adult in its homes
	[[nodiscard]] virtual bool HasRoomForNewcomer(entt::entity town) const = 0;
	/// A villager has died: the children of its town whose mother it was lose her, and it leaves its home and its town
	virtual void RemoveDeadVillager(entt::entity villager) = 0;
	/// The people of a town whose mother a villager is, in the town's order
	[[nodiscard]] virtual std::vector<entt::entity> ChildrenOf(entt::entity town, entt::entity mother) const = 0;

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
	/// Once a game turn: a burning store or village centre calls an emergency, and one that has run its time ends; the
	/// missionaries preaching in each town follow their villagers or stop; a town's mark of changed wants comes down once
	/// it has been up a whole turn
	virtual void ProcessTurn() = 0;
	/// A town's wants have changed: a store stocked from empty, a building site added, a death. Its disciples waiting for
	/// work look again soon.
	virtual void MarkDesiresChanged(entt::entity town) = 0;
	/// Whether a town's wants have just changed
	[[nodiscard]] virtual bool DesiresChanged(entt::entity town) const = 0;

	/// A player takes a town. Its people stop worshipping and it leaves its worship site; it becomes the player's, after
	/// their other towns, joining their worship site and influence, its totem turning to them; its people celebrate,
	/// and for any player but the neutral one fireworks and a fountain of the player's symbols rise from its centre.
	/// The player at this machine hears the town won. Nothing happens if the town is theirs already.
	virtual void ClaimTown(entt::entity town, PlayerNames player) = 0;
	/// Each frame, gameSeconds on: every village centre shows its players' symbols of belief over its totem, made with
	/// the centre and stepped as it is drawn
	virtual void UpdateFrame(float /*gameSeconds*/) {}
};
} // namespace openblack::ecs::systems
