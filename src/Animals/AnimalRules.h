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
#include <optional>
#include <span>

#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/AllMeshes.h"
#include "Enums.h"

// How the animals are turned to face their way, and the places of a flock's birds about their leader. Pure functions,
// tested on their own.

namespace openblack
{
struct GAnimalStateTableInfo;
} // namespace openblack

namespace openblack::animals
{

/// The angle a kind's turn angle allows in a turn, in radians: the tables count a full turn as 0x800
[[nodiscard]] float TurnAngleRadians(int turnAngle);

/// An animal's size as it is born: a young one (younger than its kind's grown up age) its kind's table's size for the
/// year before its age, grown a random way up to three quarters of the way to the size for the year after; a grown one
/// nine tenths, then a size between 0.95 and 1.05, drawn twice. `random(max)` is the game's random number up to max.
[[nodiscard]] float BirthScale(uint32_t age, uint32_t grownUpAge, std::span<const float> ageToScale,
                               const std::function<float(float)>& random);

/// How an animal's model is turned to face a heading across the land (radians from +x towards +z), tilted by a bank
/// about the way it faces. The models face -z.
[[nodiscard]] glm::mat3 Orientation(float heading, float bank);

/// A follower's place in its flock's formation, by its place in the flock counted from the leader (1 for the leader,
/// up to the number of birds for the newest): its row and column of the formation
struct FormationSlot
{
	int row;
	int column;
};
[[nodiscard]] FormationSlot FormationSlotOf(int place);
/// A formation's rows and columns are this far apart
inline constexpr float k_FormationSpacing = 10.0f;
/// Where a follower goes for its slot, in metres: the game angle of its slot's (row, column), plus the way from the
/// leader to the follower, then the row's spacing along that angle's x and the column's along its z, from the leader
[[nodiscard]] glm::vec2 FormationGoal(glm::vec2 leader, glm::vec2 follower, FormationSlot slot);

/// What lets an animal take up a reaction to what it sees, or not
struct ReactionFacts
{
	/// A script controls it: one the script made or took hold of
	bool scriptControlled {false};
	/// It is in the hand, in the physics or carried by a storm
	bool outOfTheWorld {false};
	/// It is dancing, dying, dead, drowning, downed, being eaten or waiting for a clip, or on its way into one of them
	bool busyState {false};
};
/// Whether a state, by its number in the game's animal state table, keeps an animal from taking up a reaction: dancing
/// (5), dying (14), dead (15), drowning (16), downed (17), being eaten (18) and waiting for a clip (23). The state to
/// judge is the one the animal is to end up in, as the table says, not one it only passes through.
[[nodiscard]] constexpr bool IsBusyForReaction(uint8_t gameState)
{
	constexpr uint8_t k_Dancing = 5;
	constexpr uint8_t k_DyingFirst = 14;
	constexpr uint8_t k_BeingEaten = 18;
	constexpr uint8_t k_WaitingForClip = 23;
	return gameState == k_Dancing || (gameState >= k_DyingFirst && gameState <= k_BeingEaten) || gameState == k_WaitingForClip;
}
/// An animal takes up no reaction while a script controls it: a script's animal stays where the script put it, even
/// with a creature at its side
[[nodiscard]] constexpr bool AvailableForReaction(const ReactionFacts& facts)
{
	return !facts.scriptControlled && !facts.outOfTheWorld && !facts.busyState;
}

/// The state that an animal goes into when its reaction ends, by its number in the game's animal state table. This is
/// the resume state of the table row of the state that the animal was in when it took up the reaction. A number that
/// is not in the table gives the empty state (0).
[[nodiscard]] uint8_t StateAfterReacting(uint8_t stateBefore, std::span<const GAnimalStateTableInfo> table);

/// Whether a kind of animal frightens a creature. Bats, the bats of the evil flock and vultures frighten a creature.
/// Lions, tigers, leopards, wolves and the wolves that a spell makes also frighten a creature.
/// The lion and the cow of the puzzles do not frighten a creature.
[[nodiscard]] bool FrightensCreatures(AnimalInfo type);

/// A killed animal lies dead this many turns, and one more, before it goes
inline constexpr int16_t k_TurnsToDieOver = 600;
/// The clip a kind of animal falls dead with, and the one it then lies dead in, once killed; none where its kind keeps
/// the clip it had. A farm animal lies dead on its right side only when it last came down on its right side
[[nodiscard]] std::optional<AnimId> DyingClip(AnimalInfo type);
[[nodiscard]] std::optional<AnimId> DeadClip(AnimalInfo type, bool onRightSide);
/// A turn of a body lying dead: whether it goes now. Its turns left count down to none, then it goes; a body a script
/// controls never counts down and lies there until the script lets it go. The count is the animal's count of turns,
/// which goes from its lowest value to its highest value: a body with a count below none lies until the count comes
/// round to none.
[[nodiscard]] bool DeadBodyGoes(int16_t& turnsLeft, bool scriptControlled);

/// Who holds an animal or moves it this turn
struct AnimalHolders
{
	bool tornado {false};
	bool playersHand {false};
	bool creature {false};
	bool physics {false};
};
/// Whether an animal takes its own turn: not while a tornado, the player's hand or the physics moves it. In a creature's
/// hand it takes its turn only while it dies or lies dead, so that it falls and its body goes there.
[[nodiscard]] bool TakesOwnTurn(const AnimalHolders& holders, bool dyingOrDead);
/// A body going leaves a grey puff of smoke this big, half its height out along the way it faces from where it lies
inline constexpr float k_DeadBodySmokeSize = 1.0f;
[[nodiscard]] glm::vec3 DeadBodySmokePlace(const glm::vec3& position, float heading, float height);

} // namespace openblack::animals
