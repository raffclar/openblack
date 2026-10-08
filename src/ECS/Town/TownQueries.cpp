/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownQueries.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <bitset>
#include <string>
#include <utility>

#include <fmt/format.h>
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "Audio/Services/Guidance.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "Common/TruncateToInt.h"
#include "Debug/StateHash.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/TotemStatue.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Fields.h"
#include "ECS/FishFarms.h"
#include "ECS/Map.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Scaffolds.h"
#include "ECS/StoragePitStore.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/TownCellObjectsInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/Workshops.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerHome.h"
#include "InfoConstants.h"
#include "Locator.h"

namespace openblack::ecs::town_queries
{
using namespace components;

namespace
{
/// The cell's object lists from their heads, the fixed list then the mobile one
std::vector<entt::entity> CellObjects(int cellX, int cellZ)
{
	return Locator::townCellObjects::value().ObjectsInCell(glm::ivec2(cellX, cellZ));
}

/// The object's 2D radius
float Radius2D(entt::entity object)
{
	return Locator::townCellObjects::value().Get2DRadius(object);
}

/// Whether the object is a field. openblack's fields (components::Field) are not abodes, so they are
/// never in the town's abode list; an abode of number Field is taken as one too
bool IsField(entt::entity abode)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<Field>(abode))
	{
		return true;
	}
	const auto* a = registry.TryGet<const Abode>(abode);
	return a != nullptr && a->type == AbodeNumber::Field;
}

/// The town's abode list, newest first: an abode joining the town goes to the head. openblack orders by the object
/// creation index, descending, as the order they joined (approximate: an abode joins its town when it is made). It
/// decides GetCongregationPos's y (the last one read: the oldest), the base of its fallback when there is one abode, and
/// which ones the ring of 100 keeps with more than 100
std::vector<entt::entity> TownAbodes(const Town& town)
{
	std::vector<std::pair<int64_t, entt::entity>> found;
	Locator::entitiesRegistry::value().Each<const Abode, const Transform>(
	    [&](entt::entity entity, const Abode& abode, const Transform&) {
		    if (abode.townId == town.id)
		    {
			    found.emplace_back(object_index::Of(entity), entity);
		    }
	    });
	std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
	std::vector<entt::entity> result;
	result.reserve(found.size());
	for (const auto& [index, entity] : found)
	{
		result.push_back(entity);
	}
	return result;
}

/// What the congregation trace keeps between calls, in the debug hooks' store (Locator::debugHooks)
struct TownQueriesDebugHooksState
{
	std::bitset<256> congregationTraced; // OPENBLACK_VILLAGER_TRACE: towns (id & 0xFF) already traced
};

