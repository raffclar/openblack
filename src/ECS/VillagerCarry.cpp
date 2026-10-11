/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerCarry.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// The game keeps each load in 16 bits and wraps when it overflows
int16_t Wrap(int32_t value)
{
	return static_cast<int16_t>(static_cast<uint16_t>(static_cast<uint32_t>(value)));
}
} // namespace

int16_t villager_carry::PickUp(components::Villager& villager, ResourceType type, int16_t amount, uint8_t woodGraphic)
{
	if (type == ResourceType::Food)
	{
		villager.foodHeld = Wrap(villager.foodHeld + amount);
	}
	else
	{
		villager.woodHeld = Wrap(villager.woodHeld + amount);
		villager.woodGraphic = woodGraphic & 3u;
	}
	return amount;
}

uint16_t villager_carry::PutDown(components::Villager& villager, ResourceType type, uint16_t amount)
{
	auto& held = type == ResourceType::Food ? villager.foodHeld : villager.woodHeld;
	const auto have = static_cast<uint16_t>(held);
	if (amount == 0 || amount > have)
	{
		amount = have;
	}
	held = Wrap(held - amount);
	return amount;
}

int32_t villager_carry::Room(const components::Villager& villager, ResourceType type, int32_t maxFood, int32_t maxWood)
{
	return type == ResourceType::Food ? maxFood - villager.foodHeld : maxWood - villager.woodHeld;
}

ResourceType villager_carry::HeldResource(const components::Villager& villager)
{
	const auto food = static_cast<uint16_t>(villager.foodHeld);
	const auto wood = static_cast<uint16_t>(villager.woodHeld);
	if (food > wood)
	{
		return ResourceType::Food;
	}
	return wood != 0 ? ResourceType::Wood : ResourceType::None;
}
