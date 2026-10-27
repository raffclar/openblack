/******************************************************************************
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

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::systems
{

/// The towns' workshops (components::Workshop) and the scaffolds that they make (components::Scaffold): a workshop makes
/// scaffolds from its wood and keeps them in its yard, and the hand carries the scaffolds away
class WorkshopSystemInterface
{
public:
	virtual ~WorkshopSystemInterface() = default;

	/// The turn of a workshop, in the pass of its town over its buildings: it starts or finishes a scaffold, and it lets
	/// go of the scaffolds that it does not keep now
	virtual void ProcessWorkshop(entt::entity workshop) = 0;

	/// The room of a workshop for more scaffolds (see disciple_work::WorkshopRoom); 0 for what is not a workshop
	[[nodiscard]] virtual int32_t RoomOf(entt::entity workshop) const = 0;

	/// The pile that holds the wood of a workshop. The workshop makes the pile when it has none, at the special point of
	/// its wood pile. None for what is not a workshop.
	virtual entt::entity WoodPileOf(entt::entity workshop) = 0;

	/// Makes a scaffold of a scale at a place on the land, for a town and a workshop, turned by an angle about the up axis.
	/// The town and the workshop can be none.
	virtual entt::entity CreateScaffold(glm::vec3 place, entt::entity town, entt::entity workshop, float yAngle,
	                                    float scale) = 0;

	/// Whether the hand can take a scaffold. The hand can take a scaffold on a building site only before the building
	/// starts, and only for a time after the hand put the scaffold down.
	[[nodiscard]] virtual bool CanTake(entt::entity scaffold) const = 0;
	/// A hand took a scaffold: its workshop keeps its place free for it, and the building that the scaffold started goes
	virtual void TakenByHand(entt::entity scaffold, PlayerNames player) = 0;
	/// A hand lets go of a scaffold; `forced` when the hand drops it by force. A scaffold that the hand puts down on the
	/// land, not by force, comes down gently.
	virtual void LetGoByHand(entt::entity scaffold, bool forced) = 0;
	/// The hand did not put a scaffold down on the land: it flies, thus it does not come down gently
	virtual void NotPutDownByHand(entt::entity scaffold) = 0;
	/// A scaffold comes to rest. `fromHand` when a hand let it go last; `inMap` when it goes back into the map. A scaffold
	/// that comes to rest near a place of a workshop of its town goes into that place.
	virtual void CameToRest(entt::entity scaffold, bool fromHand, bool inMap) = 0;
	/// Whether the physics can move a scaffold: not one that stands in a place of its workshop's yard, and not one on a
	/// building site
	[[nodiscard]] virtual bool CanMove(entt::entity scaffold) const = 0;
	/// The game turn of a scaffold in a hand: its phantom building turns and changes size. While the hand stays still,
	/// the scaffold looks for the building that the town near it wants (see scaffold_offer::HeldTurn).
	virtual void ProcessHeld(entt::entity scaffold) = 0;
	/// A building site closes: the scaffolds on it go
	virtual void SiteClosed(entt::entity building) = 0;
	/// The frame shows the phantom buildings of the scaffolds. The phantom of a scaffold in the hand of the local player
	/// shows at `drawnHand`, where the frame draws that hand. The phantom on a building site fades as the building
	/// grows, and the scaffold on the site becomes small.
	virtual void PlaceGhostsForFrame(std::optional<glm::vec3> drawnHand) = 0;

	/// The town that a scaffold at a place builds for: the town of the nearest building, else the nearest town or
	/// planned building of a town nearer than 50 metres; none when there is none
	[[nodiscard]] virtual entt::entity TownForBuilding(glm::vec3 place) const = 0;
};

} // namespace openblack::ecs::systems
