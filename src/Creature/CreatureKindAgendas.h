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
#include <vector>

#include <glm/vec2.hpp>

#include "Creature/CreatureIdleMind.h"

/// The agendas of the actions a creature does with particular kinds of thing: giving a tree to a town's store as wood,
/// and planting a tree somewhere else.
namespace openblack::creature_mind
{
/// The tree an action takes up first, when its hand doesn't hold a tree already
struct TreeToTake
{
	uint32_t tree {0};
	glm::vec2 point {0.0f};
	float radius {0.0f};
};

/// Giving a tree to a store as wood: unless it holds a tree, walking to the tree until within a fifth more than both
/// their radii and pulling it up; then going to where it can throw into the store, turning to face it a tenth of a
/// second (where the action counts as done), and throwing the tree in, with a look of compassion until it throws
[[nodiscard]] std::vector<Step> GiveWoodToStore(const std::optional<TreeToTake>& take, uint32_t store, float radius,
                                                float height);
constexpr float k_TreeReach = 1.2f;
constexpr float k_FaceStoreForWoodSeconds = 0.1f;

/// Planting a tree by a thing: unless it holds a tree, picking the tree up; then going right up to the thing, turning
/// to face it, and putting the tree down there, where it takes root
[[nodiscard]] std::vector<Step> PlantTreeBy(std::optional<uint32_t> tree, uint32_t by);

/// Practising a throw: unless its hand holds something, picking the thing up; then throwing what it holds at the point
/// three times its height further along both of the land's axes than where it stood as it set about it
[[nodiscard]] std::vector<Step> PracticeThrow(std::optional<uint32_t> pickUp, glm::vec2 from, float height);
constexpr float k_PracticeThrowHeights = 3.0f;

/// Throwing a toy at a thing: half the time a happy jump first; picking the toy up; going to where it can throw at the
/// thing and throwing the toy at it, angry, watching it; then half the time another happy jump
[[nodiscard]] std::vector<Step> ThrowToyAt(uint32_t toy, uint32_t target, glm::vec2 targetPoint, float height,
                                           const Random& random);
constexpr uint32_t k_HappyAroundToyThrowLots = 2;

/// Breaking a rock: practising a throw (unless its hand holds something, picking the rock up; then throwing what it
/// holds three times its height further along both of the land's axes), then breaking the rock in two as it flies
[[nodiscard]] std::vector<Step> BreakRock(std::optional<uint32_t> pickUp, uint32_t rock, glm::vec2 from, float height);

/// Stroking a cuddly toy: picking it up and stroking it in its hand, looking at it, with a look of compassion
[[nodiscard]] std::vector<Step> StrokeToy(uint32_t toy);

/// Taking a toy home: picking it up and walking it to a place within five of home along each of the land's axes
/// (offset by chance), until within the creature's height of it or five at most, then putting it down, eight times in
/// ten, or else tossing it away; all with a look of compassion. randomFraction is a chance from 0 to 1.
[[nodiscard]] std::vector<Step> TakeToyHome(uint32_t toy, glm::vec2 home, float height, const Random& random,
                                            const std::function<float()>& randomFraction);
constexpr float k_ToyHomeSpread = 10.0f;
constexpr float k_ToyHomeNearest = 5.0f;
constexpr uint32_t k_ToyPutDownLots = 10;
constexpr uint32_t k_ToyTossedBelow = 2;

/// Throwing a die: unless its hand holds something, picking the die up; throwing what it holds at the point 1.2 times
/// its height further along both of the land's axes than where it stood; watching the die fly, amazed; then two times
/// in three a happy jump, else a sad shake
[[nodiscard]] std::vector<Step> ThrowDie(std::optional<uint32_t> pickUp, uint32_t die, glm::vec2 from, float height,
                                         const Random& random);
constexpr float k_DieThrowHeights = 1.2f;
constexpr uint32_t k_DieOutcomeLots = 3;

/// Playing a throwing game with the player: picking up the toy and, when its hand held nothing as it set about it,
/// throwing it at the player's hand, watching the hand; then a happy jump
[[nodiscard]] std::vector<Step> ThrowToPlayer(uint32_t toy, glm::vec3 hand, bool handFull);

/// Helping repair a house: unless its hand holds something, picking up the tree; going right up to the house, turning
/// to face it a tenth of a second and putting down what it holds; mending the house by a fifth of its life, looking at
/// it; then doing away with the tree
[[nodiscard]] std::vector<Step> HelpRepairHouse(std::optional<uint32_t> pickUp, uint32_t house, uint32_t tree);
constexpr float k_FaceHouseSeconds = 0.1f;
/// What a creature's mending gives a building, of its life
constexpr float k_RepairLife = 0.2f;

/// Kicking a ball about: going up to it, turning to face it a tenth of a second, going right up to it again and kicking
/// it, watching it; then a quarter of the time pointing at it a second, else waiting 0.4 to 1 second (in whole turns).
/// It kicks it again after each, two times at the least, each time going on while the count is under two plus a chance
/// of nought to two. At the end, half the time, a happy jump.
[[nodiscard]] std::vector<Step> KickBallAround(uint32_t ball, const Random& random);
constexpr uint32_t k_LeastKicks = 2;
constexpr uint32_t k_MoreKicksLots = 3;
constexpr uint32_t k_PointAfterKickLots = 4;
constexpr float k_FaceBallSeconds = 0.1f;
constexpr float k_PointAtBallSeconds = 1.0f;
/// The wait after a kick: 4 to 9 turns of a tenth of a second, all as likely
constexpr uint32_t k_WaitAfterKickTurns = 4;
constexpr uint32_t k_WaitAfterKickMoreTurns = 6;
constexpr float k_KickTurnSeconds = 0.1f;
constexpr uint32_t k_HappyAfterKicksLots = 2;

/// Getting high on a magic mushroom: unless it holds a mushroom, picking up the thing; then eating it. A magic
/// mushroom eaten makes it high once it is done (see BecomeHigh).
[[nodiscard]] std::vector<Step> GetHigh(std::optional<uint32_t> pickUp, uint32_t eaten);

/// Where a creature high on a magic mushroom may sit down, when it does: the middle of a clear place about it four
/// times its radius across, and its radius
struct HighSeat
{
	glm::vec2 place {0.0f};
	float radius {0.0f};
};
/// Being high, once a magic mushroom it ate is done: amazed, it watches the player's camera three seconds and talks;
/// smiling with its eyes wide and darting about, it watches the camera two more seconds and waits a little over three;
/// it plays a dazed action, half the time a happy jump, a confused one; waits a little over a second; half the time
/// another dazed action, and a confused one again. Half the time it then goes to a clear place (when there is one,
/// found by findSeat) and sits there, sad, ten to fifteen seconds, then plays out two dazed actions, the second
/// frightened, and one of two more. At last, amazed, it watches the camera three seconds, talks and plays a dazed
/// action once more. Its eyes stay wide and darting about until it next takes up something else to do.
[[nodiscard]] std::vector<Step> BecomeHigh(const std::function<std::optional<HighSeat>()>& findSeat, const Random& random,
                                           const std::function<float(float)>& randomFloat);
constexpr size_t k_Talk = 205;
constexpr size_t k_HighDazed = 218;
constexpr size_t k_HighDazedAgain = 220;
constexpr size_t k_HighConfused = 63;
constexpr size_t k_HighSitDazed = 56;
constexpr size_t k_HighFrightened = 61;
constexpr size_t k_HighLastDazed = 219;
constexpr float k_HighWatchSeconds = 3.0f;
constexpr float k_HighStaredSeconds = 2.0f;
constexpr float k_HighWaitSeconds = 3.1f;
constexpr float k_HighPauseSeconds = 1.1f;
constexpr float k_HighSitSeconds = 10.0f;
constexpr float k_HighSitExtraSeconds = 5.0f;
constexpr float k_HighSeatRadii = 4.0f;
constexpr uint32_t k_HighChanceLots = 2;

/// The most trees a creature remembers planting others by
constexpr size_t k_PlantedRemembered = 10;

} // namespace openblack::creature_mind
