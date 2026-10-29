/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "WonderSystem.h"

#include <cstddef>

#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownArtefact.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Wonder.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "ECS/WonderRules.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace wonder = openblack::ecs::wonder;

namespace
{
/// The land balance of how impressive the things in the world are
constexpr size_t k_ThingsBalance = 1;

ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

/// The record of a player on the land, none for a player not on it
Player* PlayerRecord(PlayerNames name)
{
	Player* found = nullptr;
	Entities().Each<Player>([&found, name](Player& record) {
		if (found == nullptr && record.name == name)
		{
			found = &record;
		}
	});
	return found;
}

/// The player of the town of a building, none for a building with no town
std::optional<PlayerNames> TownPlayerOf(const Abode& abode)
{
	auto& registry = Entities();
	const auto& towns = registry.Context().towns;
	const auto town = towns.find(abode.townId);
	if (town == towns.end() || !registry.Valid(town->second))
	{
		return std::nullopt;
	}
	return registry.Get<const Town>(town->second).owner;
}

/// The most power that the tribe of a finished wonder gives the player of its town changes by the power of the wonder.
/// Nothing changes for a wonder that is not finished, or that has no player or tribe.
template <typename Change>
void ChangeMostTribalPower(entt::entity building, Change change)
{
	auto& registry = Entities();
	if (!registry.Valid(building) || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto* data = registry.TryGet<const Wonder>(building);
	const auto* abode = registry.TryGet<const Abode>(building);
	if (data == nullptr || abode == nullptr || abode->info == AbodeInfo::None)
	{
		return;
	}
	const auto owner = TownPlayerOf(*abode);
	auto* record = owner.has_value() ? PlayerRecord(*owner) : nullptr;
	if (record == nullptr)
	{
		return;
	}
	const auto tribe = Locator::infoConstants::value().abode.at(static_cast<size_t>(abode->info)).tribeType;
	if (tribe == Tribe::NONE || static_cast<size_t>(tribe) >= Player::k_TribeCount)
	{
		return;
	}
	if (!ecs::construction::IsBuilt(building))
	{
		return;
	}
	auto& most = record->miracles.maxTribalPower.at(static_cast<size_t>(tribe));
	most = change(most, data->power);
}

void AddToPlayer(entt::entity building)
{
	ChangeMostTribalPower(building, wonder::WithWonder);
}

void RemoveFromPlayer(entt::entity building)
{
	ChangeMostTribalPower(building, wonder::WithoutWonder);
}

/// The wonders of a town, in the order of the town's buildings
std::vector<entt::entity> WondersOf(entt::entity town)
{
	auto& registry = Entities();
	std::vector<entt::entity> wonders;
	const auto* data = registry.Valid(town) ? registry.TryGet<const Town>(town) : nullptr;
	if (data == nullptr)
	{
		return wonders;
	}
	for (const auto building : data->abodes)
	{
		if (registry.Valid(building) && registry.AllOf<Wonder>(building))
		{
			wonders.push_back(building);
		}
	}
	return wonders;
}
} // namespace

void WonderSystem::Made(entt::entity building, float scale)
{
	auto& registry = Entities();
	if (!registry.Valid(building))
	{
		return;
	}
	registry.AssignOrReplace<Wonder>(building, Wonder {.power = scale});
	AddToPlayer(building);
}

void WonderSystem::Built(entt::entity building)
{
	AddToPlayer(building);
}

void WonderSystem::Lost(entt::entity building)
{
	RemoveFromPlayer(building);
}

void WonderSystem::TownLeavesPlayer(entt::entity town)
{
	for (const auto building : WondersOf(town))
	{
		RemoveFromPlayer(building);
	}
}

void WonderSystem::TownJoinsPlayer(entt::entity town)
{
	for (const auto building : WondersOf(town))
	{
		AddToPlayer(building);
	}
}

void WonderSystem::SetPower(entt::entity building, float power)
{
	auto& registry = Entities();
	if (auto* data = registry.Valid(building) ? registry.TryGet<Wonder>(building) : nullptr)
	{
		data->power = power;
	}
}

float WonderSystem::PowerAt(entt::entity town, glm::vec3 place) const
{
	auto& registry = Entities();
	const float desire = Locator::townDesireSystem::has_value()
	                         ? Locator::townDesireSystem::value().GetRawDesire(town, TownDesireInfo::ToBuildWonder)
	                         : 0.0f;
	// The things in the map cell of the place. The things made since the last look go into their cells first.
	std::vector<wonder::Neighbour> inCell;
	const auto coords = map_coords::FromMetres(glm::vec2(place.x, place.z));
	if (Locator::entitiesMap::has_value() && map_coords::InBounds(coords))
	{
		auto& map = Locator::entitiesMap::value();
		map.FileNew();
		const float balance = registry.Context().mapScriptGlobals.landBalance.at(k_ThingsBalance);
		for (const auto thing : map.GetAllInCell(map_coords::Cell(coords)))
		{
			const auto* transform = registry.Valid(thing) ? registry.TryGet<const Transform>(thing) : nullptr;
			if (transform == nullptr)
			{
				continue;
			}
			const auto* artefact = registry.TryGet<const TownArtefact>(thing);
			const auto* info = ecs::world_objects::InfoOf(thing);
			inCell.push_back({
			    .distance = gutils::GetDistanceInMetres(place, transform->position),
			    .artefactWorth = artefact != nullptr ? artefact->value : 0.0f,
			    .impressiveness = info != nullptr ? balance * info->impressiveValue : 0.0f,
			});
		}
	}
	return wonder::Power(desire, inCell);
}

float WonderSystem::ScaleAt(entt::entity town, glm::vec3 place) const
{
	return wonder::Scale(PowerAt(town, place));
}

void WonderSystem::ProcessTurn()
{
	Entities().Each<Player>([](Player& record) {
		for (size_t tribe = 0; tribe < Player::k_TribeCount; ++tribe)
		{
			auto& most = record.miracles.maxTribalPower.at(tribe);
			most = wonder::SettledTribalPower(most);
			record.miracles.tribalPower.at(tribe) = most;
		}
	});
}
