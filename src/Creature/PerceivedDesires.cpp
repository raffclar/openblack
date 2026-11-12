/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PerceivedDesires.h"

#include <algorithm>

using namespace openblack;
using namespace openblack::creature_perceived_desires;

void creature_perceived_desires::Increase(PerceivedDesires& desires, size_t desire, float amount)
{
	if (desire < desires.player.size())
	{
		desires.player.at(desire) = std::clamp(desires.player.at(desire) + amount, 0.0f, 1.0f);
	}
}

void creature_perceived_desires::IncreaseTown(PerceivedDesires& desires, size_t desire, float amount)
{
	if (desire < desires.town.size())
	{
		desires.town.at(desire) = std::clamp(desires.town.at(desire) + amount, 0.0f, 1.0f);
	}
}

void creature_perceived_desires::Fade(PerceivedDesires& desires)
{
	std::ranges::for_each(desires.player, [](float& value) { value *= k_TurnFade; });
	std::ranges::for_each(desires.town, [](float& value) { value *= k_TurnFade; });
}

std::optional<size_t> creature_perceived_desires::TakeDominant(PerceivedDesires& desires,
                                                               const std::function<bool(size_t)>& activated)
{
	std::optional<size_t> dominant;
	for (size_t desire = 0; desire < desires.player.size(); ++desire)
	{
		if (activated(desire) && desires.player.at(desire) > 0.0f)
		{
			desires.player.at(desire) = 0.0f;
			dominant = desire;
		}
	}
	return dominant;
}
