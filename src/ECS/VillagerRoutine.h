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

#include <glm/vec2.hpp>

#include "Enums.h"

/// The rules of a villager's day around its home, as the game has them: where it steps out of its door, where it sits
/// to while away time, what it does with nothing to do, how it sleeps and when it wakes, where the homeless roam and
/// lie down, and how long a town's emergency lasts. Pure: random numbers come from the caller, in the game's order.
namespace openblack::ecs::villager_routine
{

/// Random numbers on the game's shared stream: a float from 0 up to a limit, and a whole number below a limit
using FloatRandom = std::function<float(float)>;
using IntRandom = std::function<uint32_t(uint32_t)>;

/// A quarter and an eighth of a turn, as the game writes them
constexpr float k_QuarterPi = 0.7853982f;
constexpr float k_EighthPi = 0.39269909f;
constexpr float k_HalfPi = 1.5707964f;
constexpr float k_TwoPi = 6.2831855f;

/// A point a distance from another along an angle in radians, on the map's fixed-point grid as the game moves positions
[[nodiscard]] glm::vec2 Polar(glm::vec2 from, float angle, float metres);

/// A villager knocked out of its home comes out this far from its door, and up to as far again
constexpr float k_OutsideDoorLeast = 1.5f;
constexpr float k_OutsideDoorRange = 1.5f;
/// Where a villager stands after coming out of its home: a few steps out from the door, straight out from the home
/// give or take an eighth of a turn. The distance is drawn first.
[[nodiscard]] glm::vec2 PosOutsideDoor(glm::vec2 abode, glm::vec2 door, const FloatRandom& random);

/// A spot about a building's door: out from it within a share of a turn (a turn divided into `divisions`), at least
/// `leastDistance` and up to `range` more. The angle is drawn first.
[[nodiscard]] glm::vec2 PosOutside(glm::vec2 abode, glm::vec2 door, float divisions, float leastDistance, float range,
                                   const FloatRandom& random);

/// A tenth of how far from the town's gathering place people sit about: the least distance they sit at
constexpr float k_ChillOutRadiusShare = 0.1f;
/// They sit within this many of those tenths past it
constexpr float k_ChillOutRadiusSpan = 9.0f;
/// Where a villager goes to sit about its town: on its own side of the gathering place, give or take an eighth of a
/// turn, from a tenth of the town's chill-out distance to the whole of it. The angle's jitter is drawn first.
[[nodiscard]] glm::vec2 ChillOutPos(glm::vec2 congregation, glm::vec2 villager, float chillOutDistance,
                                    const FloatRandom& random);

/// What a villager with nothing to do does
enum class Idle : uint8_t
{
	GoHome,
	ChillOutsideHome,
	SitInTown,
};
/// One of nine ways: one in nine goes home if its home works (or one time in ten without), three go and sit outside
/// their home, five go and sit about their town; each falls through to the next when it can't, ending at home
[[nodiscard]] Idle NothingToDo(bool hasAbode, bool abodeFunctional, bool hasTown, const IntRandom& random);

/// A villager knocked out of its home won't go to bed again for its next decision, unless it is badly hurt
[[nodiscard]] constexpr bool MaySleep(bool woken, float life, float goHomeLife)
{
	return !woken || goHomeLife > life;
}

struct Sleep
{
	bool poisoned {false};
	float life {1.0f};
	/// The life its kind is made with
	float fullLife {1.0f};
	/// Life a check restores
	float restores {0.0f};
	/// How much more or less rest it gets where it lies
	float multiplier {1.0f};
	/// Sleep is what its town wants most
	bool townWantsSleep {false};
	/// It sleeps on while its life is under this
	float sleepUntil {0.0f};
};
struct SleepResult
{
	/// Life it gains from the check
	float gain {0.0f};
	/// It sleeps on to the next check
	bool sleepsOn {false};
};
/// A check on a sleeper: unless poisoned, which wakes it, it gains life when short of its full life, then sleeps on
/// while its town wants sleep most or its life is still under the mark
[[nodiscard]] SleepResult CheckSleep(const Sleep& sleep);

/// Where a homeless villager heads home to its town: from far off, to the near side of the town; nearby, a spot round
/// itself to lie down at, and failing that one further off
constexpr float k_FarFromTown = 100.0f;
constexpr float k_TowardsTownLeast = 10.0f;
constexpr float k_TowardsTownRange = 25.0f;
constexpr float k_TentSearchLeast = 2.0f;
constexpr float k_TentSearchRange = 8.0f;
constexpr float k_HomelessRoamLeast = 10.0f;
constexpr float k_HomelessRoamRange = 20.0f;
/// From far off: the angle is drawn first, within a quarter turn either side of the line from the town to it
[[nodiscard]] glm::vec2 TowardsTown(glm::vec2 town, glm::vec2 villager, const FloatRandom& random);
/// Round itself: the distance is drawn first, then the angle
[[nodiscard]] glm::vec2 RoundAbout(glm::vec2 from, float least, float range, const FloatRandom& random);

/// A villager with no town looks this far for one to join
constexpr float k_VagrantTownSearch = 200.0f;
constexpr float k_VagrantWanderLeast = 10.0f;
constexpr float k_VagrantWanderRange = 20.0f;
constexpr float k_VagrantTentRange = 5.0f;
/// A vagrant wanders on roughly the way it faces: within an eighth of a turn either way, angle drawn first
[[nodiscard]] glm::vec2 VagrantWander(glm::vec2 villager, float facing, const FloatRandom& random);
/// A hurt vagrant looks for a spot to lie down close by: the distance is drawn first, then the angle
[[nodiscard]] glm::vec2 VagrantTentSearch(glm::vec2 villager, const FloatRandom& random);

/// A hurt villager whose home is broken lies down outside it, away from it give or take an eighth of a turn: the jitter
/// is drawn first, then the distance
constexpr float k_BrokenHomeTentLeast = 5.0f;
constexpr float k_BrokenHomeTentRange = 5.0f;
[[nodiscard]] glm::vec2 BrokenHomeTentSearch(glm::vec2 abode, glm::vec2 villager, const FloatRandom& random);

/// A spot to lie down in the open is moved on this far, and up to as far again past it, when it is taken
constexpr float k_TentRetryLeast = 3.0f;
constexpr float k_TentRetryRange = 5.0f;
/// Spots to try in the open
constexpr uint32_t k_TentTries = 3;
/// Another sleeper this close to a spot in the open takes it
constexpr float k_TentSleeperGap = 5.0f;
/// A spot taken in the open is moved on: the distance is drawn first, then the angle
[[nodiscard]] glm::vec2 TentRetry(glm::vec2 spot, const FloatRandom& random);

/// A tree a villager lies down by is looked for this far off; it lies this far from the tree, on the far side from what
/// is near it or else from itself
constexpr float k_TentTreeSearch = 50.0f;
constexpr float k_TentTreeGap = 2.0f;
/// Something this close to a tree, past its own size, is near it
constexpr float k_TentTreeCrowd = 4.0f;

/// A town is in an emergency for so many turns after one was last called
[[nodiscard]] constexpr bool IsInStateOfEmergency(uint32_t emergencyTurn, uint32_t turn, uint32_t lasts)
{
	return emergencyTurn != 0 && turn - emergencyTurn < lasts;
}

/// Whether a villager working towards a state leaves it to gather when its town calls an emergency: most states, but
/// not being inside its home, in bed, eating at home, sitting about or held by a script
[[nodiscard]] bool AnswersTownEmergency(VillagerStates state);

/// A villager sat about goes on sitting once its time is up, one time in ten it thinks of something else to do
constexpr uint32_t k_ChillOutRethinkChance = 10;
/// A villager at home with nothing to do goes to bed one time in this many
constexpr uint32_t k_GoToBedChance = 4;

} // namespace openblack::ecs::villager_routine
