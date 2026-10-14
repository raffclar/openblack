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

#include <span>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureIdleMind.h"

/// What a creature does to get better and to go home: it rests until it is better, heals itself with a miracle, goes
/// or runs home, and goes into, prays at or looks at the temple of its player.
namespace openblack::creature_mind
{
/// Another creature nearer than this many times the creature's height frightens it too much to rest
constexpr float k_FrighteningHeights = 8.0f;
/// Whether no other creature is near enough to frighten a creature of a height, by the distances in metres of the others
[[nodiscard]] bool NothingFrighteningNear(float height, std::span<const float> otherCreatures);

/// A creature goes home by itself only when it is further than this from its home, in metres
constexpr float k_NearHome = 20.0f;
[[nodiscard]] bool FarFromHome(float homeDistance);
/// A creature prays at its temple, or sleeps at home, only when its player has a temple and it is nearer to its home
/// than this, in metres
constexpr float k_FurthestFromHome = 1000.0f;
[[nodiscard]] bool NotTooFarFromHome(bool hasTemple, float homeDistance);

/// Whether a creature that rests is better: its life is full and it is not tired at all
[[nodiscard]] bool IsBetter(float life, float exhaustion);

/// The rest itself: the sleep clips, mirrored half the time, with the eyes closed. The loop lasts until the creature is
/// better. The action counts as done once it is up.
[[nodiscard]] Step RestStep();

/// Resting to get better where it is: facing down the slope, resting, then a dazed look about with a grimace
[[nodiscard]] std::vector<Step> RestToGetBetter();
/// Going home to recover: walking home until within its height (no further than 5), then resting to get better
[[nodiscard]] std::vector<Step> GoHomeToRecover(glm::vec2 home, float height);
constexpr float k_FurthestFromHomeToRecover = 5.0f;

/// Healing itself: the creature casts the heal at the place where it stands, with a grimace, held three seconds; then
/// its life becomes full and its wounds heal
[[nodiscard]] std::vector<Step> HealItself(uint32_t healMagicType, glm::vec3 place);
/// A heal makes the wounds and blood of the creature age as much as this many game turns of healing
constexpr uint32_t k_HealItselfCounts = 9600;

/// Resting on the spot: waiting a while, exhausted, looking at its own feet
[[nodiscard]] std::vector<Step> RestOnTheSpot(uint32_t self, float seconds);
/// It rests on the spot three seconds and up to eight more
constexpr float k_RestOnTheSpotSeconds = 3.0f;
constexpr float k_RestOnTheSpotExtraSeconds = 8.0f;

/// Casting the one-shot miracle in its hand at itself to restore its health: the globe goes from its hand, and it casts
/// the miracle from the casting pose with a grimace
[[nodiscard]] std::vector<Step> CastRestoreHealthInHand(uint32_t magicType, uint32_t self);
/// Picking up a globe that restores health and casting its miracle at itself. It picks the globe up only with an empty
/// hand. It waits a while, then what its hand holds goes from the world, it casts, and the globe goes from the world.
[[nodiscard]] std::vector<Step> PickUpAndCastRestoreHealth(uint32_t magicType, uint32_t self, uint32_t globe, bool handEmpty,
                                                           float waitSeconds);
/// It waits ten seconds and up to twenty more before the cast
constexpr float k_RestoreHealthWaitSeconds = 10.0f;
constexpr float k_RestoreHealthWaitExtraSeconds = 20.0f;

/// Going home: walking home until within its height, no further than 5
[[nodiscard]] std::vector<Step> GoHome(glm::vec2 home, float height);
constexpr float k_FurthestFromHomeToStop = 5.0f;
/// Running home: half the time it is frightened first, with its eyes wide; then it runs home until within two times its
/// height
[[nodiscard]] std::vector<Step> RunHome(glm::vec2 home, float height, bool frightenedFirst);
constexpr uint32_t k_FrightenedBeforeRunningHomeLots = 2;
constexpr float k_RunHomeHeights = 2.0f;

/// Where a creature stops as it goes into its temple: on the line from where it stands to the heart, short of the heart
/// by 1.3 times the heart's route radius. The heart's route radius is a third of its radius on the ground. The
/// distance is in three dimensions, the heart on the ground.
[[nodiscard]] glm::vec2 TempleStop(glm::vec3 standing, glm::vec3 heart, float heartRadius);
constexpr float k_HeartRouteShare = 0.33f;
constexpr float k_TempleStopRoutes = 1.3f;
/// Going into its temple: walking home until within its height, turning to face the heart with a smile, then walking
/// to the place where it stops
[[nodiscard]] std::vector<Step> EnterTemple(glm::vec2 home, glm::vec3 heart, glm::vec2 stop, float height);

/// Praying at its temple: walking home until within its height, then praying. The action counts as done as it arrives.
[[nodiscard]] std::vector<Step> PrayAtTemple(glm::vec2 home, float height);
constexpr size_t k_PrayAnimation = 221;

/// Looking at a part of a temple: facing it two seconds, watching its top three seconds, one time in three pointing at
/// it two seconds, amazed, then watching its foot three seconds and its top two seconds
[[nodiscard]] std::vector<Step> LookAtTemple(uint32_t part, bool pointsAtIt);
constexpr uint32_t k_PointAtTempleLots = 3;
constexpr float k_FaceTempleSeconds = 2.0f;
constexpr float k_WatchTempleTopSeconds = 3.0f;
constexpr float k_PointAtTempleSeconds = 2.0f;
constexpr float k_WatchTempleFootSeconds = 3.0f;
constexpr float k_WatchTempleTopAgainSeconds = 2.0f;
} // namespace openblack::creature_mind
