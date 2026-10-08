/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerCommands.h"

#include <cctype>

#include <algorithm>

#include <glm/vec2.hpp>

#include "3D/MapCoords.h"
#include "ECS/MapCells.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownStats.h"
#include "InfoConstants.h"
#include "Locator.h"

namespace openblack::lhscriptx::villager_commands
{

namespace
{
bool IEquals(std::string_view a, std::string_view b)
{
	return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
		       return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
	       });
}
} // namespace

int32_t VillagerInfoFromText(std::string_view text)
{
	// (inferred) the 9 tribe records are in tribe order (they are filled at start-up; the names are k_TribeStrs) and
	// each one's 7 villagers in VILLAGER_NUMBER order (k_VillagerNumberStrs)
	for (size_t tribe = 0; tribe < k_TribeStrs.size(); ++tribe)
	{
		const auto name = k_TribeStrs.at(tribe);
		if (text.size() <= name.size() || text[name.size()] != '_' || !IEquals(text.substr(0, name.size()), name))
		{
			continue; // the next tribe
		}
		const auto rest = text.substr(name.size() + 1);
		for (size_t number = 0; number < k_VillagerNumberStrs.size(); ++number)
		{
			if (IEquals(rest, k_VillagerNumberStrs.at(number)))
			{
				return static_cast<int32_t>(tribe * k_VillagerNumberStrs.size() + number);
			}
		}
		// none of its 7: the next tribe
	}
	return k_NoVillagerInfo;
}

std::optional<VillagerInfo> FindVillagerInfo(Tribe tribe, VillagerNumber number)
{
	const auto& villagers = Locator::infoConstants::value().villager;
	for (size_t i = 0; i < villagers.size(); ++i)
	{
		// tribe, then number; the first match
		if (villagers.at(i).tribeType == tribe && villagers.at(i).villagerNumber == number)
		{
			return static_cast<VillagerInfo>(i);
		}
	}
	return std::nullopt;
}

AbodeAt FindAbodeAt(const glm::vec3& position)
{
	// the script position's MapCoords; only the cells are compared
	const auto at = map_coords::FromMetres(glm::vec2(position.x, position.z));
	AbodeAt found;
	ecs::map_cells::ForEachTown([&](entt::entity town) {
		for (const auto abode : ecs::town_stats::AbodesOf(town))
		{
			const auto door = ecs::abode_queries::GetArrivePos(abode);
			if (map_coords::CellOf(door.x) != map_coords::CellX(at) || map_coords::CellOf(door.y) != map_coords::CellZ(at))
			{
				continue;
			}
			// its town, and the abode only while it has room
			found.town = ecs::abode_villagers::TownOf(abode);
			const auto count = static_cast<uint32_t>(ecs::abode_villagers::VillagersOf(abode).size());
			found.abode = ecs::abode_villagers::MaxVillagers(abode) - count != 0 ? abode : entt::null;
			return false;
		}
		return true;
	});
	return found;
}

} // namespace openblack::lhscriptx::villager_commands
