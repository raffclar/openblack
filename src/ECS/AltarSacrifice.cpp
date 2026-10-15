/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AltarSacrifice.h"

namespace openblack::ecs::altar_sacrifice
{

namespace
{
/// A man cries one of eight samples, a woman one of six
constexpr Cry k_ManCry {.first = 221, .count = 8};
constexpr Cry k_WomanCry {.first = 536, .count = 6};
} // namespace

float SacrificeValue(float life, float tableValue)
{
	return (life * 0.5f + 0.5f) * tableValue;
}

std::optional<Sacrifice> Judge(const Offered& offered)
{
	const float value = SacrificeValue(offered.life, offered.tableValue);
	switch (offered.kind)
	{
	case Offering::Villager:
		if (!offered.siteBuilt || offered.indestructible)
		{
			return std::nullopt;
		}
		return Sacrifice {
		    .prayer = offered.child ? value * k_ChildBonus : value,
		    .deed = true,
		    .villagerDies = true,
		    .light = true,
		    .number = true,
		};
	case Offering::Animal:
		// An animal is taken whether or not the site is built, and even when indestructible
		return Sacrifice {
		    .prayer = value * offered.life,
		    .deed = true,
		    .animalCounts = true,
		    .number = true,
		};
	case Offering::Tree:
		if (offered.indestructible)
		{
			return std::nullopt;
		}
		return Sacrifice {.prayer = value * offered.life, .ghost = true, .number = true};
	case Offering::DeadTree:
		return Sacrifice {.prayer = value * offered.life, .ghost = true};
	}
	return std::nullopt;
}

float AnimalAlignmentChange(float sacrificeChange)
{
	return sacrificeChange * k_AnimalAlignmentShare;
}

std::optional<Cry> CryOf(bool child, bool female)
{
	if (child)
	{
		return std::nullopt;
	}
	return female ? k_WomanCry : k_ManCry;
}

} // namespace openblack::ecs::altar_sacrifice
