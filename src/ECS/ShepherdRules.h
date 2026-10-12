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

#include "3D/MapCoords.h"
#include "Enums.h"

/// The rules of a shepherd's work and of finding water to drink. A villager sent for food may take one of its town's
/// flocks no one herds: it walks to the flock's leader and joins the flock, then either takes the flock to drink at its
/// home's water and brings it back, or, when its town wants food badly enough, leads one animal to the store, where the
/// animal becomes food. Free of the game's state so they can be tested on their own.
namespace openblack::ecs::shepherd_rules
{

/// A flock this far from the villager or more counts for nothing when it chooses a food job, in metres
inline constexpr float k_FlockReach = 300.0f;
/// Turns the shepherd looks its flock over once it has reached the leader
inline constexpr int16_t k_LookOverTurns = 50;
/// Turns the flock stays at the water before the shepherd brings it back
inline constexpr int16_t k_TurnsAtWater = 900;
/// Turns of the shepherd's short waits: before deciding again, before going home, and between looks at the animal it
/// leads
inline constexpr int16_t k_ShortWait = 20;
/// Turns the shepherd waits inside the store's door before it takes the animal
inline constexpr int16_t k_WaitAtDoor = 5;
/// How far a shepherd's home looks for water when it is built, and how far the shepherd looks when it has none, in
/// metres
inline constexpr float k_HomeWaterReach = 200.0f;
inline constexpr float k_ShepherdWaterReach = 400.0f;
/// The animal the shepherd picks to lead to the store is nearer than this, in metres
inline constexpr float k_PickAnimalReach = 100.0f;
/// The shepherd walks on to the store only while the animal it leads keeps within this, in metres
inline constexpr float k_LeadReach = 6.0f;
/// The animal is taken once it is within this of the shepherd at the store's door, in metres
inline constexpr float k_TakeReach = 4.0f;
/// Further than this from the store's door the shepherd walks a quarter of the way at a time, in metres
inline constexpr float k_FarFromStore = 100.0f;
/// Further than this, and no further than the above, it walks this far at a time; nearer, straight to the door
inline constexpr float k_NearStore = 30.0f;
inline constexpr float k_StepNearStore = 20.0f;
/// The flock of an animal being led keeps it this near the shepherd, in metres
inline constexpr uint16_t k_LedFlockDistance = 4;
/// Above this the town wants food badly enough to take a flock's last animal
inline constexpr float k_HungryTown = 0.5f;
/// Every villager reads its shepherding season from the normal shepherd's row of the jobs table
inline constexpr JobInfo k_ShepherdJob = JobInfo::NormalShepherd;

/// Whether the job's row says to work at all this season
[[nodiscard]] constexpr bool JobActive(JobActivity activity)
{
	return activity != JobActivity::None;
}

/// A flock of the villager's town, as the villager weighs it: whether someone herds it already, and how far its place
/// is from the villager in metres
struct FlockCandidate
{
	bool herded {false};
	float distance {0.0f};
};
/// The flock the villager would herd, and how much it wants to: the nearest flock no one herds, the first of equals;
/// none when no flock is near enough to count
struct FlockChoice
{
	std::size_t index {0};
	float score {0.0f};
};
[[nodiscard]] std::optional<FlockChoice> ChooseBestFlock(std::span<const FlockCandidate> flocks);

/// What the shepherd does with its flock after looking it over
enum class FlockDecision : uint8_t
{
	/// Only the shepherd is left in the flock: it lets the flock go and goes home
	Release,
	/// Its town wants food: it leads an animal to the store
	Slaughter,
	/// It takes the flock to drink
	MoveToWater,
};
/// `members` counts the shepherd. The town wants food enough once its want reaches its trigger, and then the shepherd
/// takes an animal if two or more are left, or the last one when the town wants food badly.
[[nodiscard]] FlockDecision DecideWithFlock(std::size_t members, float townWantsFood, float trigger);

/// How the shepherd walks towards the store while leading an animal, by its distance to the store's door
enum class StoreLeg : uint8_t
{
	/// A quarter of the way
	Quarter,
	/// A fixed step
	Step,
	/// Straight to the door
	Door,
};
[[nodiscard]] StoreLeg StoreLegFor(float distanceToDoor);
/// Where the shepherd walks to next on the way to the store's door
[[nodiscard]] map_coords::MapCoords StoreLegGoal(const map_coords::MapCoords& from, const map_coords::MapCoords& door);

/// A member of the shepherd's flock as it looks for an animal: where it stands, and whether it is the shepherd itself
/// or another villager
struct MemberView
{
	map_coords::MapCoords position;
	bool self {false};
	bool villager {false};
};
/// The member nearest to a place, nearer than `reach` metres, that is neither the shepherd nor a villager; the first of
/// equals
[[nodiscard]] std::optional<std::size_t> NearestAnimal(std::span<const MemberView> members, const map_coords::MapCoords& from,
                                                       float reach);

/// The food the shepherd takes from an animal of a kind giving this food: its whole food value when it is meat or
/// vegetable food, none otherwise
[[nodiscard]] int16_t SlaughterFood(float foodValue, FoodType foodType);

/// Water to drink near a place. The nearest point of the rivers within reach, measured between the ground under each,
/// and then any coast nearer still; with no river point in reach, the nearest coast in reach.
struct River
{
	/// Every river's points in map positions, river by river
	std::span<const map_coords::MapCoords> points;
	/// The land's height at a point, in metres
	std::function<float(const map_coords::MapCoords&)> groundHeight;
};
/// Whether a map cell is dry coast
using IsCoast = std::function<bool(const map_coords::MapCoords&)>;
[[nodiscard]] std::optional<map_coords::MapCoords> NearestRiverPoint(const map_coords::MapCoords& from, const River& river,
                                                                     float reach);
/// The coast is searched cell by cell in a spiral out from the place, keeping its place within the cells, until a cell
/// is further than reach
[[nodiscard]] std::optional<map_coords::MapCoords> NearestCoast(const map_coords::MapCoords& from, const IsCoast& isCoast,
                                                                float reach);
[[nodiscard]] std::optional<map_coords::MapCoords> NearestDrinkingWater(const map_coords::MapCoords& from, const River& river,
                                                                        const IsCoast& isCoast, float reach);

} // namespace openblack::ecs::shepherd_rules
