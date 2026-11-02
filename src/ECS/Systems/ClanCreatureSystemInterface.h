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
#include <span>
#include <vector>

namespace openblack::ecs::systems
{

/// The start creature of a god that plays for a clan: the data that goes to the other players in place of the
/// creature file of the player's profile, and the alignment that goes with it
struct ClanStartCreature
{
	/// The creature file of the stored clan creature, or the order to make a new clan creature
	std::vector<uint8_t> file;
	/// The alignment that the lobby kept with the stored clan creature. A new clan creature starts at 0.
	float alignment {0.0f};
};

/// What the clan upload did at the end of the game
enum class ClanUpload : uint8_t
{
	/// The god of this computer plays for no clan
	NoClan,
	/// The game has no client of the lobby
	NoLobby,
	/// The game kept no creature file in this run
	NoCreatureFile,
	/// The kept creature file is not a creature file of the game
	BadCreatureFile,
	/// The lobby gave no second buffer, and the game does not make the user data of the player yet
	NoUserData,
	Sent,
};

/// The creature of a clan in a network game on the internet. When the player uses a clan in the game room, the game
/// asks the lobby for the clan creature. At the start, the god of this computer sends that creature in place of the
/// creature of the player's profile. At the end of the game, the game of the god sends the creature back to the lobby,
/// which keeps it for the next game of the clan.
class ClanCreatureSystemInterface
{
public:
	virtual ~ClanCreatureSystemInterface() = default;

	/// The player uses a clan in the game room, or no clan. With a clan, the game asks the lobby for the clan creature
	/// and keeps the answer. With no clan, the game forgets the answer.
	virtual void UseClan(std::optional<int64_t> clan) = 0;
	/// The clan that the player uses, if any
	[[nodiscard]] virtual std::optional<int64_t> UsedClan() const = 0;
	/// The start creature of the god of this computer. None when the player uses no clan, when the lobby did not answer
	/// yet, or when the clan has no creature: the god then brings the creature of the player's profile.
	[[nodiscard]] virtual std::optional<ClanStartCreature> StartCreature() const = 0;
	/// In a game turn: the creature file of the creature of the god of this computer, as it is now. The game keeps it in
	/// place of the file that it kept before, until the clan upload sends it.
	virtual void KeepCreatureFile(std::vector<uint8_t> file) = 0;
	/// The creature file that the game kept, empty when it kept none
	[[nodiscard]] virtual std::span<const uint8_t> KeptCreatureFile() const = 0;
	/// An end box of a decided game, or of a god that is out, opened in a game on the internet: the game sends the clan
	/// creature to the lobby with the alignment of the god of this computer
	virtual ClanUpload SendAtGameEnd(float godAlignment) = 0;
};

} // namespace openblack::ecs::systems
