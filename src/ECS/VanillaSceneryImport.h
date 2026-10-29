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
#include <string>
#include <vector>

#include "CreatureRecord.h"
#include "Enums.h"

namespace openblack::gamesave
{
class VanillaWorld;
} // namespace openblack::gamesave

// The trees, the forests and the big forests of a world file of the game, as values that openblack can make again.
// The rules here use no service of the game, so that tests can give them a world that they make.
namespace openblack::ecs::vanilla_world_import
{

/// The number of the first row of each table that the import reads. The game gives the last row that it loads the
/// number 0, and each row before it the number of the next row plus 1. Thus the number of a row is the number of the
/// first row of its table less its index.
///
/// The tables that the game loads after the table of buildings: the villagers (84 rows), the named villagers (48), the
/// trees (23), four small tables (11 rows in all), the towns (1), the jobs (16), the features (76), the flowers (2),
/// the animated things (16), the mobile objects (32), the scaffolds (1), the static things (61) and the pots (19).
constexpr uint32_t k_FirstTreeRowNumber = 2115;
constexpr uint32_t k_FirstFeatureRowNumber = 2064;
constexpr uint32_t k_FirstAnimatedStaticRowNumber = 1986;
constexpr uint32_t k_FirstMobileObjectRowNumber = 1970;
constexpr uint32_t k_FirstMobileStaticRowNumber = 1937;
constexpr uint32_t k_FirstPotRowNumber = 1876;
/// The kinds of field come after the pots and 12 more tables, then the fish farms
constexpr uint32_t k_FirstFieldTypeRowNumber = 1765;
constexpr uint32_t k_FirstFishFarmRowNumber = 1759;
/// The game loads the animals before the creatures and the temple, and these before the buildings
constexpr uint32_t k_FirstAnimalRowNumber = 2548;
/// The game loads 13 tables with 366 rows in all after the big forests. The totems come right before the big forests.
constexpr uint32_t k_FirstBigForestRowNumber = 369;
constexpr uint32_t k_FirstTotemStatueRowNumber = 378;

[[nodiscard]] std::optional<TreeInfo> TreeRowOf(uint32_t number);
[[nodiscard]] std::optional<FeatureInfo> FeatureRowOf(uint32_t number);
[[nodiscard]] std::optional<AnimatedStaticInfo> AnimatedStaticRowOf(uint32_t number);
[[nodiscard]] std::optional<MobileObjectInfo> MobileObjectRowOf(uint32_t number);
[[nodiscard]] std::optional<MobileStaticInfo> MobileStaticRowOf(uint32_t number);
[[nodiscard]] std::optional<PotInfo> PotRowOf(uint32_t number);
[[nodiscard]] std::optional<FieldTypeInfo> FieldTypeRowOf(uint32_t number);
[[nodiscard]] std::optional<AnimalInfo> AnimalRowOf(uint32_t number);
[[nodiscard]] std::optional<BigForestInfo> BigForestRowOf(uint32_t number);
/// The row of a totem in its table of nine rows
[[nodiscard]] std::optional<uint32_t> TotemStatueRowOf(uint32_t number);

struct ImportedTree
{
	/// The number of the object in the file
	uint32_t number {0};
	TreeInfo row {TreeInfo::Beech};
	gamesave::MapCoords position {};
	float heading {0.0f};
	float scale {1.0f};
	float life {1.0f};
	/// The largest size that it grows to
	float maxSize {1.0f};
	uint16_t growthCountdown {0};
};

struct ImportedBigForest
{
	uint32_t number {0};
	BigForestInfo row {BigForestInfo::Info1};
	gamesave::MapCoords position {};
	float heading {0.0f};
	float scale {1.0f};
	float life {1.0f};
	float wood {0.0f};
};

struct ImportedForest
{
	uint32_t number {0};
	/// The number that the land's script gives the forest
	uint32_t id {0};
	gamesave::MapCoords position {};
	/// A forest of the lone trees about a town, which never grows
	bool scenic {false};
	/// The number of the object of its big forest in the file, 0 for none
	uint32_t bigForest {0};
	/// The numbers of the objects of its trees: first the grown trees, then the trees that grow, each in the order of
	/// the file
	std::vector<uint32_t> grown;
	std::vector<uint32_t> growing;
};

/// The forests of a world file, in the order that the game meets them, and their trees and big forests
struct ImportedForests
{
	std::vector<ImportedForest> forests;
	std::vector<ImportedBigForest> bigForests;
	std::vector<ImportedTree> trees;
	/// The turn that a forest last got a tree that grew near another tree
	uint32_t lastTreeAddedTurn {0};
	/// The number that the next forest without a number gets
	uint32_t nextForestId {1};
	/// Each record that the import does not take, and why
	std::vector<std::string> skipped;
};

/// The forests, the big forests and the trees of the world. The game meets the forests in the order of its list of
/// forests, which the game object keeps. A forest that is not in the list comes after the forests of the list.
[[nodiscard]] ImportedForests ReadForests(const gamesave::VanillaWorld& world);

/// A field of a town and its crop
struct ImportedField
{
	uint32_t number {0};
	/// The building row of the field, which gives its tribe
	AbodeInfo row {AbodeInfo::None};
	/// The kind of crop that grows in it
	FieldTypeInfo type {FieldTypeInfo::Wheat};
	/// The number of the object of its town in the file
	uint32_t town {0};
	gamesave::MapCoords position {};
	float heading {0.0f};
	float scale {1.0f};
	float life {1.0f};
	float built {1.0f};
	uint8_t timesSown {0};
	float age {0.0f};
	float food {0.0f};
	/// Which of every ten turns the crop grows on
	uint32_t growthTurn {0};
	/// The numbers of the objects of the villagers that farm it, in the order of the file
	std::vector<uint32_t> farmers;
};

/// A stretch of sea that a town fishes
struct ImportedFishFarm
{
	uint32_t number {0};
	gamesave::MapCoords position {};
	/// The number of the object of its town in the file, 0 for no town
	uint32_t town {0};
	float fish {0.0f};
	/// The numbers of the objects of the villagers that fish it, in the order of the file
	std::vector<uint32_t> fishermen;
};

/// The fields and the fish farms of a world file, in the order of the file
struct ImportedFarms
{
	std::vector<ImportedField> fields;
	std::vector<ImportedFishFarm> fishFarms;
	std::vector<std::string> skipped;
};

/// The fields and the fish farms of the world. A field without a row of the tables goes into the skipped list.
[[nodiscard]] ImportedFarms ReadFarms(const gamesave::VanillaWorld& world);

/// The kinds of still things and loose things that the import makes from the file
enum class StaticKind : uint8_t
{
	/// A thing of the table of static things: a rock, a stone, a post or a lantern
	MobileStatic,
	/// A thing of the table of static things that is a rock to the scripts, or a bonfire
	Rock,
	/// A thing of the table of static things that shows only as a print on the land, as the holder of a singing stone
	BaseOnly,
	/// A lantern of a town or of the country
	StreetLantern,
	/// A tree that came out of the ground
	DeadTree,
	Feature,
	/// A gate or another thing that a script opens and closes
	AnimatedStatic,
	/// A thing of the table of loose things, as a log, food or a lump of poo
	MobileObject,
};

/// A still thing or a loose thing of the file
struct ImportedStatic
{
	uint32_t number {0};
	StaticKind kind {StaticKind::MobileStatic};
	/// Its row of its table: the static things for a mobile static, a rock and a lantern, the trees for a dead tree, the
	/// features, the animated things or the loose things
	uint32_t row {0};
	gamesave::MapCoords position {};
	float heading {0.0f};
	float scale {1.0f};
	float life {1.0f};
	/// How it leans, in radians about x and about z (a mobile static, a rock, a dead tree or a loose thing)
	float xAngle {0.0f};
	float zAngle {0.0f};
	/// What the wood of a dead tree is worth against that of a tree
	float woodMultiplier {1.0f};
};

struct ImportedStatics
{
	/// In the order of the file
	std::vector<ImportedStatic> statics;
	std::vector<std::string> skipped;
};

/// The still things and the loose things of the world. A thing without a row of its table goes into the skipped list.
[[nodiscard]] ImportedStatics ReadStatics(const gamesave::VanillaWorld& world);

/// An animal of the file: a bird or a grazer of the land, or an animal of a temple
struct ImportedAnimal
{
	uint32_t number {0};
	AnimalInfo row {AnimalInfo::None};
	gamesave::MapCoords position {};
	float scale {1.0f};
	float life {1.0f};
	uint32_t birthTurn {0};
	/// Where it goes, in whole map units, the way it faces in game angles and its whole map units a turn
	gamesave::MapCoords goal {};
	uint16_t angle {0};
	uint16_t speed {0};
	/// The step of a turn in whole map units, and the state of its move by its number in the game's table of the move
	/// states
	int32_t stepX {0};
	int32_t stepZ {0};
	uint8_t moveState {0};
	/// Its state and the state that it goes into next, by their numbers in the game's table of the animal states
	uint8_t state {0};
	uint8_t finalState {0};
	/// The numbers of its flock and of the town that it belongs to in the file, 0 for none
	uint32_t flock {0};
	uint32_t town {0};
};

/// A flock of the file
struct ImportedFlock
{
	uint32_t number {0};
	/// Its place, and the place that it comes back to
	gamesave::MapCoords place {};
	gamesave::MapCoords savedPlace {};
	/// How far from its place its leader goes, and its followers from its leader, in whole metres
	uint16_t domainRadius {0};
	uint16_t flockDistance {0};
	/// The numbers of the villager that herds it, of the temple heart that it flies round and of its town, 0 for none
	uint32_t shepherd {0};
	uint32_t heart {0};
	uint32_t town {0};
	/// The numbers of its members in the order of its line. The last is its leader.
	std::vector<uint32_t> members;
};

struct ImportedAnimals
{
	/// In the order of the file
	std::vector<ImportedAnimal> animals;
	std::vector<ImportedFlock> flocks;
	std::vector<std::string> skipped;
};

/// The animals and the flocks of the world. An animal without a row of the table of the animals goes into the skipped
/// list.
[[nodiscard]] ImportedAnimals ReadAnimals(const gamesave::VanillaWorld& world);

/// The clock of day and night of a file
struct ImportedDayCycle
{
	/// The time of day that the game shows, in hours from 0 to 24
	float visualTime {12.0f};
	/// The seconds of game time in a day
	float duration {1700.0f};
	/// The parts of the half day that are night and that change from night to day
	float night {0.083f};
	float change {0.07f};
	/// The rate that the clock runs at, 0 when it stands
	float scale {1.0f};
};

/// The clock of day and night of the file, nothing when the game object does not keep it
[[nodiscard]] std::optional<ImportedDayCycle> ReadDayCycle(const gamesave::VanillaWorld& world);

} // namespace openblack::ecs::vanilla_world_import
