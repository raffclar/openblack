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
#include <vector>

#include <entt/entity/entity.hpp>

#include "Creature/CreatureHelp.h"

namespace openblack::ecs::systems
{

/// The good advisor's messages about the player's creature (see creature_help): what the rest of the game raises, the
/// waiting messages told in their turn, and what the help watches of the creature each turn by itself (its body's
/// problems, what it wants while the hand stays over it, and the prompts on how to look after it)
class CreatureHelpSystemInterface
{
public:
	/// A message told, for the inspector
	struct Told
	{
		entt::entity creature;
		creature_help::Kind kind;
		uint32_t parameter;
		std::optional<uint32_t> text;
		uint32_t turn;
	};

	virtual ~CreatureHelpSystemInterface() = default;

	/// Something happened that the help may tell of: it is told now, waits its turn, or is let go
	virtual void Raise(entt::entity creature, const creature_help::Message& message) = 0;
	/// Once a game turn, after the creatures have thought: the waiting messages and the help's own watching
	virtual void ProcessTurn() = 0;
	/// The player has shown they know one of the creature's prompts, which isn't given again
	virtual void PromptKnown(entt::entity creature, creature_help::Prompt prompt) = 0;
	/// The creature faints from what its body lacks: the help tells why first
	virtual void Fainting(entt::entity creature) = 0;

	/// The creature help option, which the scripts can also set
	virtual void SetHelpOn(bool on) = 0;
	[[nodiscard]] virtual bool IsHelpOn() const = 0;

	/// The messages told lately, the newest last
	[[nodiscard]] virtual const std::vector<Told>& GetTold() const = 0;
};

} // namespace openblack::ecs::systems
