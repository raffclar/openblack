/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureCarryOver.h"

#include <algorithm>
#include <string>

using namespace openblack;
using namespace openblack::creature_carry_over;

namespace
{
/// A millisecond in seconds, as a float product
constexpr float k_SecondsPerMillisecond = 0.001f;
} // namespace

Fizz creature_carry_over::SetFizz(const Fizz& fizz, float target, float seconds)
{
	target = std::clamp(target, 0.0f, 1.0f);
	if (seconds != 0.0f && target != fizz.now)
	{
		const auto rate = 1.0f / seconds;
		return {.now = fizz.now, .target = target, .perSecond = target - fizz.now > 0.0f ? rate : -rate};
	}
	return {.now = target, .target = target, .perSecond = 0.0f};
}

Fizz creature_carry_over::StepFizz(const Fizz& fizz, float turnMilliseconds)
{
	if (fizz.perSecond == 0.0f)
	{
		return fizz;
	}
	auto next = fizz;
	next.now = (turnMilliseconds * fizz.perSecond * k_SecondsPerMillisecond) + fizz.now;
	const bool rising = fizz.perSecond > 0.0f;
	if ((rising && next.now >= fizz.target) || (!rising && next.now <= fizz.target))
	{
		next.now = fizz.target;
		next.perSecond = 0.0f;
	}
	return next;
}

Fizz creature_carry_over::ArrivalFizz()
{
	return SetFizz(SetFizz({}, 1.0f, 0.0f), 0.0f, k_ArrivalSeconds);
}

map_coords::MapCoords creature_carry_over::ArrivalCoords(glm::vec2 place)
{
	return map_coords::CellCentre(map_coords::CellOf(map_coords::ToFixed(place.x)),
	                              map_coords::CellOf(map_coords::ToFixed(place.y)));
}

creature_carry_over::KeptFiles creature_carry_over::KeptFilesIn(const std::filesystem::path& folder,
                                                                std::string_view profileFile)
{
	const std::string name(profileFile);
	return {.mind = folder / name, .physique = folder / ("Physique" + name)};
}
