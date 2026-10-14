/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownMiracleRules.h"

namespace openblack::town_miracle_tool
{

std::vector<MagicType> TypesToGive(MagicType type, MagicType plain, bool withPlain)
{
	std::vector<MagicType> types {type};
	if (withPlain && plain != MagicType::None && plain != type)
	{
		types.push_back(plain);
	}
	return types;
}

std::vector<MagicType> TypesToTake(MagicType type, MagicType plain, const std::array<MagicType, 3>& levels, bool allLevels)
{
	if (!allLevels || plain == MagicType::None)
	{
		return {type};
	}
	std::vector<MagicType> types {plain};
	for (const auto level : levels)
	{
		if (level != MagicType::None && level != plain)
		{
			types.push_back(level);
		}
	}
	return types;
}

std::optional<NoSite> WhyNoSite(const SiteChecks& checks)
{
	if (checks.hasSite)
	{
		return std::nullopt;
	}
	if (checks.landWithoutSites && !checks.anyLand)
	{
		return NoSite::LandWithoutSites;
	}
	if (checks.population == 0)
	{
		return NoSite::NoPeople;
	}
	if (checks.neutral)
	{
		return NoSite::Neutral;
	}
	if (!checks.hasTemple)
	{
		return NoSite::NoTemple;
	}
	if (!checks.templeBuilt)
	{
		return NoSite::TempleNotBuilt;
	}
	return NoSite::NoPlace;
}

std::string_view Describe(NoSite reason)
{
	switch (reason)
	{
	case NoSite::Neutral:
		return "the town belongs to no player";
	case NoSite::NoTemple:
		return "the town's player has no temple";
	case NoSite::TempleNotBuilt:
		return "the town's player's temple is not built yet";
	case NoSite::LandWithoutSites:
		return "the first land has no worship sites";
	case NoSite::NoPeople:
		return "the town has no people";
	case NoSite::NoPlace:
		return "the temple has no free place for a site";
	}
	return "";
}

} // namespace openblack::town_miracle_tool
