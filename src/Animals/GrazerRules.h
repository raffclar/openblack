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

#include <glm/vec2.hpp>

#include "3D/AllMeshes.h"
#include "Enums.h"

// The animals that graze the land: sheep, cows, horses, pigs and tortoises. They all live the same way and differ only
// in their clips and table values. A herd keeps about its home round its leader: each wanders in straight lines, steered
// afresh as it crosses into each new cell of the map towards the others; hungry, it walks to a cell of fresh grass ahead
// and grazes there; tired, it goes to sleep at the herd's sleeping place; and a herd that has lost members has young to
// make them up again. Positions are map units (6553.6 to the metre), angles game angles (2048 to the circle), speeds
// speed states (map units a turn). Pure functions, tested on their own.

namespace openblack::animals::grazers
{

/// The kinds that graze. The game's goats and zebras have table rows but are never made.
[[nodiscard]] bool IsGrazer(AnimalInfo type);

/// The clips a grazer plays, by what it is doing
enum class ClipSlot : uint8_t
{
	/// Walking, or running at speed
	Move,
	/// Standing to decide what to do, to be born and in a script's hands
	Stand,
	/// Grazing, one of its two clips at random
	Eat,
	/// Lowering its head to graze, and raising it when done
	StartToEat,
	FinishEating,
	Sleep,
	InHand,
	Thrown,
	/// Falling dead
	Dying,
};
/// The speeds of the table that a kind's clips change at: walking up to the first, running past it (a horse trots up
/// to the second and gallops past it)
struct SpeedThresholds
{
	uint16_t walk {0};
	uint16_t run {0};
};
/// The row of the table's speed thresholds a kind's clips use: the cow's for cows, sheep and pigs, the horse's for
/// horses. Tortoises always play the same clip.
[[nodiscard]] SpeedThreshold ThresholdRowOf(AnimalInfo type);
/// A grazer's clip. `roll(n)` is the game's random whole number below n, drawn only for grazing.
[[nodiscard]] AnimId Clip(AnimalInfo type, ClipSlot slot, uint16_t speed, SpeedThresholds thresholds,
                          const std::function<uint32_t(uint32_t)>& roll);

/// A whole number scaled by num / den, truncated
[[nodiscard]] int32_t Scale(int32_t value, int32_t num, int32_t den);
/// Adds a push to a step within what is left of the speed (the speed less the step's larger side): the push is cut to
/// what is left, by its larger side. Whether the speed is used up.
bool AddSteer(glm::ivec2& step, glm::ivec2 push, uint16_t speed);

/// Another member of the herd as a wanderer sees it: where it is and the step it last took
struct HerdMate
{
	glm::ivec2 position {0};
	glm::ivec2 step {0};
};
/// The herd's pull on a wanderer's step: a fifth of its speed towards the middle of the others, then that again with
/// its nearest mate's pull added (towards the mate along each axis where the mate is further off than the flock
/// distance, away where nearer; the distance is compared with map units, so in practice it always pulls), then three
/// fifths of the nearest mate's step. `mates` are the others, the newest member first. Whether the speed is used up.
bool HerdSteer(glm::ivec2& step, glm::ivec2 position, uint16_t speed, std::span<const HerdMate> mates, uint16_t flockDistance);

/// A wanderer's new straight step, as it sets off and as it crosses into a new cell
struct WanderSetup
{
	glm::ivec2 position {0};
	uint16_t angle {0};
	uint16_t speed {0};
	uint16_t turnAngle {0};
	/// The point it keeps within its distances of: its herd's leader. None for none.
	std::optional<glm::ivec2> centre;
	/// The distances it keeps from the centre, in whole metres: further than the outer one it heads back at nine
	/// tenths of its speed, nearer than the inner one it heads away
	int32_t inner {0};
	int32_t outer {0};
	uint16_t flockDistance {0};
};
/// The new step: back towards or away from its centre, then the herd's pull, then with what is left of its speed a step
/// turned at random by up to half its turn angle either way. `roll(n)` is the game's random whole number below n.
[[nodiscard]] glm::ivec2 NewWanderStep(const WanderSetup& setup, std::span<const HerdMate> mates,
                                       const std::function<uint32_t(uint32_t)>& roll);

/// A grazer's needs, each counting up turn by turn to its kind's limit
struct Needs
{
	int16_t hunger {0};
	int16_t sleep {0};
	int16_t breed {0};
};
/// A kind's limits from the table; a limit of 0 is a need it never has
struct NeedLimits
{
	uint32_t hunger {0};
	uint32_t sleep {0};
	uint32_t breed {0};
	/// Only animals of this age and more breed
	uint32_t grownUpAge {0};
};
/// How many a herd has, and the most it has had
struct HerdSize
{
	uint32_t members {0};
	uint32_t most {0};
};
/// A turn of its needs, in a state it sees to them in: each grows up to its limit, the need to breed only in a herd of
/// two and more that has lost members, of a grown animal
void GrowNeeds(Needs& needs, const NeedLimits& limits, uint32_t age, std::optional<HerdSize> herd);
/// Whether it is ready to breed as it decides what to do: its need to breed is full and its herd has lost members. Full
/// in a herd that hasn't, the need starts again.
[[nodiscard]] bool ReadyToBreed(Needs& needs, const NeedLimits& limits, std::optional<HerdSize> herd);
/// What a grazer goes to see to
enum class Need : uint8_t
{
	None,
	/// It gives birth, its need to breed starting again
	Breed,
	/// It looks for grass; finding none it sees to nothing else this turn
	Graze,
	/// It goes to sleep at its herd's sleeping place
	Sleep,
};
/// Its needs, by their order: breeding (a grown animal's need grows once more, and when full and its herd has lost
/// members it breeds; full otherwise, it starts again), then hunger, then sleep where it has a sleeping place
[[nodiscard]] Need NeedToSee(Needs& needs, const NeedLimits& limits, uint32_t age, std::optional<HerdSize> herd,
                             bool hasSleepingPlace);

/// The cells of the spiral a grazer looks for grass in, out from where it stands: the square of a tenth of its herd's
/// reach in metres
[[nodiscard]] int32_t GrazeSearchCells(uint16_t domainRadius);
/// The first point of the spiral of cells out from where it stands that suits it, keeping its place within each cell
[[nodiscard]] std::optional<glm::ivec2> FindGrazeSpot(glm::ivec2 position, uint16_t domainRadius,
                                                      const std::function<bool(glm::ivec2)>& suits);

/// Where a sleeper lies down: at random in a square about its herd's sleeping cell's corner, two metres a side for each
/// member. `random(max)` is the game's random number up to max, drawn for x then z.
[[nodiscard]] glm::ivec2 SleepSpot(glm::ivec2 cell, uint32_t members, const std::function<float(float)>& random);
/// A sleeper's turn: its need to sleep runs down by two (one more is added back while it sleeps). Whether it wakes.
[[nodiscard]] bool SleepTurn(Needs& needs);

/// A young grazer's size, grown a quarter of a year: a random way up to three quarters of the way to its table size
/// for the year after its age. `random(max)` is the game's random number up to max.
[[nodiscard]] float GrownScale(float scale, uint32_t age, std::span<const float> ageToScale,
                               const std::function<float(float)>& random);

/// Two herds merging: whether the one looking keeps them all (it is as big or bigger and the other isn't a script's)
[[nodiscard]] bool MergeKeepsLooker(uint32_t looker, uint32_t other, bool otherScripted);
/// The most the keeper has had once the other's members join it: the other's most is added once for each member that
/// joins, kept within the kind's largest herd each time
[[nodiscard]] uint32_t MergedMost(uint32_t keeperMost, uint32_t otherMost, uint32_t joining, uint32_t maxFlockSize);

/// A grazer eats this many clips' worth, and up to this many more
inline constexpr uint32_t k_MealsMin = 20;
inline constexpr uint32_t k_MealsRange = 15;
/// The game draws for the common animal's meal before a grazer's own: 15 and up to 10 more
inline constexpr uint32_t k_CommonMealsMin = 15;
inline constexpr uint32_t k_CommonMealsRange = 10;

} // namespace openblack::animals::grazers
