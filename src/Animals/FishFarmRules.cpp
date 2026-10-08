/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FishFarmRules.h"

#include <algorithm>

namespace openblack::fish_farm
{
namespace
{
/// The kinds of food a farm's table can name, of which any makes it hold food
constexpr uint32_t k_FoodKinds = 3u;
} // namespace

float Full(const Type& type)
{
	return (type.foodType & k_FoodKinds) != 0 ? type.foodValue : 0.0f;
}

float Grow(float fish, uint32_t turn, const Type& type)
{
	if (type.turnsPerFish != 0 && turn % type.turnsPerFish == 0)
	{
		fish += 1.0f;
	}
	if (fish < 0.0f)
	{
		return 0.0f;
	}
	const float full = Full(type);
	return full < fish ? full : fish;
}

uint32_t Take(float& fish, uint32_t wanted)
{
	if (static_cast<float>(wanted) <= fish)
	{
		fish -= static_cast<float>(wanted);
		return wanted;
	}
	const auto taken = static_cast<uint32_t>(fish);
	fish = 0.0f;
	return taken;
}

uint32_t FirstHandful(uint32_t initialScoop, const Type& type)
{
	const float full = Full(type);
	return static_cast<uint32_t>(full <= static_cast<float>(initialScoop) ? full : static_cast<float>(initialScoop));
}

uint32_t ScoopWanted(uint32_t ramp, const Type& type, uint32_t held, uint32_t maxPickedUp)
{
	const float full = Full(type);
	auto wanted = static_cast<uint32_t>(full <= static_cast<float>(ramp) ? full : static_cast<float>(ramp));
	if (maxPickedUp != 0)
	{
		const auto room = static_cast<uint32_t>(std::max<int64_t>(int64_t {maxPickedUp} - int64_t {held}, 0));
		wanted = std::min(wanted, room);
	}
	return wanted;
}

} // namespace openblack::fish_farm
