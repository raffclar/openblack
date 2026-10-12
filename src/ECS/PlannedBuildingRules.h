/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <span>

#include <glm/vec2.hpp>

#include "Enums.h"

/// The rules by which a town picks one of the buildings it planned and starts it going up: how much it wants each
/// planned building built, which one it picks, and whether a new house may stand where it was planned. Free of the
/// game's state so they can be tested on their own.
namespace openblack::ecs::planned_building
{

/// Football pitches are a hidden feature that a normal install never unlocks, so a town never wants one
inline constexpr bool k_FootballEnabled = false;

/// What a town has that its wants for planned buildings read
struct TownForPlans
{
	/// The neutral player's town wants neither a totem nor a workshop
	bool neutral {false};
	/// The places left for adults in its built houses (below nothing when they are over full), its homeless, and its
	/// people
	int32_t adultRoom {0};
	uint32_t homeless {0};
	uint32_t adults {0};
	uint32_t children {0};
	/// Its built buildings of each abode number
	std::array<uint8_t, static_cast<size_t>(AbodeNumber::_COUNT)> built {};
	/// How much it wants to build a wonder, and the want at which its villagers set about it
	float wonderDesire {0.0f};
	float wonderTrigger {0.0f};
};

/// A building a town planned, as its want of it reads it
struct PlannedKind
{
	AbodeType type {AbodeType::General};
	AbodeNumber number {AbodeNumber::Invalid};
	/// How much a town wants a building of this kind built, from the kind's info
	float desireToBeBuilt {0.0f};
	/// The most adults that live in one
	uint32_t maxAdults {0};
	/// The town's building sites whose building is of the same type, mending ones included
	uint32_t sitesOfSameType {0};
};

/// How much a town wants one of its planned buildings built, 0 for not at all. A house is wanted less the more room its
/// town has for adults beyond a tenth of its people, more the further short of room it is, and less the more houses of
/// its kind stand. A civic building is wanted while the town has none of it (a crèche, a storage pit, a graveyard, a
/// village centre or totem), half as much with a workshop already, and a spell dispenser only once all the others
/// stand. A wonder is wanted by how much the town wants one. Every want is shared out over the sites of its type.
[[nodiscard]] float DesireToBeBuilt(const PlannedKind& plan, const TownForPlans& town);

/// One of a town's planned buildings, in the order the town planned them
struct PlannedCandidate
{
	AbodeType type {AbodeType::General};
	float desire {0.0f};
};

/// The planned building of a wanted type (living quarters, or civic) a town wants built most, none if it wants none at
/// all; the first planned on a tie
[[nodiscard]] std::optional<size_t> BestPlanned(std::span<const PlannedCandidate> planned, AbodeType wanted);

/// Where another town covers the land: the cell of its place and, if it has any buildings or fields, the cells of the
/// box about them, each widened by its radius
struct OtherTown
{
	glm::ivec2 cell {0};
	std::optional<glm::ivec2> areaMin;
	std::optional<glm::ivec2> areaMax;
};
/// The towns are measured cell by cell, as the crow flies with the diagonal at half again (the longer way plus half the
/// shorter); this far out the box counts
inline constexpr int32_t k_AreaMargin = 4;
inline constexpr uint32_t k_FarthestTown = 10000000;

/// Whether a cell lies within the box (widened by four cells) of the nearest of the other towns. Such a place passes a
/// new house's placement check at once: the game reads the town it found as a pass.
[[nodiscard]] bool WithinNearestOtherTown(glm::ivec2 cell, std::span<const OtherTown> others);

/// A building of the map in a cell of a new house's footprint: how far its place is from the house's, and its radius
/// across the ground
struct BuildingNearby
{
	float distance {0.0f};
	float radius {0.0f};
};

/// One cell of a new house's footprint, with the buildings filed in it
struct FootprintCell
{
	std::span<const BuildingNearby> buildings;
};

/// Whether a planned house may go up where it was planned: for each cell of its footprint in turn, the land at its own
/// place must be dry, and no building filed in the cell may stand nearer it than their radii together (the house's
/// being the larger half side of its model's box). A footprint with no cell passes.
[[nodiscard]] bool IsClearToBuild(std::span<const FootprintCell> footprint, bool dryLandAtPlace, float reach);

} // namespace openblack::ecs::planned_building
