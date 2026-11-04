/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "ECS/Systems/ReactionSystemInterface.h"
#include "Enums.h"
#include "Magic/VillagerReactionRules.h"

namespace openblack::gamesave
{
class VanillaWorld;
struct VanillaRecord;
} // namespace openblack::gamesave

// The reactions of a world file of the game and what each living thing remembers of the reactions that it took up. The
// game deletes the reactions of the land before it reads the file, so after the load only the reactions of the file go
// on.
namespace openblack::ecs::vanilla_reaction_import
{

/// The place of the game's list of all the reactions among the lists of the game object
inline constexpr size_t k_GameReactionList = 9;
/// The number of kinds of reaction in the reaction table
inline constexpr uint32_t k_ReactionKinds = 41;
/// A living thing remembers this many kinds of reaction. When the game reads one more, the oldest goes.
inline constexpr size_t k_RememberedKinds = 3;

/// A reaction of the file: a thing that the living around it react to
struct ImportedReaction
{
	uint32_t number {0};
	/// The numbers of the objects of the thing that the living react to and of the dance of the reaction, 0 for none
	uint32_t target {0};
	uint32_t dance {0};
	/// The living things that react to it, the first to join first
	std::vector<uint32_t> takers;
	/// The kind of the reaction, by its row in the reaction table
	uint32_t type {0};
	/// The turn when the first living thing took it up. None when no living thing took it up.
	std::optional<uint32_t> firstTaken;
	/// Whether it spreads to more living things
	bool spreads {true};
	/// The player that made it. None when no player made it.
	std::optional<PlayerNames> player;
	/// How far it reaches now
	float reach {0.0f};
};

/// The reactions of the file, the oldest first. This is the reverse of the order of the game's list of reactions, which
/// the game's load keeps. A reaction that the list does not have comes after them, in the order of the file.
[[nodiscard]] std::vector<ImportedReaction> ReadReactions(const gamesave::VanillaWorld& world);

/// What a living thing of the file remembers of the kinds of reaction that it took up, the oldest first. The game keeps
/// only the newest three when it reads them.
[[nodiscard]] std::vector<magic::villager_reaction::Memory::Entry> ReadMemory(const gamesave::VanillaWorld& world,
                                                                              const gamesave::VanillaRecord& record);

/// The thing that the living react to, as the import made it
struct Initiator
{
	entt::entity entity {entt::null};
	glm::vec3 position {0.0f};
	/// How impressive the thing is, from the table row of its kind
	float impressiveValue {0.0f};
};

/// A reaction of the file as the reactions system keeps it at the turn of the file:
/// - It comes from the thing that the import made, with the kind and the reach of the file.
/// - A reaction of no player impresses no one.
/// - The time of a reaction that a living thing took up started at the turn of the file. It continues for the turns
///   since then.
/// - A thing in the world is as strong as 1. It has no magic type and no creature cast it.
[[nodiscard]] systems::ReactionSystemInterface::Active ActiveOf(const ImportedReaction& file, const Initiator& initiator,
                                                                uint32_t turn);

} // namespace openblack::ecs::vanilla_reaction_import
