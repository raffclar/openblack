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

#include <array>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "Enums.h"

// How a villager takes up a reaction going on near it, by the reaction's table row. A villager sees a reaction within
// the row's distance by the row's priority, more urgently the nearer it is; it takes up a reaction of a kind again only
// once a cooldown since it last did has passed, and it keeps reacting for as long as the row says, longer for nearer
// things. It remembers the last three kinds it took up, each for 1800 turns. Pure rules, tested without the game.

namespace openblack::magic::villager_reaction
{

/// The turns that mean for ever
inline constexpr uint32_t k_Forever = 0x7FFFFFFF;

/// What a reaction's table row says about distance
struct Distance
{
	/// How far it reaches, and how much nearness counts, 0 to 1
	float maxDistance {0.0f};
	float importance {0.0f};
};

/// Whether a villager takes up a kind of reaction it heeds. Villagers do nothing about the hand picking something up,
/// the hand using a totem, the hand putting things in a storage pit or something crushed: they weigh such a reaction,
/// and remember having heeded its kind, but never hold it, so it neither moves nor impresses them (the creature
/// reacts to these).
[[nodiscard]] bool TakesUp(openblack::Reaction type);

/// Whether a town's building just finished makes its people react to it: a building of the town's own (a totem, storage
/// pit, crèche, workshop, wonder, graveyard, town centre or spell dispenser), not a home, nor a temple or a football
/// pitch
[[nodiscard]] bool ReactedToWhenBuilt(openblack::AbodeType type);

/// What a villager weighs when its town celebrates being won: whether it is a disciple, under a script, of a town at all,
/// and whether the celebration is made by its own town itself (a town with no town centre; one with a centre celebrates
/// from the centre)
struct CelebrationWatch
{
	bool disciple {false};
	bool scripted {false};
	bool hasTown {false};
	bool madeByOwnTown {false};
};

/// How urgently a villager heeds a town's celebration: by the celebration row's priority, only an ordinary villager of
/// the town and only when the town itself made it. So a town celebrating from its centre draws none of its people.
[[nodiscard]] uint8_t TownCelebrationPriority(const CelebrationWatch& watch, uint32_t tablePriority);

/// The distance a villager is from a reaction as it is spread: half the sum of how far apart they are along each axis
[[nodiscard]] float SpreadDistance(glm::vec2 villager, glm::vec2 reaction);

/// How urgent a reaction is to a villager at a distance, 0 to 255: its kind's priority to the villager, up to half as
/// much again by its nearness; none beyond its reach or when the villager's kind doesn't react to it
[[nodiscard]] uint8_t Priority(uint8_t kindPriority, bool kindReacts, const Distance& distance, float at);

/// What a villager weighs when the hand has picked up one of its people: whether the one held is a villager still in the
/// hand, whether the watcher is of an age to make love, of the other sex and of the same player, and whether a script
/// controls the watcher
struct VillagerInHandWatch
{
	bool heldIsVillager {false};
	bool heldInHand {false};
	bool watcherSexuallyActive {false};
	bool otherSex {false};
	bool samePlayer {false};
	bool watcherScripted {false};
};

/// How urgently a villager heeds one of its people held in the hand: only a villager of an age to make love, of the other
/// sex and of the same player, not under a script, heeds a villager held, and it does so by the priority of the
/// breeder's row of the reaction table (the game reads that row for this reaction), as a byte, whatever the distance.
/// None for anyone else.
[[nodiscard]] uint8_t VillagerInHandPriority(const VillagerInHandWatch& watch, uint32_t breederPriority);

/// Whether a villager waiting for the one held in the hand as a mate keeps waiting, by how far that one now is, in metres
/// across the land, against the reaction's reach
[[nodiscard]] bool KeepsWaitingForMate(float distance, float reach);

/// The kinds of reaction a villager has taken up lately and when
class Memory
{
public:
	/// Whether the villager may take up a reaction of a kind now, after a cooldown of turns since it last did: it
	/// remembers taking it up now when it may. Looking through what it remembers, oldest first, it forgets each other kind
	/// older than 1800 turns it passes before it comes to this kind; a kind new to it is remembered last, the oldest going
	/// once it remembers three.
	bool MayReactAgain(uint32_t type, uint32_t turn, uint32_t cooldown);
	/// The villager switched to a kind of reaction now
	void Record(uint32_t type, uint32_t turn);
	/// The villager let go of a kind of reaction now: when it remembers the kind, it remembers it as now; a kind it has
	/// forgotten stays forgotten
	void Refresh(uint32_t type, uint32_t turn);
	/// When the villager last took up a kind of reaction, 0 for not lately
	[[nodiscard]] uint32_t LastReacted(uint32_t type) const;

private:
	struct Entry
	{
		uint32_t type;
		uint32_t turn;
	};
	/// A new kind is added once this many are remembered, the oldest going first
	static constexpr size_t k_Records = 3;
	static constexpr uint32_t k_ForgetAfter = 1800;
	void Remember(uint32_t type, uint32_t turn);
	/// Oldest first
	std::vector<Entry> _records;
};

} // namespace openblack::magic::villager_reaction
