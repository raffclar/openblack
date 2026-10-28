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

} // namespace openblack::ecs::vanilla_world_import
