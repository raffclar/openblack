/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PlannedBuildingRules.h"

#include <cmath>

#include <algorithm>
#include <numbers>

using namespace openblack;
using namespace openblack::ecs::planned_building;
namespace planned_building = openblack::ecs::planned_building;

namespace
{

/// A house's want, by how its town stands for room
constexpr float k_MostShortScale = 0.8f;
constexpr float k_MostShortCount = 10.0f;
constexpr float k_ShortShare = 0.2f;
constexpr float k_ShortBase = 0.6f;
/// Each house of the kind standing takes off a share of the want, at most a tenth
constexpr float k_LeastSpread = 10.0f;
/// A workshop is wanted half as much once the town has one or one is going up
constexpr float k_SecondWorkshop = 0.5f;

bool Has(const TownForPlans& town, AbodeNumber number)
{
	return town.built.at(static_cast<size_t>(number)) != 0;
}

/// A house: none wanted while the town has room for a tenth of its people and more besides; more wanted the further
/// short of room it is; then less for each house of its kind already standing
float HouseDesire(float desire, const PlannedKind& plan, const TownForPlans& town)
{
	const int32_t spare = town.adultRoom - static_cast<int32_t>(town.homeless);
	const auto needed = static_cast<int32_t>((town.adults + town.children) / 10 + 1);
	if (spare > needed)
	{
		desire = 0.0f;
	}
	else if (spare < 0)
	{
		const auto spareRoom = static_cast<float>(spare);
		const float scale = std::min(k_MostShortScale, desire - spareRoom / static_cast<float>(needed));
		// How many short, counted up to ten
		const auto shortBy = static_cast<uint32_t>(-std::max(-k_MostShortCount, spareRoom));
		const float share =
		    shortBy > plan.maxAdults
		        ? static_cast<float>(plan.maxAdults) / static_cast<float>(shortBy) * k_ShortShare
		        : static_cast<float>(shortBy) / static_cast<float>(plan.maxAdults) * k_ShortShare + k_ShortShare;
		desire = (share + k_ShortBase) * scale;
	}
	const auto standing = plan.number != AbodeNumber::Invalid ? town.built.at(static_cast<size_t>(plan.number)) : uint8_t {0};
	const float spread = std::max(k_LeastSpread, static_cast<float>(standing + 1));
	return desire - desire / spread * static_cast<float>(standing);
}

/// The want of a building the town may have only one of, shared over no sites: none once it has one or one is going up
float OneOnly(float desire, bool has, uint32_t sites)
{
	return has || sites != 0 ? 0.0f : desire;
}

} // namespace

float planned_building::DesireToBeBuilt(const PlannedKind& plan, const TownForPlans& town)
{
	const float desire = plan.desireToBeBuilt;
	if (desire == 0.0f)
	{
		return desire;
	}
	const auto sites = plan.sitesOfSameType;
	float wanted = desire;
	switch (plan.type)
	{
	case AbodeType::LivingQuarters:
		wanted = HouseDesire(desire, plan, town);
		break;
	case AbodeType::Totem:
		if (town.neutral)
		{
			return 0.0f;
		}
		return OneOnly(desire, Has(town, AbodeNumber::Totem) || Has(town, AbodeNumber::TownCentre), sites);
	case AbodeType::TownCentre:
		return OneOnly(desire, Has(town, AbodeNumber::TownCentre) || Has(town, AbodeNumber::Totem), sites);
	case AbodeType::StoragePit:
		return OneOnly(desire, Has(town, AbodeNumber::StoragePit), sites);
	case AbodeType::Creche:
		return OneOnly(desire, Has(town, AbodeNumber::Creche), sites);
	case AbodeType::FootballPitch:
		if (!k_FootballEnabled)
		{
			return 0.0f;
		}
		return OneOnly(desire, Has(town, AbodeNumber::FootballPitch), sites);
	case AbodeType::Workshop:
		wanted = town.neutral ? 0.0f : desire;
		if (!Has(town, AbodeNumber::Workshop) && sites == 0)
		{
			return wanted;
		}
		wanted *= k_SecondWorkshop;
		break;
	case AbodeType::Graveyard:
		wanted = Has(town, AbodeNumber::Graveyard) ? 0.0f : desire;
		break;
	case AbodeType::SpellDispenser:
		wanted = Has(town, AbodeNumber::TownCentre) && Has(town, AbodeNumber::StoragePit) &&
		                 Has(town, AbodeNumber::Graveyard) && Has(town, AbodeNumber::Creche) && Has(town, AbodeNumber::Workshop)
		             ? desire
		             : 0.0f;
		break;
	case AbodeType::Wonder:
		// Wanted by the town's want of a wonder, and not at all below the want that sets its villagers to it
		wanted = town.wonderDesire < town.wonderTrigger ? 0.0f : town.wonderDesire * desire;
		break;
	default:
		break;
	}
	return sites != 0 ? wanted / static_cast<float>(sites) : wanted;
}