TownQueriesDebugHooksState& TownQueriesDebugHooksData()
{
	if (!Locator::debugHooks::has_value())
	{
		std::fputs("ecs::town_queries: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return Locator::debugHooks::value().Get<TownQueriesDebugHooksState>();
}

/// GetCongregationPos's "town <id>" trace, once per town
void TraceCongregation(const Town& town, const std::string& line)
{
	if (const char* trace = std::getenv("OPENBLACK_VILLAGER_TRACE"); trace == nullptr || *trace == '\0')
	{
		return;
	}
	auto& traced = TownQueriesDebugHooksData().congregationTraced;
	const auto bit = static_cast<size_t>(town.id & 0xFF);
	if (traced.test(bit))
	{
		return;
	}
	traced.set(bit);
	if (auto logger = spdlog::get("game"); logger != nullptr)
	{
		const auto& info = Locator::infoConstants::value().town;
		SPDLOG_LOGGER_INFO(
		    logger,
		    "Villager trace: congregation town {} (turn {}): {} (gameTurnsAfterEmergencyVillagersReact {} "
		    "maxDistanceFromCongreationPosThatPeopleChillOut {:.2f} maxDistanceFromHouseThatPeopleChillOut {:.2f})",
		    town.id, villager::CurrentTurn(), line, info.gameTurnsAfterEmergencyVillagersReact,
		    info.maxDistanceFromCongreationPosThatPeopleChillOut, info.maxDistanceFromHouseThatPeopleChillOut);
	}
}
} // namespace

glm::ivec2 ToMapCoords(glm::vec2 metres)
{
	return {map_coords::ToFixed(metres.x), map_coords::ToFixed(metres.y)};
}

glm::vec2 ToMetres(glm::ivec2 mapCoords)
{
	return {map_coords::ToMetres(mapCoords.x), map_coords::ToMetres(mapCoords.y)};
}

glm::ivec2 PosOf(entt::entity object)
{
	const auto* transform = Locator::entitiesRegistry::value().TryGet<const Transform>(object);
	return transform != nullptr ? ToMapCoords({transform->position.x, transform->position.z}) : glm::ivec2(0);
}

float GetDistanceInMetres(glm::ivec2 a, glm::ivec2 b)
{
	// The whole distance converted to metres: the shared table hypotenuse, not std::hypot (the table is 0.024 % long at
	// 100 m), and the whole units are multiplied exactly
	return gutils::GetDistanceInMetres(a, b);
}

uint16_t GetAngleFromXZ(glm::ivec2 a, glm::ivec2 b)
{
	return gutils::GetAngleFromXZ(a, b);
}

float Get3DAngleFromXZ(glm::ivec2 a, glm::ivec2 b)
{
	return gutils::Get3DAngleFromXZ(a, b);
}

glm::ivec2 GetPosFromAngle(float angle, float metres)
{
	// The cosine in double, rounded once by the product with the distance, as the original's FPU does
	const auto pos = gutils::GetPosFromAngle(angle, metres);
	return {pos.x, pos.z};
}

uint32_t GetMapCellSpiralSizeFromRadius(float radius)
{
	return static_cast<uint32_t>(map_coords::CellSpiralSize(radius));
}

uint32_t GetIncrementSpiralSizeFromRadius(float a, float b)
{
	return static_cast<uint32_t>(map_coords::IncrementSpiralSize(a, b));
}

void SpiralIncrement(glm::ivec2& pos, int32_t& dir, int32_t& count, float step)
{
	// Through map_coords::SpiralIncrement
	map_coords::Spiral spiral {dir, count};
	map_coords::MapCoords coords {pos.x, pos.y, 0.0f};
	map_coords::SpiralIncrement(coords, spiral, step);
	dir = spiral.dir;
	count = spiral.count;
	pos = {coords.x, coords.z};
}

entt::entity GetStoragePit(entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* t = registry.TryGet<const Town>(town);
	if (t == nullptr || t->storagePit == entt::null)
	{
		return entt::null;
	}
	// Only when it is available
	return abode_queries::IsAvailable(t->storagePit) ? t->storagePit : entt::null;
}

entt::entity GetCreche(entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* t = registry.TryGet<const Town>(town);
	if (t == nullptr || t->creche == entt::null || !registry.Valid(t->creche))
	{
		return entt::null;
	}
	return t->creche;
}

Tribe TribeOf(entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* tribe = town != entt::null && registry.Valid(town) ? registry.TryGet<const Tribe>(town) : nullptr;
	return tribe != nullptr ? *tribe : Tribe::NONE;
}

void Pulse(entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* t = town != entt::null && registry.Valid(town) ? registry.TryGet<Town>(town) : nullptr; t != nullptr)
	{
		t->buildPulse = 1;
		t->buildPulsePrevious = 0;
	}
}

uint32_t NextOwnerListStamp()
{
	// the land's own counter (RegistryContext, made again with every land: Registry::Reset)
	return ++Locator::entitiesRegistry::value().Context().ownerListCounter;
}

void RegisterStateHash()
{
	state_hash::Register("town_owner_list", [](state_hash::Hasher& h) {
		for (const auto town : TownsNewestFirst())
		{
			h.U32(Locator::entitiesRegistry::value().Get<const Town>(town).ownerListStamp);
		}
	});
}

std::vector<entt::entity> TownsNewestFirst()
{
	std::vector<entt::entity> list;
	if (!Locator::entitiesRegistry::has_value())
	{
		return list;
	}
	// A new town goes to the head of the global list: the newest first, by the creation stamp
	std::vector<std::pair<uint32_t, entt::entity>> towns;
	Locator::entitiesRegistry::value().Each<const Town>(
	    [&](entt::entity entity, const Town& town) { towns.emplace_back(town.creationStamp, entity); });
	std::sort(towns.rbegin(), towns.rend());
	list.reserve(towns.size());
	for (const auto& [stamp, town] : towns)
	{
		list.push_back(town);
	}
	return list;
}

audio::guidance::HelpTown HelpTownOf(entt::entity town)
{
	audio::guidance::HelpTown help;
	auto& registry = Locator::entitiesRegistry::value();
	if (const auto* t = registry.TryGet<const Town>(town); t != nullptr)
	{
		help.population = t->stats.adults + t->stats.children;
	}
	const auto pit = GetStoragePit(town);
	help.storagePitFunctional = pit != entt::null && abode_queries::IsFunctional(pit);
	if (const auto* transform = registry.TryGet<const Transform>(town); transform != nullptr)
	{
		help.position = transform->position;
	}
	return help;
}

