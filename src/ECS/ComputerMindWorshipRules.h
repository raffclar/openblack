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

#include "3D/MapCoords.h"
#include "Enums.h"

/// The maths of the computer god's worship and building rules: how tired its worshippers are, which disciple a town
/// wants, how much a building site wants workers and wood, how good a free builder is, and where the hand puts a
/// villager down. Each formula keeps the game's order of float operations.
namespace openblack::computer_mind_rules
{

/// A site with a rest desire below this has no need of rest
constexpr float k_LeastRestDesire = 0.2f;
/// The god takes a tired worshipper home only from a site with a rest desire above this
constexpr float k_RestDesireToTakeHome = 0.25f;
/// A worshipper more tired than this makes the god want to take it home with all its heart
constexpr double k_TiredWorshipper = 0.75;
/// A town with fewer breeders than this wants a new one first
constexpr float k_FewestBreeders = 2.0f;
/// The cells round its hand that the god looks at for a free villager, in a spiral out from the hand
constexpr int32_t k_FreeVillagerCells = 36;

/// How much a villager wants life: what it lacks of full life
[[nodiscard]] float LifeDesire(float life);
/// How much the dancers of a site want rest: the mean of what each wants of life, at most 1. A dancer that is not a
/// villager wants none, but counts.
[[nodiscard]] float DancersRestDesire(std::span<const float> lifeDesires);
/// A site's rest desire as the god counts it: none below the least
[[nodiscard]] float RestDesireCounted(float restDesire);
/// How much the god wants to rest its worshippers, from the most rest that a site wants: twice that, at most 1
[[nodiscard]] float WorshippersRestUrge(float mostRestDesire);
/// How much the god wants to take the most tired worshipper home, from what it lacks of life: all above the mark
[[nodiscard]] float TiredWorshipperUrge(float mostLifeDesire);

/// The desire of a town that a disciple of each work serves, none for a work that serves none
[[nodiscard]] std::optional<TownDesireInfo> DesireServedBy(VillagerDisciple work);

/// The work that a town wants a new disciple for, and how much
struct DiscipleChoice
{
	VillagerDisciple work {VillagerDisciple::None};
	float value {0.0f};
};
/// The work of the most wanted new disciple of a town: each work's desire, less the share of disciples that it has
/// and the share of the adults free of worship that are disciples already. None when no work is wanted.
/// - `disciples`: the town's disciples of each work.
/// - `adults`: the town's adults.
/// - `worshipShare`: the share of the town wanted at worship, 0 without a totem.
/// - `desires`: the town's desires, each with its boosts.
[[nodiscard]] std::optional<DiscipleChoice> MostWantedDisciple(const std::array<uint8_t, 13>& disciples, uint32_t adults,
                                                               float worshipShare, std::span<const float> desires);

/// How much the god wants to make a disciple for a town, and its work. A town with fewer than two breeders first looks
/// for a villager to make a breeder; with none, it takes the most wanted work, by how good the free builder found is.
/// - `breeder()`: how good the villager found to make a breeder is, 0 for none.
/// - `freeBuilder()`: how good the free villager found for the work is, 0 for none.
template <typename Breeder, typename FreeBuilder>
[[nodiscard]] std::optional<DiscipleChoice> TownDiscipleWant(const std::array<uint8_t, 13>& disciples, uint32_t adults,
                                                             float worshipShare, std::span<const float> desires,
                                                             Breeder&& breeder, FreeBuilder&& freeBuilder)
{
	if (static_cast<float>(disciples.at(static_cast<size_t>(VillagerDisciple::Breeder))) < k_FewestBreeders)
	{
		if (const float found = breeder(); found > 0.0f)
		{
			return DiscipleChoice {.work = VillagerDisciple::Breeder, .value = found};
		}
	}
	auto choice = MostWantedDisciple(disciples, adults, worshipShare, desires);
	if (choice.has_value())
	{
		choice->value = freeBuilder() * choice->value;
	}
	return choice;
}

/// What the god knows of a building site to work out its want of workers or wood
struct SiteFacts
{
	/// The most builders the building takes, and the builders the site still wants
	int32_t maxBuilders {0};
	int32_t buildersNeeded {0};
	/// The building is built, and the site mends it: then it wants mending by this much
	bool built {false};
	float repairDesire {0.0f};
	/// The wood the site still wants to build the building
	float woodNeeded {0.0f};
};
/// Whether the god gives the site workers: it takes builders and wants more, and a mending site wants mending
[[nodiscard]] bool TakesWorkers(const SiteFacts& site);
/// Whether the god gives the site wood: it wants wood, and a mending site wants mending
[[nodiscard]] bool TakesWood(const SiteFacts& site);

/// How much the god wants to give a building site workers: its want of villagers, less the share of the most builders
/// that the town's builders already fill, by the pull of the building's place
[[nodiscard]] float SiteWorkersValue(float pull, float desireForVillagers, uint32_t builders, int32_t maxBuilders);
/// How much the god wants to give a building site wood: what it lacks of the cube of its share of the wood that it
/// needs, by the pull of the building's place
[[nodiscard]] float SiteWoodValue(float pull, uint32_t wood, float woodNeeded);
/// How good a free villager is as a builder: the pull of its place by its life by how near the hand it is
[[nodiscard]] float FreeBuilderValue(float pull, float life, float nearness);

/// How far from a building the hand puts a worker for its site: a quarter more than the building's radius
[[nodiscard]] float SiteWorkerDropRadius(float buildingRadius);
/// How far from its home's middle the hand puts a tired worshipper: a quarter more than the home's radius, and its own
[[nodiscard]] float HomeDropDistance(float homeRadius, float villagerRadius);

/// The steps of the angle round a building at which the hand tries to put a worker, in radians
constexpr float k_SiteWorkerDropStep = 0.7853981852531433f;
constexpr float k_FullTurn = 6.2831854820251465f;

/// Where the hand puts a worker for a building site: it tries points round the building, a step of an angle apart from
/// the angle 0. It keeps the last point on the map, and stops at a point on the map with wood there. With no point on
/// the map, the place stays at the start.
/// - `offset(angle)`: the offset of a point at an angle from the building, as a map position.
/// - `onMap(place)`: whether a place is on the map.
/// - `hasWood(place)`: whether a thing with wood stands in the cell of a place.
template <typename Offset, typename OnMap, typename HasWood>
[[nodiscard]] map_coords::MapCoords SiteWorkerDropPlace(const map_coords::MapCoords& building, map_coords::MapCoords start,
                                                        Offset&& offset, OnMap&& onMap, HasWood&& hasWood)
{
	auto place = start;
	for (float angle = 0.0f; angle < k_FullTurn; angle += k_SiteWorkerDropStep)
	{
		const map_coords::MapCoords point = building + offset(angle);
		if (onMap(point))
		{
			place = point;
		}
		if (hasWood(point))
		{
			break;
		}
	}
	return place;
}

} // namespace openblack::computer_mind_rules
