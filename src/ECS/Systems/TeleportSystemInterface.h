/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <chrono>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "ECS/Systems/SavedStateInterface.h"
#include "Enums.h"

namespace openblack::ecs::systems
{
class ObjectDrawListSystemInterface;

/// The teleport miracle's stones. Each cast leaves an invisible stone with a swirling pool on the land; a player's stones
/// form a network. Villagers and creatures walking past one of them turn aside into it when jumping to another of the
/// player's stones saves them enough of their walk, and come out of the stone that leaves them closest to where they
/// were going. A stone goes with its miracle.
class TeleportSystemInterface: public SavedStateInterface
{
public:
	virtual ~TeleportSystemInterface() = default;

	/// A new stone of a miracle at a point on the land, at the head of its player's stones; none if one may not go there
	virtual entt::entity CreateStone(glm::vec3 point, PlayerNames player, entt::entity spell) = 0;
	/// The stone goes, with its pool, its sound and its reaction
	virtual void RemoveStone(entt::entity stone) = 0;
	/// A miracle closes down or goes: its stones go at that time, in the miracle's turn
	virtual void RemoveStonesOf(entt::entity spell) = 0;
	/// Whether a stone may be put at a point: nothing standing fixed on the land within its reach
	[[nodiscard]] virtual bool CanPlaceStone(glm::vec3 point) const = 0;
	/// Whether a stone stands where a new building would go: nothing may be built on one
	[[nodiscard]] virtual bool BlocksNewBuilding(glm::vec3 point) const = 0;
	/// A player's stones, the newest first
	[[nodiscard]] virtual std::vector<entt::entity> GetStones(PlayerNames player) const = 0;
	/// The stone of a miracle, if it has one
	[[nodiscard]] virtual std::optional<entt::entity> StoneOf(entt::entity spell) const = 0;

	/// Whether a villager or creature walking past a stone would save enough of its walk by jumping from it to another of
	/// the stone's player's stones: what makes it take up the stone's reaction at all
	[[nodiscard]] virtual bool ShouldReact(entt::entity stone, entt::entity living) const = 0;
	/// A villager or creature turns aside into a stone: it is listed with where it was going, and heads for it
	virtual void SetupReact(entt::entity stone, entt::entity living) = 0;
	/// The jump from a stone to the player's stone that leaves the traveller closest to where it was going; forced, it
	/// takes any other stone. Whether it jumped.
	virtual bool DoTeleport(entt::entity stone, entt::entity living, bool forced) = 0;
	/// A creature that jumped starts its fade out: the stone carries it from the next turn. Whether the stone carries it.
	virtual bool StartCarry(entt::entity creature) = 0;
	/// One turn of a creature that the stone carries: it fades out, comes out of the far stone, then fades back in.
	/// Whether it is back in sight, or the stone does not carry it.
	virtual bool CarryTurn(entt::entity creature) = 0;
	/// The stone no longer controls the creature: the creature is not a traveller of a stone, and does not react to it
	virtual void StopControl(entt::entity creature) = 0;
	/// A villager dropped by a player's hand onto a stone of theirs jumps at once, if there is another stone: it decides
	/// what to do as it lands and jumps to the stone nearest where that takes it. Whether it jumped.
	virtual bool DropOnStone(entt::entity villager, entt::entity stone, PlayerNames dropper) = 0;
	/// The stone the hand touches along a ray, within its touch radius
	[[nodiscard]] virtual std::optional<entt::entity> StoneAlong(glm::vec3 origin, glm::vec3 direction) const = 0;
	/// The stone a worshipper of a player heads for to reach a far worship site, if going through the stones is shorter
	/// than the limit
	[[nodiscard]] virtual std::optional<entt::entity> RouteStoneFor(PlayerNames player, glm::vec3 worshipper, glm::vec3 site,
	                                                                float maxDistance) const = 0;
	/// A villager of a player setting off to worship at a site further than villagers walk to worship turns aside into
	/// the stone nearest it, when going through the stones is shorter than that distance; once through it takes up its
	/// way to the site again. Whether it did.
	virtual bool RouteWorshipper(entt::entity villager, glm::vec3 site) = 0;

	/// In the turn of each player, after its towns: its stones drop the travellers that gave up, the newest stone first
	virtual void PlayerTurn(PlayerNames player) = 0;
	/// Once a game turn, after the turns of the players: the stones of miracles that are not there go. A creature is
	/// dropped when its plan has no step of the teleport left.
	virtual void ProcessTurn() = 0;
	/// Once a frame, after the frame makes the land's draw list: the pools of the stones that the frame draws show and
	/// move on, and the other pools do not show. They step no more often than 60 times a second, by the game time since
	/// the last step. A stone in the list stays drawn when the camera stops, because its draw does not test the screen.
	virtual void UpdateFrame(ObjectDrawListSystemInterface& /*drawList*/, std::chrono::milliseconds /*frameRealTime*/,
	                         std::chrono::milliseconds /*frameGameTime*/, bool /*landDrawn*/)
	{
	}
	/// A new land: no stones
	virtual void Reset() = 0;
};

} // namespace openblack::ecs::systems
