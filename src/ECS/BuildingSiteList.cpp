/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BuildingSiteList.h"

#include <algorithm>
#include <utility>

using namespace openblack::ecs;
using components::BuildingSite;
using components::BuildingSites;

BuildingSite* building_site_list::Find(BuildingSites& sites, entt::entity town)
{
	const auto found = std::ranges::find(sites.sites, town, &BuildingSite::town);
	return found != sites.sites.end() ? &*found : nullptr;
}

const BuildingSite* building_site_list::Find(const BuildingSites& sites, entt::entity town)
{
	const auto found = std::ranges::find(sites.sites, town, &BuildingSite::town);
	return found != sites.sites.end() ? &*found : nullptr;
}

BuildingSite* building_site_list::TakingBroughtWood(BuildingSites& sites)
{
	const auto found = std::ranges::find_if(sites.sites, &BuildingSite::takesBroughtWood);
	return found != sites.sites.end() ? &*found : nullptr;
}

const BuildingSite* building_site_list::TakingBroughtWood(const BuildingSites& sites)
{
	const auto found = std::ranges::find_if(sites.sites, &BuildingSite::takesBroughtWood);
	return found != sites.sites.end() ? &*found : nullptr;
}

BuildingSite& building_site_list::Open(BuildingSites& sites, entt::entity town)
{
	for (auto& site : sites.sites)
	{
		site.takesBroughtWood = false;
	}
	return *sites.sites.insert(sites.sites.begin(), BuildingSite {.town = town, .takesBroughtWood = true});
}

std::optional<BuildingSite> building_site_list::Close(BuildingSites& sites, entt::entity town)
{
	const auto found = std::ranges::find(sites.sites, town, &BuildingSite::town);
	if (found == sites.sites.end())
	{
		return std::nullopt;
	}
	auto closed = std::move(*found);
	sites.sites.erase(found);
	for (auto& site : sites.sites)
	{
		site.takesBroughtWood = false;
	}
	return closed;
}

std::vector<entt::entity> building_site_list::ClosingOrder(const BuildingSites& sites)
{
	std::vector<entt::entity> towns;
	towns.reserve(sites.sites.size());
	if (const auto* brought = TakingBroughtWood(sites))
	{
		towns.push_back(brought->town);
	}
	for (const auto& site : sites.sites)
	{
		if (!site.takesBroughtWood)
		{
			towns.push_back(site.town);
		}
	}
	return towns;
}
