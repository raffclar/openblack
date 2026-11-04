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

#include <array>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <glm/vec3.hpp>

#include "CreatureRecord.h"
#include "ECS/VanillaFootpathImport.h"
#include "ECS/WallHugRules.h"
#include "Enums.h"
#include "Magic/ResourcePiles.h"

namespace openblack::gamesave
{
class VanillaWorld;
struct VanillaRecord;
} // namespace openblack::gamesave

namespace openblack::ecs::components
{
struct Dance;
struct WorshipIcon;
} // namespace openblack::ecs::components

// The towns, the buildings of the towns and the villagers of a world file of the game, as values that openblack can
// make again. The rules here use no service of the game, so that tests can give them a world that they make.
namespace openblack::ecs::vanilla_world_import
{

/// The game gives each row of its tables a number, in the order that it loads the rows. The last row that it loads
/// has the number 0, and each row before it has the number of the next row plus 1. The rows of the buildings come
/// first, and the rows of the villagers come next.
constexpr uint32_t k_FirstAbodeRowNumber = 2394;
constexpr uint32_t k_FirstVillagerRowNumber = k_FirstAbodeRowNumber - static_cast<uint32_t>(AbodeInfo::_COUNT);

/// The number of the first row of the table of the tribes. The game loads the 9 tribes, then the speeds (10 rows), the
/// ball (1), the football (1), the places of the football players (10), the dances (25), the states of the villagers
/// (255) and the states of the animals (53). The first totem comes right after them.
constexpr uint32_t k_FirstTribeRowNumber = 742;

/// The number of the first row of the table of the spell seeds. The game loads the spell seeds right before the
/// animals, so the last spell seed has the number that comes after the number of the first animal.
constexpr uint32_t k_FirstSpellSeedRowNumber = 2548 + static_cast<uint32_t>(SpellSeedType::BeamExplosion) + 1;

/// The building row that has the number, or nothing for a number of another table
[[nodiscard]] std::optional<AbodeInfo> AbodeRowOf(uint32_t number);
/// The tribe that has the number, or nothing for a number of another table
[[nodiscard]] std::optional<Tribe> TribeRowOf(uint32_t number);
/// The villager row that has the number, or nothing for a number of another table
[[nodiscard]] std::optional<VillagerInfo> VillagerRowOf(uint32_t number);
/// The spell seed that has the number, or nothing for a number of another table
[[nodiscard]] std::optional<SpellSeedType> SpellSeedRowOf(uint32_t number);

/// True when the import makes buildings of the row from the file: the abodes, the storage pit, the creche, the
/// wonder, the graveyard and the town centre of each tribe. The import removes the buildings of these rows that the land made.
[[nodiscard]] bool IsImportedBuilding(AbodeInfo row);

/// True when a villager in the state is in its abode: the game does not show it, and its abode counts it as present
[[nodiscard]] bool IsInsideHome(VillagerStates state);

/// True when the import can keep the values that the file keeps for the state: a state with no values, a state of a
/// fisherman, a farmer or a shepherd, the walk on a footpath, a script state, the dead state or a disciple with nothing
/// to do
[[nodiscard]] bool CanKeepStateValues(uint8_t state);

/// True when the import can keep a walk in the move state of the file. It keeps a walk that stands still, takes its
/// last step, steps straight through, goes straight, goes round a circle or goes out of a circle.
[[nodiscard]] bool CanKeepWalk(uint8_t moveState);

/// The states of a villager that decides again: it decides what to do, with no final state and no state before
[[nodiscard]] std::array<uint8_t, 3> DecideAgain();

/// The three states that a villager starts in after the import: the top state, the final state and the state before.
/// The import keeps the states of the file when it can keep the walk and the values of the top and final states.
/// Otherwise the villager decides again.
[[nodiscard]] std::array<uint8_t, 3> StartStates(const std::array<uint8_t, 3>& states, uint8_t moveState);

/// One value that a villager keeps in the file for one of its states: the number of an object, or bytes
struct ImportedStateValue
{
	std::optional<uint32_t> object;
	std::vector<uint8_t> bytes;
};

/// The work that the values of a state give: the field or the fish farm, and the spot in the field
struct ImportedWork
{
	/// The number of the object of the field or the fish farm in the file
	uint32_t target {0};
	/// The spot in the field that a farmer goes to next
	std::optional<gamesave::MapCoords> spot;
};

/// The work of a state of a fisherman or of a farmer, from the values of the file. Nothing for another state.
[[nodiscard]] std::optional<ImportedWork> WorkOf(uint8_t state, const std::vector<ImportedStateValue>& values);

/// The values that a script last gave a villager, which its script states keep: a place in map units (x, z), a number
/// and a pair of whole numbers. The game keeps the clip and the plays of a script clip in the same pair.
struct ImportedScriptValues
{
	int32_t x {0};
	int32_t z {0};
	float number {0.0f};
	uint32_t first {0};
	uint32_t second {0};
};

/// The values of a script state (held by the script, wandering round a place, playing a clip, in a crowd) from the
/// values of the file. Nothing for another state.
[[nodiscard]] std::optional<ImportedScriptValues> ScriptValuesOf(uint8_t state, const std::vector<ImportedStateValue>& values);

/// A place in metres that gives the map units again when the game changes it back to map units. The game makes the
/// place of a script from metres, so the map units of a file always come from a float.
[[nodiscard]] float MetresOfMapUnits(int32_t units);

/// The values of a state of a shepherd: the turns left of what it does with its flock, and the state that it goes to
/// when its wait ends (0 to decide again)
struct ImportedShepherdValues
{
	int16_t countdown {0};
	uint16_t pending {0};
};

/// The values of a state of a shepherd from the values of the file. Nothing for another state.
[[nodiscard]] std::optional<ImportedShepherdValues> ShepherdValuesOf(uint8_t state,
                                                                     const std::vector<ImportedStateValue>& values);

/// The reason for the death of a villager in the dead state, from the values of the file. Nothing for another state or
/// for a reason that the game does not have.
[[nodiscard]] std::optional<DeathReason> DeathReasonOf(uint8_t state, const std::vector<ImportedStateValue>& values);

/// True when the living status of the file puts the villager on its back (landing pose 2 in bits 4 and 5)
[[nodiscard]] bool LiesOnBack(uint16_t status);

/// What a trading disciple with nothing to do keeps: the resource that it carries (0 food, 1 wood) and the number of
/// the object of the town that it trades with in the file
struct ImportedTrade
{
	ResourceType resource {ResourceType::Food};
	uint32_t town {0};
};

/// The trade of a trading disciple with nothing to do, from the values of the file. Nothing for another state or for
/// another kind of disciple.
[[nodiscard]] std::optional<ImportedTrade> TradeOf(uint8_t state, uint8_t discipleType,
                                                   const std::vector<ImportedStateValue>& values);

/// The work that the hand gave a villager: a disciple (villager flag 0x200) or a villager that follows a disciple one
/// time (villager flag 0x400)
struct ImportedDisciple
{
	VillagerDisciple type {VillagerDisciple::None};
	bool follower {false};
};

/// The disciple work of a villager from its disciple type and its villager flags. Nothing when it is not a disciple or
/// a follower, or when the type is not a disciple type of the game.
[[nodiscard]] std::optional<ImportedDisciple> DiscipleOf(uint8_t discipleType, uint16_t villagerFlags);

/// The circle that a walk of the file heads for or goes round
struct ImportedCircle
{
	/// The turns until the walker comes to the circle, or 0xff when the circle is not in reach
	uint8_t turnsToObject {0};
	/// How far from its goal the walker was when it started round the circle, in 128ths of a metre less one
	uint32_t entryDistance {0};
	/// The number in the file of the thing of the circle, or 0 when the circle is not on a thing
	uint32_t object {0};
	/// For a thing, the circle of the thing that the save gave (-1 for its only circle). The load does not use it.
	/// When there is no thing, 1 for water or the edge of the land, 0 for no circle.
	int32_t index {0};
	/// A map position in the cell of the water or of the edge of the land
	gamesave::MapCoords coords {};
};

/// What the circle of a walk of the file is on
enum class CircleKind : uint8_t
{
	/// The walk has no circle
	None,
	/// A thing of the file
	Thing,
	/// Water or the edge of the land
	Landscape,
};

/// What the circle of a walk of the file is on
[[nodiscard]] CircleKind KindOf(const ImportedCircle& circle);

/// The circle of a thing that a walker takes after a load, from the number of circles of the thing: always the last
/// circle, whichever circle the save gave. Nothing for a thing with no circles.
[[nodiscard]] std::optional<size_t> CircleAfterLoad(size_t circles);

/// The circle of water or of the edge of the land that a walker takes after a load: a circle at the middle of the cell
/// of the map position
[[nodiscard]] wall_hug::BlockingCircle LandscapeCircleOf(const gamesave::MapCoords& coords);

/// The walk of a villager in the file
struct ImportedWalk
{
	/// Where the walk goes to
	gamesave::MapCoords goal {};
	/// The step of each turn, in whole map units, and its height
	int32_t stepX {0};
	int32_t stepZ {0};
	float stepAltitude {0.0f};
	/// Whole map units in a turn
	uint16_t speed {0};
	/// The way the walker faces, in game angles (2048 to the circle)
	uint16_t angle {0};
	/// Turns until a walker that steps straight through aims at its goal again
	int8_t turnsUntilStepRebuild {0};
	/// The circle that a walk heads for or goes round. Only a walk that goes straight, goes round a circle or goes out
	/// of a circle has one.
	std::optional<ImportedCircle> circle;
};

/// The walk of a record of a walker of the file: a villager or an animal
[[nodiscard]] ImportedWalk ReadWalk(const gamesave::VanillaWorld& world, const gamesave::VanillaRecord& record);

/// A speed in metres a second that gives the whole map units a turn of the file again
[[nodiscard]] float WalkSpeedOf(uint16_t wholeSpeed);

/// The number of players, of kinds of reaction and of things that a town wants, as the file keeps them
inline constexpr size_t k_ImportedPlayers = 8;
inline constexpr size_t k_ImportedReactions = 41;
inline constexpr size_t k_ImportedDesires = 17;

/// The belief of a town in the file
struct ImportedBelief
{
	/// What the town believes in each player, what it was given lately, the most it believes in each player and what
	/// it will believe at its next turn
	std::array<float, k_ImportedPlayers> belief {};
	std::array<float, k_ImportedPlayers> recent {};
	std::array<float, k_ImportedPlayers> cap {};
	std::array<float, k_ImportedPlayers> pending {};
	/// What the town gained in each player since the hand last told the player
	std::array<float, k_ImportedPlayers> gainedSinceShown {};
	/// How weary the town is of each kind of reaction, 1 when it has never seen one
	std::array<float, k_ImportedReactions> boredom {};
	/// For each thing that the town wants, the level above which the want costs its owner belief
	std::array<float, k_ImportedDesires> desireLevel {};
};

/// One place in the order of the desires of a town in the file
struct ImportedDesireSort
{
	float boosts {0.0f};
	float value {0.0f};
	uint32_t index {0};
};

/// What a town wants in the file
struct ImportedDesire
{
	std::array<float, k_ImportedDesires> cheat {};
	std::array<float, k_ImportedDesires> boost {};
	std::array<float, k_ImportedDesires> desire {};
	float population {0.0f};
	std::array<float, k_ImportedDesires> raw {};
	std::array<ImportedDesireSort, k_ImportedDesires> sorted {};
	std::array<ImportedDesireSort, k_ImportedDesires> sortedRaw {};
	std::array<uint32_t, k_ImportedDesires> turnsOverAlignment {};
	std::array<float, k_ImportedDesires> doingAtStart {};
	std::array<float, k_ImportedDesires> doingCountAtStart {};
	std::array<float, k_ImportedDesires> doing {};
	std::array<float, k_ImportedDesires> doingCount {};
};

/// The running counts of the food and the wood of a town in the file
struct ImportedTally
{
	float foodUsed {0.0f};
	float woodUsed {0.0f};
	float foodCarried {0.0f};
	float woodCarried {0.0f};
	float woodAtSites {0.0f};
};

struct ImportedTown
{
	/// The number of the object in the file
	uint32_t number {0};
	/// The number that the land's script gives the town
	uint32_t id {0};
	Tribe tribe {Tribe::NONE};
	PlayerNames owner {PlayerNames::NEUTRAL};
	gamesave::MapCoords position {};
	bool cannotBuildWorshipSite {false};
	bool uninhabitable {false};
	uint32_t emptyTownTimer {0};
	/// The share of its people that it wants at worship
	float worshipShare {0.0f};
	/// What it believes in the neutral player, and what its pending belief is multiplied by
	float beliefInNeutral {0.0f};
	float beliefScale {1.0f};
	std::optional<ImportedBelief> belief;
	std::optional<ImportedDesire> desire;
	std::optional<ImportedTally> tally;
	/// The numbers of the objects of its buildings and of its homeless villagers, in the order of the file
	std::vector<uint32_t> abodes;
	std::vector<uint32_t> homeless;
	/// The numbers of the objects of its plans and of its building sites, in the order of the file: the newest site first
	std::vector<uint32_t> planned;
	std::vector<uint32_t> sites;
};

/// A pile of food or wood of a storage pit or of a building site in the file
struct ImportedPile
{
	uint32_t number {0};
	/// Its row of the table of pots, which says which pile of the storage pit it is
	PotInfo row {PotInfo::StoragePitFoodPile};
	gamesave::MapCoords position {};
	float heading {0.0f};
	uint32_t amount {0};
	bool poisoned {false};
	/// How it rises out of the ground or sinks into it
	magic::piles::Rise rise {};
};

/// The number of bytes of the rise of a pile in the file
inline constexpr size_t k_PileRiseBytes = 48;

/// The rise of a pile of the file: its offset, its target, the speed of the target, its speed, a value that nothing
/// reads, the time that went by, the time of the rise, its start and the speed at the start, then the three factors of
/// its curve. The speed of the target is always 0 for a pile.
[[nodiscard]] magic::piles::Rise PileRiseOf(std::span<const uint8_t, k_PileRiseBytes> bytes);

/// The values of a workshop of the file
struct ImportedWorkshop
{
	/// The game turns until the scaffold that the workshop makes is finished, 0 when it makes none
	int32_t makingTurns {0};
	/// The scaffolds that the workshop counts as its own
	uint32_t counted {0};
	/// What each of the three places of its yard holds: 0 free, 1 away in the hand, 2 holds its scaffold
	std::array<uint8_t, 3> places {};
	/// The numbers of the objects of its scaffolds in the file, the newest first
	std::vector<uint32_t> scaffolds;
	/// The number of the object of its wood pile in the file, 0 for none
	uint32_t woodPile {0};
};

/// The values that the flags of a scaffold give
struct ImportedScaffoldFlags
{
	/// The phantom grows, or has its full size
	bool grows {false};
	/// The scaffold looked for an offer since the hand last moved
	bool looked {false};
	/// The game turns since the scaffold last showed that it has no offer
	uint8_t noOfferTurns {0};
	/// Its place in the yard of its workshop, 0 to 3
	uint8_t place {0};
	/// The offer is a village centre that starts a new town
	bool foundsTown {false};
	/// The phantom does not turn
	bool fixedAngle {false};
	/// The hand let the scaffold go onto the land, not by force
	bool gentle {false};
	/// A script marked the scaffold to destroy the things in its way
	bool destroysThingsInWay {false};
};

/// The values of the flags of a scaffold of the file
[[nodiscard]] constexpr ImportedScaffoldFlags ScaffoldFlagsOf(uint16_t flags)
{
	return {.grows = (flags & 0x0001U) != 0,
	        .looked = (flags & 0x0002U) != 0,
	        .noOfferTurns = static_cast<uint8_t>((flags >> 2U) & 0x0FU),
	        .place = static_cast<uint8_t>((flags >> 6U) & 0x03U),
	        .foundsTown = (flags & 0x0100U) != 0,
	        .fixedAngle = (flags & 0x0200U) != 0,
	        .gentle = (flags & 0x0400U) != 0,
	        .destroysThingsInWay = (flags & 0x0800U) != 0};
}

/// The game turn that the load gives each scaffold as the turn that a hand last put it down. The file does not keep
/// that turn.
inline constexpr uint32_t k_ScaffoldPutDownTurnAfterLoad = 1;

/// A building that the phantom of a scaffold of the file offers
struct ImportedScaffoldOffer
{
	AbodeInfo row {AbodeInfo::None};
	/// The turn of the building about the up axis, in radians
	float heading {0.0f};
	float scale {1.0f};
};

/// A scaffold of the file
struct ImportedScaffold
{
	uint32_t number {0};
	gamesave::MapCoords position {};
	float heading {0.0f};
	/// Its size, which gives how many scaffolds it is worth
	float scale {1.0f};
	/// The numbers of the objects of its town and of the building of its site in the file, 0 for none
	uint32_t town {0};
	uint32_t site {0};
	/// The building that its phantom shows, and the building that shows next, nothing for none
	std::optional<ImportedScaffoldOffer> offer;
	std::optional<ImportedScaffoldOffer> nextOffer;
	/// The milliseconds until the phantom has its full size, or until it is small again
	int32_t changeMs {0};
	/// The player whose hand holds the scaffold, or let it go last until it came to rest, nothing for none
	std::optional<PlayerNames> handPlayer;
	/// The turn of the phantom about the up axis, in radians, and the size of the phantom at its full size
	float phantomAngle {0.0f};
	float phantomScale {0.0f};
	ImportedScaffoldFlags flags {};
	/// The abode number of the building that a script gave the scaffold, nothing when no script gave one
	std::optional<AbodeNumber> scriptBuilding;
};

struct ImportedAbode
{
	uint32_t number {0};
	AbodeInfo row {AbodeInfo::None};
	/// The number of the object of its town in the file, 0 for no town
	uint32_t town {0};
	gamesave::MapCoords position {};
	float heading {0.0f};
	float scale {1.0f};
	float life {1.0f};
	/// How much of the building stands, from 0 to 1
	float built {1.0f};
	uint32_t food {0};
	uint32_t wood {0};
	/// The numbers of the objects of its villagers in the file
	std::vector<uint32_t> villagers;
	/// The number of its villagers that are in it
	uint8_t present {0};
	/// The power of a wonder, nothing for another building
	std::optional<float> wonderPower;
	/// The piles of a storage pit or the wood pile of a workshop, in the order of the file
	std::vector<ImportedPile> piles;
	/// The values of a workshop, nothing for another building
	std::optional<ImportedWorkshop> workshop;
};

/// The totem of a town centre
struct ImportedTotem
{
	uint32_t number {0};
	/// The number of the object of its town centre in the file
	uint32_t townCentre {0};
	gamesave::MapCoords position {};
	float heading {0.0f};
	float scale {1.0f};
	float life {1.0f};
	/// The share of the town at worship. The load puts the totem at this share, still.
	float share {0.0f};
	/// The share that the hand or a god held the totem at. The creation step after the load sets it to the share again.
	float heldShare {0.0f};
	/// A word that only the save and the load use
	uint32_t word84 {0};
};

/// The power that each tribe gives a player, by tribe number
struct ImportedTribalPower
{
	PlayerNames player {PlayerNames::PLAYER_ONE};
	std::array<float, static_cast<size_t>(Tribe::_COUNT)> power {};
	/// The most power that each tribe gave the player
	std::array<float, static_cast<size_t>(Tribe::_COUNT)> most {};
};

struct ImportedVillager
{
	uint32_t number {0};
	VillagerInfo row {VillagerInfo::None};
	/// The row of its name in the table of named villagers, for a named villager
	std::optional<uint8_t> nameRow;
	gamesave::MapCoords position {};
	float heading {0.0f};
	float scale {1.0f};
	float life {1.0f};
	/// The place of the villager in the order of all things that the game made
	uint32_t made {0};
	uint32_t birthTurn {0};
	float food {0.0f};
	uint32_t lastCheckTurn {0};
	uint8_t foodSpeedUp {0};
	int16_t foodHeld {0};
	int16_t woodHeld {0};
	int16_t pregnancy {0};
	/// The numbers of the objects of its abode, its town and its mother in the file, 0 for none
	uint32_t home {0};
	uint32_t town {0};
	uint32_t mother {0};
	/// The three states of the file: the top state, the final state and the state before
	std::array<uint8_t, 3> states {};
	uint8_t moveState {0};
	ImportedWalk walk;
	/// The footpath that it walks, the node that it walks to, the way and the footpath after it
	vanilla_footpath_import::FileWalk footpathWalk;
	/// The values of the top state and of the final state
	std::vector<ImportedStateValue> topValues;
	std::vector<ImportedStateValue> finalValues;
	/// The disciple type, the villager flags and the living status of the file
	uint8_t discipleType {0};
	uint16_t villagerFlags {0};
	uint16_t status {0};
	uint16_t turnsSinceStateChange {0};
	uint16_t turnsUntilStateChange {0};
};

/// A building that a town plans to build later
struct ImportedPlan
{
	uint32_t number {0};
	AbodeInfo row {AbodeInfo::None};
	/// The number of the object of its town in the file
	uint32_t town {0};
	gamesave::MapCoords position {};
	float heading {0.0f};
	float scale {1.0f};
	/// A building that the town had built and that went from the land left the plan
	bool wasBuilt {false};
};

/// The number of places round a building where its builders stand
inline constexpr size_t k_ImportedSitePlaces = 128;
/// The game object of a file keeps this many lists of objects
inline constexpr size_t k_GameObjectLists = 44;
/// The place of the game's list of all the towns among the lists of the game object
inline constexpr size_t k_GameTownList = 31;

/// The site of a town at a building that goes up or that its builders mend
struct ImportedSite
{
	uint32_t number {0};
	/// The numbers of the objects of its building and of its wood pile in the file, 0 for none
	uint32_t building {0};
	uint32_t woodPile {0};
	/// The numbers of the objects of its builders in the file, a builder once each time that it started work
	std::vector<uint32_t> workers;
	int32_t workerCount {0};
	/// The desire for builders that a script forced on the site
	float desire {0.0f};
	std::array<glm::vec3, k_ImportedSitePlaces> places {};
	/// The site of a temple: it keeps its wood in six piles, which these numbers give (0 for a pile that has gone)
	bool temple {false};
	std::vector<uint32_t> piles;
	/// The values of its wood pile, and of each pile of the site of a temple in the order of the piles, nothing for a
	/// pile that the file does not have
	std::optional<ImportedPile> woodPileValues;
	std::vector<std::optional<ImportedPile>> pileValues;
};

/// What the import does with a pile of the site of a temple
enum class SitePileLoad
{
	/// The file has no pile in this place, so the pile of the site goes
	Remove,
	/// The pile takes the values of the pile of the file
	TakeValues,
	/// The file has a pile that the import cannot read, so the pile of the site stays as it is
	Keep,
};

/// A pile of the site of a temple after a load, from the number of the pile in the file (0 for no pile) and whether
/// the import can read the values of that pile. The load gives the site only the piles of the file. The site makes its
/// piles again only when wood comes and it has no pile.
[[nodiscard]] constexpr SitePileLoad SitePileAfterLoad(uint32_t fileNumber, bool hasValues)
{
	if (fileNumber == 0)
	{
		return SitePileLoad::Remove;
	}
	return hasValues ? SitePileLoad::TakeValues : SitePileLoad::Keep;
}

/// A town that has a building site of the file in its list of sites
struct SiteListing
{
	/// The town, by its place among the towns of the file
	std::size_t town {0};
	/// The number of the object of the site in the file
	uint32_t site {0};
	/// The town that opens the site, by its place among the towns of the file. The other towns that list the site get
	/// the same site from this town.
	std::size_t opener {0};
};

/// The places of the sites of the file in the lists of the towns. The listings come in the order of the towns, and each
/// town's list comes with its oldest site first, which is the order that numbers them. A site in the lists of two or
/// more towns is one site. The town of its building opens it if that town lists it, else the first town that lists it.
/// ownerOf gives the town of the building of a site by its place among the towns, none if there is no such town.
[[nodiscard]] std::vector<SiteListing> PlanSiteListings(std::span<const ImportedTown> towns,
                                                        const std::function<std::optional<std::size_t>(uint32_t)>& ownerOf);

/// The flag of a desire of a town: it flies as high as the town wants the thing
struct ImportedDesireFlag
{
	/// The number of the object of its town in the file
	uint32_t town {0};
	/// The desire, by its row of the table of the desires of a town
	uint32_t desire {0};
	/// How high it flies, from 0 to 1
	float level {0.0f};
};

/// The altar of a worship site of the file
struct ImportedAltar
{
	uint32_t number {0};
	/// How much of the altar is built, and its life
	float built {0.0f};
	float life {1.0f};
};

/// The prayer power of a worship site of the file
struct ImportedChants
{
	float battery {0.0f};
	/// All the prayer power that sacrifices at the altar gave
	float sacrificed {0.0f};
	/// What the site can give this turn, what it gave and what was asked for
	float available {0.0f};
	float used {0.0f};
	float requested {0.0f};
	/// What each dancer chanted last turn
	float chantsPerDancer {0.0f};
	/// How far the requests are more than the dancers can give
	float strain {0.0f};
	/// The two cheats: prayer power with no end, and miracles that cost nothing to keep
	bool infinite {false};
	bool freeMaintenance {false};
};

/// A dance of the file. The groups of the dance and their dancers are not in these values.
struct ImportedDance
{
	uint32_t number {0};
	/// The row of the table of the dances
	DanceInfo type {DanceInfo::None};
	/// The clock of the dance and how many lengths of the clock it takes before it starts again
	float clock {0.0f};
	uint32_t loopLength {1};
	/// The speed that the dance was asked to go at, the rate of its groups, and the rate at which it last started to
	/// dance
	float speed {0.0f};
	float rate {1.0f};
	float dancingRate {1.0f};
	float angle {0.0f};
	bool stopped {true};
	/// How many turns it lasts once started, 0 for as long as it is wanted, and the turn when it last started
	uint32_t duration {0};
	uint32_t startTurn {0};
	/// The turn when its first dancer came, nothing before the first dancer came
	std::optional<uint32_t> firstDancerTurn;
	/// The count of its dancers and of the dancers on their way
	uint32_t dancers {0};
	uint32_t onTheirWay {0};
	/// How many turns in a row the hand of the player was near it, whether the hand stayed near long enough, and whether
	/// the dance made its camera
	uint32_t handTurns {0};
	bool cameraWanted {false};
	bool cameraMade {false};
};

/// A spell icon of a worship site of the file
struct ImportedWorshipIcon
{
	uint32_t number {0};
	SpellSeedType seed {SpellSeedType::None};
	/// Its place at its site
	int16_t slot {0};
	/// It charges its miracle, at a power-up level, for the hand of a player, since a turn
	bool charging {false};
	int32_t powerUp {-1};
	std::optional<PlayerNames> charger;
	uint32_t chargeStartTurn {0};
	/// The prayer power that it stored
	float store {0.0f};
	/// The numbers of the seeds made from it that are still out, in the order of the file
	std::vector<uint32_t> seedsOut;
	/// The turns until it goes. Nothing in the game starts this countdown, so it is always 0.
	uint16_t removeTurns {0};
};

/// The number of need flags of a worship site: food, rest and hiding
constexpr size_t k_SiteNeedFlags = 3;

/// A worship site of the file
struct ImportedWorshipSite
{
	uint32_t number {0};
	/// The number of the object of its temple in the file
	uint32_t citadel {0};
	Tribe tribe {Tribe::NORSE};
	/// Its place round the temple, 0 to 5
	uint8_t place {0};
	/// How much of it is built
	float built {0.0f};
	/// The numbers of the objects of its towns, of its dance, of its food pot and of its spell icons in the file, each
	/// list in the order of the file
	std::vector<uint32_t> towns;
	uint32_t dance {0};
	uint32_t foodPot {0};
	std::vector<uint32_t> icons;
	/// The villagers at worship in the order of the file, and the count of the site. The game changes the count also
	/// when it removes a villager that is not in the list, so the two can be different.
	std::vector<uint32_t> worshippers;
	uint32_t worshipCount {0};
	/// The food in its pot. Nothing when the file has no pot or no amount for it.
	std::optional<uint32_t> food;
	/// Food on its way from its towns
	int32_t foodOnTheWay {0};
	ImportedChants chants;
	/// What its need flags show: food, rest and hiding. Nothing when the file has no need flags for the site.
	std::optional<std::array<float, k_SiteNeedFlags>> needs;
	/// Its altar. Nothing when the file has no altar for the site.
	std::optional<ImportedAltar> altar;
	/// The number of its footpath link in the file, 0 for none
	uint32_t footpathLink {0};
	/// The count of the villagers that ask to go home. No villager of this version of the game asks.
	uint32_t goHome {0};
	/// The values of its dance. Nothing when the file has no dance for the site.
	std::optional<ImportedDance> danceValues;
	/// The values of its spell icons, in the order of the file
	std::vector<ImportedWorshipIcon> iconValues;
};

/// The towns, buildings and villagers of a world file, in the order of the file
struct ImportedTowns
{
	uint32_t turn {0};
	std::vector<ImportedTown> towns;
	std::vector<ImportedAbode> abodes;
	/// The totems of the town centres
	std::vector<ImportedTotem> totems;
	/// The scaffolds, in the order of the file
	std::vector<ImportedScaffold> scaffolds;
	std::vector<ImportedVillager> villagers;
	/// The plans and the building sites of the towns. The lists of each town give their order.
	std::vector<ImportedPlan> plans;
	std::vector<ImportedSite> sites;
	/// The numbers of the objects of the towns in the order of the town lists of the players: the list of each player of
	/// the file in turn, each list in the order that its player got the towns
	std::vector<uint32_t> townLists;
	/// The numbers of the objects of the towns in the game's list of all the towns, as the game object of the file keeps
	/// it: the town that the game made last comes first
	std::vector<uint32_t> gameTownList;
	/// The tribal powers of each player of the file that keeps a value for each tribe
	std::vector<ImportedTribalPower> tribalPowers;
	/// The flags of the desires of the towns
	std::vector<ImportedDesireFlag> desireFlags;
	/// The worship sites, in the order of the file
	std::vector<ImportedWorshipSite> worshipSites;
	/// Each record that the import does not take, and why
	std::vector<std::string> skipped;
};

/// The towns, the buildings of the towns and the villagers of the world. A record that has a row of another table, or
/// no value that the import needs, goes into the list of records that the import does not take.
[[nodiscard]] ImportedTowns ReadTowns(const gamesave::VanillaWorld& world);

/// The values of a dance record of the file. Nothing for a record that is not a dance or has a row of another table.
[[nodiscard]] std::optional<ImportedDance> ReadDance(const gamesave::VanillaWorld& world,
                                                     const gamesave::VanillaRecord& record);
/// A dance takes the values of a dance of the file: its clock, its speed and rates, its state, its duration, its start,
/// the turn of its first dancer and the hand of the player. The hand of a game with one player is the hand of the first
/// god slot. The dancers stay as the dance has them, as only the villagers that keep their states can dance.
void PutDanceValues(components::Dance& dance, const ImportedDance& file);
/// An icon of a worship site takes the charge and the store of an icon of the file. The seeds out from the icon are not
/// in these values.
void PutIconValues(components::WorshipIcon& icon, const ImportedWorshipIcon& file);

/// The values of a spell icon of a worship site of the file. Nothing for a record that is not such an icon or has no
/// spell seed of the table.
[[nodiscard]] std::optional<ImportedWorshipIcon> ReadWorshipIcon(const gamesave::VanillaWorld& world,
                                                                 const gamesave::VanillaRecord& record);

} // namespace openblack::ecs::vanilla_world_import