bool IsInStateOfEmergency(const Town& town)
{
	// A start turn and (unsigned) turn - start < gameTurnsAfterEmergencyVillagersReact
	const uint32_t start = town.emergencyStartTurn;
	if (start == 0)
	{
		return false;
	}
	return villager::CurrentTurn() - start < Locator::infoConstants::value().town.gameTurnsAfterEmergencyVillagersReact;
}

bool BlocksTownClearArea(entt::entity object)
{
	// Objects block; mobiles, mobile statics and trees do not
	const auto& registry = Locator::entitiesRegistry::value();
	return !registry.AnyOf<Mobile, MobileStatic, Tree>(object);
}

bool IsObject([[maybe_unused]] entt::entity object)
{
	// 1 for an object and everything derived from it
	return true;
}

float Get2DRadius(entt::entity object)
{
	return Radius2D(object);
}

std::vector<entt::entity> ObjectsInCell(glm::ivec2 cell)
{
	return CellObjects(cell.x, cell.y);
}

bool CheckForClearArea(glm::ivec2 pos, float radius, const ClearAreaFilter& filter, entt::entity excluded,
                       entt::entity* blocker)
{
	auto& registry = Locator::entitiesRegistry::value();
	// one read filter for every cell: the filters only test the objects
	const map_cells::ReadBatch batch;
	// The number of cells; dir = count = 1
	uint32_t cells = GetMapCellSpiralSizeFromRadius(radius);
	map_coords::Spiral spiral;
	map_coords::MapCoords cell {pos.x, pos.y, 0.0f}; // the walk's MapCoords: pos plus whole cells
	while (cells != 0)
	{
		// Only the cells inside the map (the whole cells, unsigned)
		if (map_coords::InBounds(cell))
		{
			for (const auto object : CellObjects(map_coords::CellX(cell), map_coords::CellZ(cell)))
			{
				if (!registry.Valid(object) || !registry.AllOf<Transform>(object))
				{
					continue;
				}
				// GetDistanceInMetres(object, pos) - Get2DRadius < r
				const float distance = GetDistanceInMetres(PosOf(object), pos);
				if (distance - Radius2D(object) < radius)
				{
					// The excluded object is skipped; the filter: 1 -> not clear
					if (object != excluded && filter && filter(object))
					{
						if (blocker != nullptr)
						{
							*blocker = object;
						}
						return false;
					}
				}
			}
		}
		// --cells; the spiral's next cell step (whole cells)
		--cells;
		map_coords::AddCells(cell, spiral.Next());
	}
	return true;
}

std::optional<glm::ivec2> FindClearArea(glm::ivec2 start, float a, float b, float radius, const ClearAreaFilter& filter,
                                        entt::entity excluded)
{
	// one read filter for every point tried (CheckForClearArea's nest in it)
	const map_cells::ReadBatch batch;
	// The number of points; dir = count = 1
	uint32_t points = GetIncrementSpiralSizeFromRadius(a, b);
	int32_t dir = 1;
	int32_t count = 1;
	glm::ivec2 pos = start;
	while (points != 0)
	{
		if (CheckForClearArea(pos, radius, filter, excluded))
		{
			return pos;
		}
		--points;
		SpiralIncrement(pos, dir, count, b);
	}
	// Only the local is reset to start: the callers keep their point
	return std::nullopt;
}

