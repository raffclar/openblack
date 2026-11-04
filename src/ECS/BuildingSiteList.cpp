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

namespace
{

/// The town's own site first, else a site that another town shared with it
template <typename Sites>
auto FindIn(Sites& sites, entt::entity town) -> decltype(&*sites.begin())
{
	if (const auto own = std::ranges::find(sites, town, &BuildingSite::town); own != sites.end())
	{
		return &*own;
	}
	const auto shared =
	    std::ranges::find_if(sites, [town](const BuildingSite& site) { return building_site_list::IsSharedWith(site, town); });
	return shared != sites.end() ? &*shared : nullptr;
}

} // namespace

BuildingSite* building_site_list::Find(BuildingSites& sites, entt::entity town)
{
	return FindIn(sites.sites, town);
}

const BuildingSite* building_site_list::Find(const BuildingSites& sites, entt::entity town)
{
	return FindIn(sites.sites, town);
}

bool building_site_list::IsSharedWith(const BuildingSite& site, entt::entity town)
{
	return std::ranges::find(site.sharedWith, town, &BuildingSite::Listing::town) != site.sharedWith.end();
}

bool building_site_list::Share(BuildingSite& site, entt::entity town, uint32_t number)
{
	if (site.town == town || IsSharedWith(site, town))
	{
		return false;
	}
	site.sharedWith.push_back({.town = town, .number = number});
	return true;
}

uint32_t building_site_list::NumberFor(const BuildingSite& site, entt::entity town)
{
	if (site.town == town)
	{
		return site.number;
	}
	const auto listing = std::ranges::find(site.sharedWith, town, &BuildingSite::Listing::town);
	return listing != site.sharedWith.end() ? listing->number : 0;
}

bool building_site_list::SetNumberFor(BuildingSite& site, entt::entity town, uint32_t number)
{
	if (site.town == town)
	{
		site.number = number;
		return true;
	}
	const auto listing = std::ranges::find(site.sharedWith, town, &BuildingSite::Listing::town);
	if (listing == site.sharedWith.end())
	{
		return false;
	}
	listing->number = number;
	return true;
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
	const auto* removed = Find(sites, town);
	if (removed == nullptr)
	{
		return std::nullopt;
	}
	const auto found = sites.sites.begin() + (removed - sites.sites.data());
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
