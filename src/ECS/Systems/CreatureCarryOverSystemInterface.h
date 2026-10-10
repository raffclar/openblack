/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

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
	/// A creature file kept in place of any before
	virtual void Keep(std::shared_ptr<const creaturemind::MindFileData> file) = 0;
	/// The creature kept, if any
	[[nodiscard]] virtual std::shared_ptr<const creaturemind::MindFileData> Kept() const = 0;

	/// A script loads the player's creature at a place on the land. Nothing happens when the player has a creature
	/// already or none was kept. It is made standing on the ground in the middle of the place's cell, starts out of sight
	/// with the sound of sparkling and sparkles into sight over three seconds.
	virtual std::optional<entt::entity> LoadPlayersCreature(glm::vec2 place) = 0;

	/// Once a game turn: the creatures loaded sparkle into sight
	virtual void ProcessTurn() = 0;
	/// The land is cleared: the creatures still sparkling into sight go with it; what was kept stays
	virtual void Reset() = 0;
};

} // namespace openblack::ecs::systems