glm::ivec2 GetCongregationPos(entt::entity townEntity)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* town = registry.TryGet<Town>(townEntity);
	if (town == nullptr)
	{
		return PosOf(townEntity);
	}
	// The cache, unless x == 0 && z == 0 && y == 0.0
	if (town->congregationPos != glm::ivec2(0) || town->congregationPosY != 0.0f)
	{
		return town->congregationPos;
	}
	// A ring of 100; the abodes that are not fields, then (fewer than 3) the planned ones
	struct Item
	{
		glm::ivec2 xz;
		float y;
	};
	std::array<Item, 100> ring {};
	uint32_t write = 0;
	uint32_t n = 0;
	for (const auto abode : TownAbodes(*town))
	{
		if (IsField(abode))
		{
			continue;
		}
		const auto& transform = registry.Get<const Transform>(abode);
		ring.at(write) = {PosOf(abode), transform.position.y};
		write = (write + 1) % 100;
		++n;
	}
	if (n < 3)
	{
		// The planned list, oldest first: a planned abode is appended at the tail, as push_back does
		for (const auto& planned : town->plannedAbodes)
		{
			ring.at(write) = {ToMapCoords({planned.position.x, planned.position.z}), planned.position.y};
			write = (write + 1) % 100;
			++n;
		}
	}
	uint32_t read = 0;
	glm::ivec2 pos(0);
	float y = 0.0f;
	if (n > 1)
	{
		// The sums of x and z (32-bit, read as unsigned 64-bit), y the last one's
		uint32_t sumX = 0;
		uint32_t sumZ = 0;
		for (uint32_t i = 0; i < n; ++i)
		{
			const auto& item = ring.at(read);
			read = (read + 1) % 100;
			sumX += static_cast<uint32_t>(item.xz.x);
			sumZ += static_cast<uint32_t>(item.xz.y);
			y = item.y;
		}
		// sum / n, truncated. The loads are exact and the quotient is rounded once to 24 bits, so it is divided in double
		// (exact) and rounded to float once, as the FPU does: with 100 positions the quotient is above 2^24 and its ulp is
		// 2 or more
		pos.x = TruncateToInt(static_cast<float>(static_cast<double>(sumX) / static_cast<double>(n)));
		pos.y = TruncateToInt(static_cast<float>(static_cast<double>(sumZ) / static_cast<double>(n)));
		// FindClearArea(pos, pos, 130, 3, 10, BlocksTownClearArea, none)
		if (const auto clear = FindClearArea(pos, 130.0f, 3.0f, 10.0f, &BlocksTownClearArea, entt::null); clear.has_value())
		{
			pos = *clear;
			town->congregationPos = pos;
			town->congregationPosY = y;
			TraceCongregation(*town, fmt::format("n={} avg -> ({:.1f}, {:.1f})", n, ToMetres(pos).x, ToMetres(pos).y));
			return pos;
		}
	}
	// The ring's first unread item if any (n == 1), else the town's position
	const uint32_t remaining = read > write ? write - read + 100 : write - read;
	// (the whole base is copied: y is the base's)
	glm::ivec2 base = PosOf(townEntity);
	const auto* townTransform = registry.TryGet<const Transform>(townEntity);
	y = townTransform != nullptr ? townTransform->position.y : 0.0f;
	const char* from = "town";
	if (remaining > 0)
	{
		base = ring.at(read).xz;
		y = ring.at(read).y;
		from = "first";
	}
	// d = GameFloatRand(10) + 10 (drawn first), angle = GameFloatRand(2 pi) (second); base += GetPosFromAngle(angle, d)
	const float distance = game_random::GameFloatRand(10.0f) + 10.0f;
	const float angle = game_random::GameFloatRand(glm::two_pi<float>());
	pos = base + GetPosFromAngle(angle, distance);
	// The cache
	town->congregationPos = pos;
	town->congregationPosY = y;
	TraceCongregation(*town, fmt::format("n={} {} -> ({:.1f}, {:.1f})", n, from, ToMetres(pos).x, ToMetres(pos).y));
	return pos;
}

entt::entity FindTownWithID(uint32_t id)
{
	// The players in order, then the neutral one, each one's towns: the first whose script id is id
	entt::entity found = entt::null;
	map_cells::ForEachTown([&found, id](entt::entity town) {
		if (ScriptIdOf(town) == id)
		{
			found = town;
			return false;
		}
		return true;
	});
	return found;
}

uint32_t ScriptIdOf(entt::entity town)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* t = registry.Valid(town) ? registry.TryGet<const Town>(town) : nullptr;
	return t == nullptr ? 0xFFFFFFFFu : t->scriptId.value_or(t->id);
}

entt::entity TownByKey(uint32_t key)
{
	auto& towns = Locator::entitiesRegistry::value().Context().towns;
	const auto it = towns.find(key);
	return it != towns.end() ? it->second : entt::null;
}

entt::entity GetTown(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		return entt::null;
	}
	if (registry.AllOf<Villager>(object))
	{
		return villager::GetTown(object);
	}
	if (registry.AllOf<Field>(object))
	{
		return fields::TownOf(object);
	}
	if (registry.AllOf<FishFarm>(object))
	{
		return fish_farms::TownOf(object);
	}
	if (registry.AllOf<Abode>(object))
	{
		return abode_villagers::TownOf(object);
	}
	if (registry.AllOf<Scaffold>(object))
	{
		return scaffolds::GetTown(object);
	}
	if (const auto* totem = registry.TryGet<const TotemStatue>(object); totem != nullptr)
	{
		// A totem statue: its town centre's town, else none
		return totem->townCentre != entt::null ? GetTown(totem->townCentre) : entt::null;
	}
	if (registry.AllOf<Pot>(object))
	{
		// A pot: the town of the structure it is part of (when that is available), else none. openblack keeps no link
		// to the structure: a storage pit's pile, or a workshop's
		auto structure = StoragePitStore::OwnerOf(object);
		if (structure == entt::null)
		{
			structure = workshops::WorkshopOfPile(object);
		}
		return ecs::IsAvailable(structure) ? GetTown(structure) : entt::null;
	}
	// Every other object has no town
	return entt::null;
}
} // namespace openblack::ecs::town_queries
