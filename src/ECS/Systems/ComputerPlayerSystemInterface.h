/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <span>
#include <string_view>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::systems
{

/// The computer minds of the players. A land script switches a mind on. At its first turn after that, the mind starts on
/// the land: it gives the player a creature when the player has none, sizes the creature and teaches it its miracles.
class ComputerPlayerSystemInterface
{
public:
	virtual ~ComputerPlayerSystemInterface() = default;

	/// A land script switches the mind of the player on or off. The game makes the player when the land has none yet.
	virtual void SwitchOn(PlayerNames player, bool on) = 0;
	/// A land script names the player whose creature gives the size of this player's creature
	virtual void SetCreatureLike(PlayerNames player, PlayerNames like) = 0;
	/// A land script switches the mind of the player on or off, and leaves the mark of a computer player as it is
	virtual void SetActive(PlayerNames player, bool on) = 0;
	/// A land script pauses the mind of the player, or lets it go on. A paused mind also holds its hand.
	virtual void SetPaused(PlayerNames player, bool paused) = 0;
	/// A land script sets the weight of each need of the mind that has the name
	virtual void SetPersonality(PlayerNames player, std::string_view name, float weight) = 0;
	/// A land script sets the suppression of each need of the mind that has the name
	virtual void SetSuppression(PlayerNames player, std::string_view name, float suppression) = 0;
	/// The hand of the god has no action to do
	[[nodiscard]] virtual bool Ready(PlayerNames player) = 0;
	/// A land script makes the god do the action of a node of its mind with two things. False when it cannot start.
	virtual bool ForceAction(PlayerNames player, std::string_view name, entt::entity first, entt::entity second) = 0;
	/// A land script puts an action at the end of the queue of the god
	virtual void QueueAction(PlayerNames player, std::string_view name, entt::entity first, entt::entity second) = 0;
	/// A land script stops the action of the hand and empties the queue of the god
	virtual void ClearActions(PlayerNames player) = 0;
	/// A land script sets the speed of the hand of the god, in metres a second
	virtual void SetSpeed(PlayerNames player, float speed) = 0;
	/// A land script moves the hand of the god to a place at a speed, and holds the hand there. The script switches the
	/// mind on and starts a paused mind again. With a fixed height the hand floats over the land, else it goes straight.
	virtual void MoveHand(PlayerNames player, glm::vec3 place, float speed, bool fixedHeight) = 0;
	/// A land script puts the hand of the god at a place at once, and holds the hand there. With a fixed height the
	/// hand floats over the land.
	virtual void SetHand(PlayerNames player, glm::vec3 place, bool fixedHeight) = 0;
	/// Where the hand of the god is, at the height of the land under it. None for a player with no computer mind.
	[[nodiscard]] virtual std::optional<glm::vec3> HandPosition(PlayerNames player) = 0;
	/// A land script sets what the mind of the player thinks of another player. The mind keeps two times the value. The
	/// game makes the player when the land has none yet.
	virtual void SetAttitude(PlayerNames /*player*/, PlayerNames /*other*/, float /*attitude*/) {}
	/// What the mind of the player thinks of another player, as a land script reads it: half of the kept value
	[[nodiscard]] virtual float Attitude(PlayerNames /*player*/, PlayerNames /*other*/) const { return 0.0f; }
	/// A land script saves the weights of the mind of the player in a personality file. A relative name is in the
	/// profile folder. The save makes the file or writes over it. Returns true when it wrote the whole file.
	virtual bool SavePersonality(PlayerNames /*player*/, std::string_view /*file*/) { return false; }
	/// A land script loads the weights of a personality file into the mind of the player. The load looks for a relative
	/// name in the profile folder first, then in the game folder. Returns true when it found the file.
	virtual bool LoadPersonality(PlayerNames /*player*/, std::string_view /*file*/) { return false; }
	/// In the turn of the players: each mind that is switched on and did not start on the land starts
	virtual void ProcessTurn() = 0;
	/// At the start of a network game, in the order of the gods of the slots: each god with a citadel that no human and
	/// no computer mind plays gets a computer god. Its mind switches on, and it gets a fully grown creature of a random
	/// species at the creature home of its citadel. The game makes the player when the land has none yet.
	virtual void FillEmptySlots(std::span<const PlayerNames> gods, std::span<const PlayerNames> humans) = 0;
	/// The land of a network game waits for the humans of the network session before it fills its slots with no player
	virtual void WaitForHumans() = 0;
	/// In the turn of the start of a network game: the humans of the network session join. The game makes the player of
	/// each human when the land has none. Then, when the land waits for its humans, the slots with no player fill.
	virtual void HumansJoined(std::span<const PlayerNames> humans) = 0;
};

} // namespace openblack::ecs::systems
