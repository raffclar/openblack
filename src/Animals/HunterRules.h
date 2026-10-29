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
#include "Animals/GrazerRules.h"
#include "Enums.h"

// The wild hunters: the lions, tigers, leopards and wolves that the land and the scripts make. They grow hungry and
// sleepy as the grazers do. A hungry hunter looks for prey in the map cells about it, chases it, creeps up on it in its
// prowl and leaps at it. The big cats sleep late at night. A wolf pack keeps to its lair and goes out late at night.
// Pure functions, tested on their own.

namespace openblack::animals::hunters
{

/// Whether a kind is a wild hunter: a lion, a tiger, a leopard or a wolf. The miracle's wolves and the puzzle's
/// pieces are not
[[nodiscard]] bool IsWildHunter(AnimalInfo type);

/// The speeds of a hunter, by their place in its kind's speed group
inline constexpr size_t k_UsualSpeed = 0;
/// It creeps up on its prey at this speed
inline constexpr size_t k_StalkSpeed = 2;
/// It runs at its prey at this speed
inline constexpr size_t k_SprintSpeed = 3;
/// It goes after its prey and to where it last found prey at this speed
inline constexpr size_t k_HuntSpeed = 4;

/// A hunter eats this many mouthfuls, and up to this many more
inline constexpr uint32_t k_MouthfulsMin = 15;
inline constexpr uint32_t k_MouthfulsRange = 10;

/// The clips of a kind of hunter
struct KindClips
{
	AnimId stand {AnimId::Invalid};
	AnimId eat {AnimId::Invalid};
	AnimId startToEat {AnimId::Invalid};
	AnimId upFromEat {AnimId::Invalid};
	AnimId sleep {AnimId::Invalid};
	AnimId pounce {AnimId::Invalid};
	/// Its clips for a move: slower than its usual speed it prowls, faster than its kind's run speed it runs, and
	/// otherwise it walks
	AnimId prowl {AnimId::Invalid};
	AnimId walk {AnimId::Invalid};
	AnimId run {AnimId::Invalid};
};
/// The clips of a kind of wild hunter, none for another kind
[[nodiscard]] std::optional<KindClips> ClipsOf(AnimalInfo type);
/// The row of the table of speed limits for a kind of wild hunter
[[nodiscard]] SpeedThreshold ThresholdRowOf(AnimalInfo type);
/// The clip of a move at a speed
[[nodiscard]] AnimId MoveClip(const KindClips& clips, uint16_t speed, uint16_t usualSpeed, uint16_t runAbove);

/// The place in its speed group of the speed that a hunter takes on a chase, by its distance from its prey: it runs
/// within its attack distance, it creeps within its stalking distance, and within its hunting distance it creeps for
/// the first 33 turns of each 100 and goes at its hunting speed for the others. None further away.
[[nodiscard]] std::optional<size_t> ChaseSpeed(float distance, float attackDistance, float stalkingDistance,
                                               float huntingDistance, uint32_t turn);

/// Whether the big cats go to sleep as they decide what to do: late in the night, after 22 hours
[[nodiscard]] bool CatsSleepAt(float visualHours);
/// Whether a hungry wolf pack leaves its lair: late in the night, after 23 hours
[[nodiscard]] bool WolvesHuntAt(float visualHours);

/// What a hunter needs most, in the order that it sees to its needs
enum class Need : uint8_t
{
	None,
	Hunger,
	Sleep,
	/// Its need to breed is full and its flock has lost members
	Breed,
};
/// Its first need: to breed, then to eat, then to sleep. A full need to breed in a flock that has lost no members, or
/// with no flock, starts again.
[[nodiscard]] Need FirstNeed(grazers::Needs& needs, const grazers::NeedLimits& limits, std::optional<grazers::HerdSize> herd);

/// A flock that a hungry wolf pack can go to
struct FlockSeen
{
	/// The strength of the flock's leader, from its kind's table
	float leaderStrength {0.0f};
	/// The height of the flock's home above the land
	float height {0.0f};
	/// From the wolf to the flock's leader, in map units
	int32_t distance {0};
};
/// The flock that a hungry wolf in its lair goes to: the first nearest flock on the ground whose leader is no stronger
/// than the wolf. The distance is in hundreds of map units and must be within the reach of the wolf's own flock, in
/// metres. So in practice only a flock with a leader at about the wolf's place is near enough. None for none
[[nodiscard]] std::optional<size_t> PackGoal(std::span<const FlockSeen> flocks, float ownStrength, uint16_t reach);

/// A random map position in a square about a centre, both in map units: the square is `size` metres across. `random`
/// is the game's random number up to its maximum, drawn for x, then for z.
[[nodiscard]] glm::ivec2 RandomPointInSquare(glm::ivec2 centre, float size, const std::function<float(float)>& random);

} // namespace openblack::animals::hunters
