/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorshipSiteConstruction.h"

#include "ECS/WorshipSites.h"

using namespace openblack;
using namespace openblack::ecs;

glm::vec2 worship_site_construction::PilePlace(glm::vec2 spot, float facing)
{
	return worship_site::TurnedPoint(spot, facing, k_PilePoint);
}

worship_site_construction::Destination worship_site_construction::WhereGiven(ResourceType type, bool goingUp)
{
	if (goingUp && (type == ResourceType::Wood || type == ResourceType::Any))
	{
		// The building site takes wood only: anything given as any resource is left with the giver
		return type == ResourceType::Wood ? Destination::SitePile : Destination::Nowhere;
	}
	return type == ResourceType::Food ? Destination::FoodPot : Destination::Nowhere;
}

bool worship_site_construction::Stores(ResourceType type, bool goingUp)
{
	return type == ResourceType::Food || type == ResourceType::Any || (goingUp && type == ResourceType::Wood);
}
