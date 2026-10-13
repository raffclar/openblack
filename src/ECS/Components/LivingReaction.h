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

#include "Enums.h"
#include "Magic/VillagerReactionRules.h"

namespace openblack::ecs::components
{

/// What a villager or creature is reacting to
struct LivingReaction
{
	/// The reaction, 0 for none
	uint32_t reaction {0};
	Reaction type {Reaction::None};
	/// The game turn it began
	uint32_t startTurn {0};
	/// A creature's memory of the kinds it reacted to lately; a villager's is its VillagerReactionMemory, which the
	/// shields' reactions share
	magic::villager_reaction::Memory creatureMemory;
	/// The turn a creature last took up a flying thing's and a crushed thing's reaction, which it waits a while after
	std::optional<uint32_t> sawFlying;
	std::optional<uint32_t> sawCrushed;
};

/// A creature a script has told to take no notice of reactions, until a script tells it to again
struct CreatureReactionsOff
{
};

} // namespace openblack::ecs::components
