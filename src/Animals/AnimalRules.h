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

/// Where the frame draws an animal and the heading it draws it with, at a part of the turn (clamped to 0 to 1) between
/// the places and the headings of its last two turns. The heading turns the short way round.
struct DrawnBetweenTurns
{
	glm::vec3 position;
	float heading;
};
[[nodiscard]] DrawnBetweenTurns DrawnAt(const glm::vec3& previousPosition, const glm::vec3& position, float previousHeading,
                                        float heading, float turnFraction);

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
/// Where a follower goes for its slot: the game angle of its slot's (row, column), plus the way from the leader to the
/// follower, then the row's spacing along that angle's x and the column's along its z, from the leader. The positions
/// and the goal are in 16.16 map units, because a goal in metres can change by one unit when it goes back
[[nodiscard]] glm::ivec2 FormationGoal(glm::ivec2 leader, glm::ivec2 follower, FormationSlot slot);

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

/// Whether a state, by its number in the game's animal state table, is the state of a reaction: fleeing (6), looking
/// (7), following (8), inspecting (9), going to food or wood and arriving (19 to 22), going to a ball and arriving (25
/// and 26), watching what it fled (30) and fleeing a hunter (49). An animal leaves its reaction when it leaves one
[[nodiscard]] constexpr bool IsReactionState(uint8_t gameState)
{
	return (gameState >= 6 && gameState <= 9) || (gameState >= 19 && gameState <= 22) || gameState == 25 || gameState == 26 ||
	       gameState == 30 || gameState == 49;
}
/// Whether a state, by its number in the game's animal state table, has a start of its own: wandering (32), sleep (35),
/// hiding in a lair (51) and looking for food (52)
[[nodiscard]] constexpr bool StateHasOwnStart(size_t gameState)
{
	return gameState == 32 || gameState == 35 || gameState == 51 || gameState == 52;
}

/// An animal at most this high above the land leans its model to the slope of the land when it is drawn
inline constexpr float k_HighestToFollowSlope = 0.2f;
/// The most that the model of an animal lifts or drops along one of its axes for the slope of the land
inline constexpr float k_MostSlopeLift = 0.3f;
/// A position in metres as the map unit that the land's height is read at for the slope: the metres times 65536,
/// then times 0.1, each as a float, then cut to a whole number
[[nodiscard]] int32_t SlopeSampleUnit(float metres);
/// The lift of the model along one axis: the height of the land at the end of the axis less the height of the
/// animal, kept between -0.3 and 0.3
[[nodiscard]] float SlopeLift(float landHeight, float animalHeight);
/// The lean of the model in its own axes: its x axis lifts by `alongX` and its z axis by `alongZ`, as a part of its
/// up axis. The heading turn comes before it and the bank after it
[[nodiscard]] glm::mat3 SlopeLean(float alongX, float alongZ);

/// What decides whether food interests an animal
struct FoodInterest
{
	/// The food is in the world and nothing takes it away now
	bool available {false};
	/// The physics moves the food now
	bool inPhysics {false};
	/// The hunger of the kind of animal at which it looks for food. A kind with 0 never looks for food
	uint32_t hungerLimit {0};
	/// How hungry the animal is now
	int16_t hunger {0};
	/// The kinds of food that the kind of animal eats, and the kinds of the food
	FoodType eats {FoodType::None};
	FoodType food {FoodType::None};
};
/// Whether food interests an animal. The food must be available and out of the physics. The animal must have a hunger
/// limit, be at that limit or more, and eat a kind of the food
[[nodiscard]] bool InterestedInFood(const FoodInterest& facts);

/// The priority of a reaction to food for an animal: the table priority when the food holds some food, is not farther
/// than the furthest distance of the reaction and interests the animal; else 0
[[nodiscard]] uint32_t FoodPriority(bool holdsFood, float distance, float furthest, bool interested, uint32_t tablePriority);

/// The largest turn of an animal to a thing that it looks at, in game angles each turn
inline constexpr uint16_t k_LookTurnStep = 0x80;
/// The angle that an animal faces after a turn to a thing that it looks at. Nearer than the step, it faces the thing.
/// Else it turns by the step, on the shorter side
[[nodiscard]] uint16_t LookTowards(uint16_t facing, uint16_t target, uint16_t step);

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
/// Whether an animal takes its own turn: not while a tornado or the physics moves it. In the player's hand or in the
/// hand of a creature it takes its turn as in the land. Its state in the hand does nothing, but its needs grow.
[[nodiscard]] bool TakesOwnTurn(const AnimalHolders& holders);
/// Whether an animal in the state of the hand can go into this state of the game's table. The state of the hand lets it
/// go only into flight, a landing or a state of death
[[nodiscard]] bool HandStateAllows(uint8_t gameState);
/// What an animal knows about its target at the start of its turn
struct TargetFacts
{
	/// The target is not in the world now
	bool gone {false};
	/// The player's hand holds the target. The hand of a creature does not count
	bool inPlayersHand {false};
	/// A hand holds the animal itself
	bool inHand {false};
};
/// What an animal does with its target at the start of its turn
enum class TargetDrop : uint8_t
{
	/// It keeps the target, and its turn continues
	Keeps,
	/// It lets go of the target and its count of turns, and its turn stops. In a hand it stays in the state of the hand
	Drops,
	/// It lets go of the target and its count of turns, and it decides again. Its turn stops
	DropsAndDecides,
};
/// An animal lets go of a target that is gone or in the player's hand. Every state lets it decide again, but the state of
/// the hand refuses this change
[[nodiscard]] TargetDrop DropOfTarget(const TargetFacts& facts);
/// The state that an animal goes into when it comes down from the hand or the physics
enum class LandedAs : uint8_t
{
	/// It has life, and it plays its landing
	Landed,
	/// It has no life left, and it starts to die where it lands
	Dying,
	/// It has no life left, and it was dying before, so its body lies dead at once
	Dead,
};
[[nodiscard]] LandedAs LandsAs(bool alive, bool dying);
/// A body going leaves a grey puff of smoke this big, half its height out along the way it faces from where it lies
inline constexpr float k_DeadBodySmokeSize = 1.0f;
[[nodiscard]] glm::vec3 DeadBodySmokePlace(const glm::vec3& position, float heading, float height);

} // namespace openblack::animals
