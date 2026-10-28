/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScaffoldImpressionRules.h"

#include <algorithm>

#include "3D/MapCoords.h"

using namespace openblack;
using namespace openblack::ecs;

float scaffold_impression::ImpressiveValue(uint32_t value, float objectValue, std::optional<float> siteWant)
{
	// Multiplied in the game's order, each step rounded to a float
	const float scaffolds = static_cast<float>(value) * objectValue;
	return scaffolds * siteWant.value_or(1.0f);
}

TownDesireInfo scaffold_impression::SiteWant(bool abode)
{
	return abode ? TownDesireInfo::ForAbodes : TownDesireInfo::ForCivicBuilding;
}

uint32_t scaffold_impression::PeopleToImpress(uint32_t people, float proportion)
{
	const float part = static_cast<float>(people) * proportion;
	const auto count = static_cast<uint32_t>(map_coords::FtoL(part));
	return count > 1 ? count : 1;
}

std::vector<size_t> scaffold_impression::TownsToImpress(std::span<const TownEntry> towns)
{
	if (towns.empty())
	{
		return {};
	}
	const auto newest = static_cast<size_t>(std::ranges::max_element(towns, {}, &TownEntry::made) - towns.begin());
	const auto& first = towns[newest];
	std::vector<size_t> order;
	for (size_t i = 0; i < towns.size(); ++i)
	{
		if (towns[i].owner == first.owner && towns[i].gained > first.gained)
		{
			order.push_back(i);
		}
	}
	std::ranges::sort(order, {}, [towns](size_t i) { return towns[i].gained; });
	order.insert(order.begin(), newest);
	return order;
}

std::vector<entt::entity> scaffold_impression::FirstPeople(std::span<const std::vector<entt::entity>> abodes, uint32_t count)
{
	std::vector<entt::entity> people;
	for (const auto& abode : abodes)
	{
		for (const auto person : abode)
		{
			if (people.size() >= count)
			{
				return people;
			}
			people.push_back(person);
		}
	}
	return people;
}
