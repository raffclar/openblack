/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <filesystem>
#include <memory>
#include <optional>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

namespace openblack::creaturemind
{
struct MindFileData;
}

namespace openblack::ecs::systems
{

/// The player's creature outlives the land it is on. As a land is cleared, the player's own creature is kept with its
/// mind and body, as the game keeps it in the player's creature file; a later land's script then loads the creature from
/// what was kept, at the place it gives. Whatever is not in the file starts again: its life, its spells, the leash, what
/// it holds and what it was doing.
class CreatureCarryOverSystemInterface
{
public:
	virtual ~CreatureCarryOverSystemInterface() = default;

	/// As a land is cleared: the player's creature, if they have one, is kept as it is now, in place of any kept before.
	/// With no creature, what was kept before stays.
	virtual void KeepPlayersCreature() = 0;
	/// A creature of the player's own is kept as it is now, in place of any kept before, as the game keeps it whenever
	/// it is deleted: as it sparkles away through a vortex as much as with the land. Another player's creature isn't
	virtual void KeepCreature(entt::entity creature) = 0;
	/// A creature file kept in place of any before
	virtual void Keep(std::shared_ptr<const creaturemind::MindFileData> file) = 0;
	/// The creature kept, if any
	[[nodiscard]] virtual std::shared_ptr<const creaturemind::MindFileData> Kept() const = 0;
	/// The creature kept. With none kept, the creature that the profile's file keeps, which is then the creature kept.
	/// None when the file is not there or cannot be read.
	[[nodiscard]] virtual std::shared_ptr<const creaturemind::MindFileData> KeptOrRead() { return Kept(); }
	/// The physique file of the profile picked now, from which the main menu's tattoo editor shows the body of the
	/// profile's creature. None without a profile or a folder for the files. The files that the system wrote before are
	/// on the disk when it returns.
	[[nodiscard]] virtual std::optional<std::filesystem::path> ProfilePhysique() const = 0;
	/// The file that keeps the mind of the current profile's creature. A load of a saved game gives the local player's
	/// creature the mind of this file. The files that the system wrote before are on the disk when it returns.
	[[nodiscard]] virtual std::optional<std::filesystem::path> ProfileMind() const = 0;

	/// A script loads the player's creature at a place on the land. Nothing happens when the player has a creature
	/// already or none was kept. It is made standing on the ground in the middle of the place's cell, starts out of sight
	/// with the sound of sparkling and sparkles into sight over three seconds.
	virtual std::optional<entt::entity> LoadPlayersCreature(glm::vec2 place) = 0;

	/// Once a game turn: the creatures loaded sparkle into sight. A creature of the player's own is written to the
	/// player's creature files each time it has lived 600 more turns, when the land keeps creatures
	virtual void ProcessTurn() = 0;
	/// In the creature's own turn, after its body lived the turn: the same for this creature
	virtual void CreatureTurn(entt::entity /*creature*/) {}
	/// Whether the land played now keeps the player's creature in the player's files as it lives and when the game ends.
	/// The testbed doesn't, so that its creatures never replace the player's own
	virtual void SetLandKeepsCreatures(bool keeps) = 0;
	/// The game ends: the player's creature, if they have one, is written to their files as it is now, when the land
	/// keeps creatures
	virtual void EndGame() = 0;
	/// The land is cleared: the creatures still sparkling into sight go with it; what was kept stays
	virtual void Reset() = 0;
	/// A new game starts, after the last game's creature was kept: from now until the next new game the creature is
	/// kept in the file of the profile picked now. Picking another profile during a game changes nothing until then, so
	/// the last game's creature is always kept for the profile it was played as. A creature kept for another profile is
	/// forgotten, for the new profile's own file to be read when a script loads its creature.
	virtual void StartGame() = 0;
	/// The creature's files go to the disk on a thread of their own, so that a write does not delay the frames of the
	/// game. This waits until every file that the system wrote before is on the disk. Call it before a read of the files.
	virtual void FinishWrites() const = 0;
	/// In a network game, the god of this computer brought this creature from the player's creature file. In a network
	/// game, this computer writes only this creature to the player's files. The land clear forgets it.
	virtual void SetBroughtCreature(entt::entity /*creature*/) {}
};

} // namespace openblack::ecs::systems
