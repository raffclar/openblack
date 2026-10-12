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

#include <optional>
#include <span>

#include <glm/vec2.hpp>

#include "3D/MapCoords.h"
#include "Enums.h"

/// The rules of the foresters' and fishermen's work: where a villager stands to fell a tree or pick up wood, whether
/// wood lying about catches its eye, what is left of a fallen trunk once wood is taken from it, which forest of a town
/// is nearest, which food job a villager takes up, and where a fisherman stands. Free of the game's state so they can be
/// tested on their own.
namespace openblack::ecs::villager_work
{

/// A villager fells a tree standing this far beyond its own edge from the trunk
inline constexpr float k_TreeWorkingGap = 0.9f;
/// Before it swings its axe the forester turns this far to its right, in the game's angles (2048 to a turn)
inline constexpr int32_t k_ChopTurn = 0x124;
/// A forest this near the forester with no trees and no big forest goes when it finds no tree to fell
inline constexpr float k_EmptyForestReach = 50.0f;
/// The map cells a forester looks through for a tree: its own and the eight round it
inline constexpr int32_t k_TreeSearchCells = 9;
/// While wood it is going for still moves, a villager waits this many turns beside it before looking again
inline constexpr uint16_t k_WaitForWoodToLand = 10;
/// A villager walking up to wood still moving stops this near the spot it stands to pick it up
inline constexpr float k_NearMovingWood = 0.5f;
/// A big forest gives its wood at this share of its radius from its middle, on the villager's side
inline constexpr float k_BigForestArriveShare = 0.5f;
/// A creature watching a villager go to fish or fell feels this much of its town's want
inline constexpr float k_EmpathyForWork = 0.5f;

/// Where a villager stands to fell a tree: out from the trunk towards the villager, its own size and a little more
[[nodiscard]] glm::vec2 TreeWorkingPosition(glm::vec2 tree, glm::vec2 villager, float villagerRadius);
/// Where a villager stands by anything else it works at: out from it towards the villager, by both their sizes
[[nodiscard]] glm::vec2 EdgeWorkingPosition(glm::vec2 thing, glm::vec2 villager, float thingRadius, float villagerRadius);
/// Where a villager takes wood from a big forest: halfway in from its edge, on the villager's side
[[nodiscard]] glm::vec2 BigForestArrivePosition(glm::vec2 bigForest, glm::vec2 villager, float radius);

/// Whether a villager is touching a point: its edge is nearer the point than it walks in a turn
[[nodiscard]] bool Touching(float distanceToPoint, float villagerRadius, float speedInMetres);
/// Whether a walker has got to a point: nearer than its step and some slack, in the map's whole units
[[nodiscard]] bool AreWeThere(map_coords::MapCoords walker, map_coords::MapCoords point, float wholeSpeed, float slack);

/// How long, in turns, a villager goes on waiting for wood that still moves before it gives up: a hundred seconds
[[nodiscard]] uint32_t TurnsToWaitForMovingWood(uint32_t millisecondsPerTurn);

/// Whether a villager working as a forester takes up wood it has just reacted to again at once: in the forester's
/// walking, looking, chopping and finishing states
[[nodiscard]] bool ReactsToWoodAgainAtOnce(VillagerStates finalState);

/// What a villager weighs about wood lying about, to see whether it goes for it
struct WoodInterestInputs
{
	/// The wood is there for the taking: not gone, not held, not in another's hands
	bool available {false};
	/// Its room left for more wood, and the most it carries
	int32_t room {0};
	int32_t maxCarried {1};
	int16_t held {0};
	/// It has a town, its life is above the share at which it goes home hurt, and it is free to react
	bool hasTown {false};
	bool healthy {false};
	bool free {false};
	/// The wood is a fallen or felled trunk, or the pile of a building site
	bool fallenTree {false};
	bool siteWood {false};
	/// Its building site still wants no more wood
	bool siteHasEnough {false};
	/// Its kind of disciple goes for any wood it sees: foresters, builders and craftsmen
	bool disciplePicksUpWood {false};
	/// Otherwise its town's want of wood as felt, its state's care for wood, how near the wood is (from almost 1 beside
	/// it to almost 0 at the reaction's reach), and the level at which the town's want moves its people
	float townWoodDesire {0.0f};
	float stateInterest {0.0f};
	float distanceModifier {0.0f};
	float townTrigger {0.0f};
	/// What a land's script adds to every villager's interest; none unless it sets it
	float interactDesire {0.0f};
};
/// Whether wood catches the villager's eye: always a fallen trunk it has room for, any wood for the disciples that fetch
/// it, never once its site has enough, and otherwise by its town's want, its room, its state and the wood's nearness
[[nodiscard]] bool IsInterestedInWood(const WoodInterestInputs& in);

/// Wood taken from a fallen trunk: all of it goes when it holds no more than is wanted, else it shrinks to fit what is
/// left
struct FallenWoodTake
{
	uint32_t taken {0};
	/// Its new size, none when it is used up and goes
	std::optional<float> scale;
};
[[nodiscard]] FallenWoodTake TakeFromFallenTree(uint32_t wood, uint32_t wanted, uint32_t woodValue, float multiplier);

/// One of a town's forests as the town weighs it for a villager: how far its nearest point is, and whether it is the
/// town's forest of lone trees about it
struct ForestCandidate
{
	float distance {0.0f};
	bool scenic {false};
};
/// The forest a town sends a villager to: the nearest of its ordinary forests nearer than a reach, else the nearest of
/// its forests of lone trees nearer than the reach; the first of equals
[[nodiscard]] std::optional<size_t> NearestTownForest(std::span<const ForestCandidate> forests, float reach);

/// The tree in the middle of a forest: the nearest its place of its grown and its growing trees, the growing one when
/// both are as near
[[nodiscard]] std::optional<size_t> CentreTree(std::optional<float> nearestGrown, std::optional<float> nearestGrowing);

/// What a villager that has wood does next
enum class GotWood : uint8_t
{
	/// It decides afresh: it holds none, or it is a disciple who always works at its own job
	Decide,
	/// It takes it to the building site it works for
	BuildingSite,
	/// It looks for a site that wants builders, and failing one takes it to the store
	LookForSiteOrStore,
};
[[nodiscard]] GotWood AfterGettingWood(int16_t woodHeld, bool workingDisciple, bool siteStillWantsIt);

/// What a fisherman does on getting to the fish farm
enum class FishingArrival : uint8_t
{
	/// It isn't in the farm's map cell: it walks there again
	WalkAgain,
	/// It stands right on the farm's own place: it moves to a spot round it to fish from
	MoveToSpot,
	/// It is in the farm's cell: it takes up its rod
	StartFishing,
};
/// A fisherman is in a farm's map cell when the whole cells of both its x and z are the farm's
[[nodiscard]] bool IsAtValidFishingPos(map_coords::MapCoords fisherman, map_coords::MapCoords farm);
[[nodiscard]] FishingArrival ArriveAtFishing(map_coords::MapCoords fisherman, map_coords::MapCoords farm);

/// The food jobs a villager weighs when its town wants food, in the order equals are kept
enum class FoodJob : uint8_t
{
	FishFarm,
	Field,
	Flock,
};
/// One food job's best place for the villager and how good it is
struct FoodJobCandidate
{
	FoodJob job {FoodJob::FishFarm};
	float score {0.0f};
	bool found {false};
};
/// What a villager sent for food does
struct FoodJobChoice
{
	/// It takes what it carries to the store first
	bool dropOff {false};
	/// Else the job it takes up, none when no job has a place for it
	std::optional<FoodJob> job;
};
/// The best food job by score, the first of equals in the order fish farm, field, flock; taking what it carries to the
/// store wins when that scores more: how full it is times how near the store is
[[nodiscard]] FoodJobChoice ChooseFoodJob(std::span<const FoodJobCandidate> jobs, int32_t room, int32_t maxCarried,
                                          float storeNearness);

/// How much a town wants a field farmed for a villager: a field still being sown wants sowers by how much is left to
/// sow and how much room it has for farmers; a grown field wants harvesters once its crop is ripe; none otherwise
struct FieldDesireInputs
{
	uint32_t timesSown {0};
	float timesToSow {1.0f};
	float age {0.0f};
	float ageGrowth {0.0f};
	float ageRecolt {0.0f};
	uint32_t farmers {0};
	uint32_t maxFarmers {1};
};
[[nodiscard]] float DesireToBeFarmed(const FieldDesireInputs& in);

/// A farmer works at random spots up to this many metres either way of the field's middle, along x and along z
inline constexpr float k_FieldSpotReach = 5.0f;
/// The next spot a farmer works the field at, from the field's middle and two draws of 0 to twice the reach, the first
/// for x and the second for z, kept as a map position is
[[nodiscard]] glm::vec2 FieldWorkSpot(glm::vec2 field, float drawX, float drawZ);

/// What a farmer does on getting to its field or its spot in it
enum class FarmerArrival : uint8_t
{
	/// It walks on to its spot
	WalkToSpot,
	/// It sows there, after its clip
	Sow,
	/// It digs up the crop there, after its clip
	Dig,
	/// The field wants nothing of it: it decides afresh
	Decide,
};
/// A field still being sown is sown, a grown and ripe one is dug, each once the farmer stands at its spot
[[nodiscard]] FarmerArrival ArriveAtField(bool sowing, bool grown, bool ripe, bool atSpot);

/// After digging up the crop, a farmer that got some and has less room than a load of crop takes it to the store;
/// otherwise it goes back to work
[[nodiscard]] bool TakesHarvestToStore(uint32_t given, int16_t roomAfter, float cropFoodValue);

} // namespace openblack::ecs::villager_work
