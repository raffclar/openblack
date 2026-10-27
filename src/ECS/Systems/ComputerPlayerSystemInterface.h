/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <span>
#include <string_view>

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
	/// A land script sets what the mind of the player thinks of another player. The mind keeps two times the value. The
	/// game makes the player when the land has none yet.
	virtual void SetAttitude(PlayerNames /*player*/, PlayerNames /*other*/, float /*attitude*/) {}
	/// What the mind of the player thinks of another player, as a land script reads it: half of the kept value
	[[nodiscard]] virtual float Attitude(PlayerNames /*player*/, PlayerNames /*other*/) const { return 0.0f; }
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
