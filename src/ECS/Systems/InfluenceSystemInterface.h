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

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/InfluenceCircle.h"
#include "3D/MapCoords.h"
#include "ECS/Systems/SavedStateInterface.h"
#include "Enums.h"

namespace openblack::ecs::systems
{

namespace influence_rules
{
/// A player's influence at a place
struct PlayerInfluenceAt
{
	PlayerNames player {PlayerNames::NEUTRAL};
	float influence {0.0f};
};
/// The player of most influence at a place, the players in their order: the neutral player until a player has more
/// than no influence there, and a later player must have more than an earlier one to take the place
[[nodiscard]] inline PlayerNames MostInfluentialPlayer(std::span<const PlayerInfluenceAt> players)
{
	float best = 0.0f;
	PlayerNames most = PlayerNames::NEUTRAL;
	for (const auto& player : players)
	{
		if (player.influence > best)
		{
			best = player.influence;
			most = player.player;
		}
	}
	return most;
}
} // namespace influence_rules

/// The players' influence: how far their citadels and towns reach, and the border drawn round it
class InfluenceSystemInterface: public SavedStateInterface
{
public:
	virtual ~InfluenceSystemInterface() = default;

	/// As a new land opens: no border yet
	virtual void Reset() = 0;
	/// Once a game turn: the towns' and citadels' reach, and the border drawn again every ten turns once one has moved
	virtual void ProcessTurn(uint32_t turn) = 0;
	/// A town's turn starts: its influence is its base influence and its buildings', times the land's multiplier
	virtual void ProcessTownInfluence(entt::entity /*town*/) {}
	/// A town's turn ends: a change of the influence of a player's town marks the borders to be drawn again
	virtual void MarkTownInfluence(entt::entity /*town*/) {}
	/// A town has changed hands: the border is drawn again at its next turn for drawing
	virtual void BordersChanged() = 0;
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
	/// Fixes the reach of each citadel that no question fixed yet. The frames of the hand ask for the reaches in each frame,
	/// so a world with no frames of the hand, as the dedicated server, fixes them at the same time.
	virtual void FixCitadelReaches() = 0;

	[[nodiscard]] virtual std::span<const influence::Circle> GetCircles() const = 0;
	/// Whether a player's border shows yet: from the first time their temple stands wholly built on the land
	[[nodiscard]] virtual bool IsBorderShown(PlayerNames player) const = 0;
	/// A player's whole power of influence: their citadel's reach, while it stands, with the reach of each of their towns
	/// and of their other sources of influence
	[[nodiscard]] virtual float InfluencePower(PlayerNames /*player*/) const { return 0.0f; }
	/// How far a player's citadel reaches with the land's multiplier, 0 when the player has no citadel
	[[nodiscard]] virtual float CitadelReachOf(PlayerNames /*player*/) const { return 0.0f; }
	[[nodiscard]] virtual glm::vec2 GetScrollOffset() const = 0;
	[[nodiscard]] virtual std::span<const influence::Ripple> GetRipples() const = 0;
};

} // namespace openblack::ecs::systems
