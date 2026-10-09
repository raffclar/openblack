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
#include <span>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/InfluenceCircle.h"
#include "3D/MapCoords.h"
#include "Enums.h"

namespace openblack::ecs::systems
{

/// The players' influence: how far their citadels and towns reach, and the border drawn round it
class InfluenceSystemInterface
{
public:
	virtual ~InfluenceSystemInterface() = default;

	/// As a new land opens: no border yet
	virtual void Reset() = 0;
	/// Once a game turn: the towns' and citadels' reach, and the border drawn again every ten turns once one has moved
	virtual void ProcessTurn(uint32_t turn) = 0;
	/// Once a frame: the border's texture scrolls with the game time, and while the game runs the hand crossing a border
	/// sends out a ripple and a sound, and the ripples grow and fade
	virtual void Update(std::chrono::duration<float, std::milli> gameTime) = 0;
	/// Each frame this computer's hand is drawn: past the border, while it keeps some of its player's influence, it hums
	/// and lets out a mana path back towards the border; otherwise the hum is stopped. Neither changes while the hand is
	/// not drawn.
	virtual void ShowHandInfluence(std::chrono::duration<float, std::milli> gameTime) = 0;

	/// How much a place is in a player's influence, from -1 to 1. Near their hand it is what the hand keeps of their
	/// influence past the border, while it keeps any (see virtual_influence::Grant): the questions of a game turn measure
	/// from where the hand was at the turn, those asked between turns from where this computer's hand is now. Elsewhere
	/// it is their own influence (see PlayerRawInfluence).
	[[nodiscard]] virtual float PlayerInfluence(PlayerNames player, const map_coords::MapCoords& position) const = 0;
	/// The same at a point, made a map position first
	[[nodiscard]] float PlayerInfluence(PlayerNames player, const glm::vec3& position) const
	{
		return PlayerInfluence(player, map_coords::FromMetres({position.x, position.z}));
	}
	/// How much the hand's own place is in its player's influence: while the hand keeps any of their influence past the
	/// border it counts as inside however far out it is, else it is their own influence there. What the hand may do
	/// asks this.
	[[nodiscard]] virtual float HandPointInfluence(PlayerNames player, const map_coords::MapCoords& hand) const = 0;
	[[nodiscard]] bool IsHandInInfluence(PlayerNames player, const glm::vec3& hand) const
	{
		return HandPointInfluence(player, map_coords::FromMetres({hand.x, hand.z})) > 0.0f;
	}
	/// A player's own influence at a place, without what their hand keeps past the border: the reach of their citadel
	/// and of each of their towns and other sources whose reach it is within, measured as the game measures between map
	/// positions; none under another player's shield
	[[nodiscard]] virtual float PlayerRawInfluence(PlayerNames player, const map_coords::MapCoords& position) const = 0;
	[[nodiscard]] float PlayerRawInfluence(PlayerNames player, const glm::vec3& position) const
	{
		return PlayerRawInfluence(player, map_coords::FromMetres({position.x, position.z}));
	}
	/// The hand used what it holds on the land: what it keeps past the border wanes, or comes back, as for one more turn
	virtual void HeldThingUsedOnLand(PlayerNames player) = 0;
	/// Whether the game turn is being played: questions asked then measure from where the hands were at the turn
	virtual void SetInGameTurn(bool inGameTurn) = 0;

	[[nodiscard]] virtual std::span<const influence::Circle> GetCircles() const = 0;
	/// Whether a player's border shows yet: once their citadel stands
	[[nodiscard]] virtual bool IsBorderShown(PlayerNames player) const = 0;
	[[nodiscard]] virtual glm::vec2 GetScrollOffset() const = 0;
	[[nodiscard]] virtual std::span<const influence::Ripple> GetRipples() const = 0;
};

} // namespace openblack::ecs::systems