std::optional<size_t> planned_building::BestPlanned(std::span<const PlannedCandidate> planned, AbodeType wanted)
{
	std::optional<size_t> best;
	float most = 0.0f;
	for (size_t i = 0; i < planned.size(); ++i)
	{
		const auto& candidate = planned[i];
		if ((static_cast<uint32_t>(candidate.type) & static_cast<uint32_t>(wanted)) == 0)
		{
			continue;
		}
		if (most < candidate.desire)
		{
			most = candidate.desire;
			best = i;
		}
	}
	return best;
}

bool planned_building::WithinNearestOtherTown(glm::ivec2 cell, std::span<const OtherTown> others)
{
	const OtherTown* nearest = nullptr;
	auto nearestDistance = k_FarthestTown;
	for (const auto& other : others)
	{
		const int32_t dx = std::abs(cell.x - other.cell.x);
		const int32_t dz = std::abs(cell.y - other.cell.y);
		const auto distance = static_cast<uint32_t>(dx < dz ? (dx >> 1) + dz : (dz >> 1) + dx);
		if (distance < nearestDistance)
		{
			nearestDistance = distance;
			nearest = &other;
		}
	}
	if (nearest == nullptr || !nearest->areaMin.has_value() || !nearest->areaMax.has_value())
	{
		return false;
	}
	// Measured the game's way, without sign: a cell within four of the map's edge never lies within a box beyond it
	const auto x = static_cast<uint32_t>(cell.x);
	const auto z = static_cast<uint32_t>(cell.y);
	const auto margin = static_cast<uint32_t>(k_AreaMargin);
	return x + margin >= static_cast<uint32_t>(nearest->areaMin->x) &&
	       x - margin <= static_cast<uint32_t>(nearest->areaMax->x) &&
	       z + margin >= static_cast<uint32_t>(nearest->areaMin->y) && z - margin <= static_cast<uint32_t>(nearest->areaMax->y);
}

bool planned_building::IsClearToBuild(std::span<const FootprintCell> footprint, bool dryLandAtPlace, float reach)
{
	for (const auto& cell : footprint)
	{
		if (!dryLandAtPlace)
		{
			return false;
		}
		const bool blocked = std::ranges::any_of(
		    cell.buildings, [reach](const BuildingNearby& building) { return building.distance < building.radius + reach; });
		if (blocked)
		{
			return false;
		}
	}
	return true;
}

std::vector<std::optional<size_t>>
planned_building::StartedByScript(std::span<const std::vector<building_construction::PlannedCandidate>> towns, glm::vec2 place)
{
	std::vector<std::optional<size_t>> started;
	started.reserve(towns.size());
	for (const auto& planned : towns)
	{
		started.push_back(building_construction::PlannedAt(planned, place));
	}
	return started;
}

std::optional<planned_building::MendChoice> planned_building::ChooseToMend(std::span<const float> planDesires,
                                                                           std::span<const MendCandidate> buildings)
{
	float most = 0.0f;
	std::optional<size_t> plan;
	for (size_t i = 0; i < planDesires.size(); ++i)
	{
		if (planDesires[i] > most)
		{
			most = planDesires[i];
			plan = i;
		}
	}
	std::optional<size_t> building;
	for (size_t i = 0; i < buildings.size(); ++i)
	{
		if (!buildings[i].skipped && buildings[i].desireToBeRepaired > most)
		{
			most = buildings[i].desireToBeRepaired;
			building = i;
		}
	}
	if (building.has_value())
	{
		return MendChoice {.kind = MendChoice::Kind::Building, .index = *building};
	}
	if (plan.has_value())
	{
		return MendChoice {.kind = MendChoice::Kind::Plan, .index = *plan};
	}
	return std::nullopt;
}

float planned_building::YAngleOf(const glm::mat3& rotation)
{
	// Read back from where the building's own east side points, as it was turned when placed
	const float angle = std::atan2(rotation[0][2], rotation[0][0]);
	return angle < 0.0f ? angle + (2.0f * std::numbers::pi_v<float>) : angle;
}
