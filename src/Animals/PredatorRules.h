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

#include <functional>

#include "Enums.h"

// The hunters of the land (lions, tigers, wolves, leopards and the ground flock miracle's wolves) frighten the living
// about them. Each hunter carries a reaction of its own for as long as it lives, which the villagers and animals within
// its reach take up by the rules here: a villager runs from it, watches it or hides in a building of its town, a grazer
// runs from it, and a hunter runs from a stronger one. Pure functions, tested on their own.

namespace openblack::animals::predators
{

/// Whether an animal of a kind is a hunter that frightens the living about it
[[nodiscard]] bool IsPredator(AnimalInfo type);

/// A hunter's speed in metres a second from its speed state (map units a turn), as the game works it out
[[nodiscard]] float SpeedMetresPerSecond(uint16_t speedState);

/// Hunters are fast once they run faster than this, in metres a second
inline constexpr float k_FastMetresPerSecond = 5.0f;
/// Turns a villager must have been looking about outside its hiding place before a hunter frightens it again whatever the
/// hunter is doing
inline constexpr uint16_t k_LookingAboutTurnsBeforeFrightened = 20;
/// A villager or animal runs from a hunter this far at a time, in metres
inline constexpr float k_FleeStep = 10.0f;
/// One in so many villagers frightened by a slow hunter, or one not coming at it, hides
inline constexpr uint32_t k_HideChance = 6;

/// How a villager sees a hunter whose reaction reaches it
struct VillagerSees
{
	/// The hunter is an animal (only an animal frightens a villager)
	bool animal {false};
	/// From the villager to the hunter, in metres across the land
	float distance {0.0f};
	/// The reaction's least distance to run from it
	float leastRunDistance {0.0f};
	/// The villager is inside its home
	bool atHome {false};
	/// The villager is hiding in a building now
	bool hiding {false};
	/// The villager is on its way to hide in a building
	bool goingToHide {false};
	/// The villager is looking about outside the building it hid in, and for how many turns
	bool lookingAbout {false};
	uint16_t turnsLookingAbout {0};
	/// The hunter is asleep or in its lair
	bool hunterResting {false};
	/// The villager has taken up a hunter's reaction before
	bool frightenedBefore {false};
	/// The hunter is heading at the villager
	bool comingAt {false};
	/// The hunter's speed in metres a second, and its kind's speed state 3 (raw, as the game divides by it)
	float speed {0.0f};
	uint32_t kindSpeed3 {1};
	/// The table's priority of the reaction
	uint32_t tablePriority {0};
};
/// How urgent a hunter is to a villager, 0 for not at all. The game divides the hunter's speed in metres a second by a
/// raw speed state of its kind, so the urgency is half the table's, or a hair more, whatever the hunter does.
[[nodiscard]] uint8_t VillagerPriority(const VillagerSees& sees);

/// How urgent a hunter is to another hunter: only one stronger than it frightens it, and anything not living does
[[nodiscard]] uint8_t HunterPriority(bool thingLiving, float thingStrength, float ownStrength, uint32_t tablePriority);

/// How urgent a hunter is to any other living thing (a grazer, a creature): only one moving faster than its kind's
/// speed state 2 (both raw speed states), and anything not an animal
[[nodiscard]] uint8_t OtherPriority(bool thingAnimal, uint16_t thingSpeed, uint32_t thingKindSpeed2, uint32_t tablePriority);

/// What a villager frightened by a hunter does this turn
enum class VillagerFlees : uint8_t
{
	/// The hunter is gone, or far beyond the reaction's furthest run: it stops being frightened
	Stop,
	/// The hunter is something else than an animal: it does nothing
	Nothing,
	/// It tries to hide in a building of its town, and failing that runs or watches
	TryToHide,
	/// It runs or watches
	RunOrWatch,
};
struct VillagerFleeTurn
{
	bool hunterThere {false};
	bool animal {false};
	float distance {0.0f};
	float furthestRunDistance {0.0f};
	bool fast {false};
	bool comingAt {false};
};
/// The random number (below the hide chance) is drawn only when the hunter isn't both fast and coming at the villager
[[nodiscard]] VillagerFlees VillagerFleeStep(const VillagerFleeTurn& turn, const std::function<uint32_t(uint32_t)>& random);

/// Whether a frightened villager or animal turns to watch a hunter rather than run: far enough from it and the hunter
/// isn't coming at it
[[nodiscard]] bool WatchesRatherThanRuns(float distance, float leastRunDistance, bool comingAt);

/// Whether a villager looking about outside its hiding place is still frightened by the hunter of its reaction: within
/// the reaction's reach, an animal, and awake and out of its lair
[[nodiscard]] bool StillFrightensLookout(bool hunterThere, float distance, float reach, bool animal, bool hunterResting);

/// What an animal frightened by a hunter does this turn
enum class AnimalFlees : uint8_t
{
	/// It has nothing to run from: it does nothing
	Nothing,
	/// The hunter is gone or beyond the reaction's furthest run: it stops being frightened
	Stop,
	/// It runs a step away from the hunter at its kind's fleeing speed
	Run,
};
[[nodiscard]] AnimalFlees AnimalFleeStep(bool hasHunter, bool hunterThere, float distance, float furthestRunDistance);

} // namespace openblack::animals::predators
