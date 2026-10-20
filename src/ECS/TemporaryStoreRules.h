/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>

#include "Enums.h"

/// A town with no storage pit that works keeps its food and its wood in two temporary piles, one for each. A pile is
/// made when it is first wanted, near the place where the town's people gather.
namespace openblack::ecs::temporary_store
{

/// A town keeps one temporary pile for food and one for wood
inline constexpr size_t k_Kinds = 2;
/// The pile is made this far from the gathering place, along the angle 0: the wood 5 metres to one side, the food on it
inline constexpr float k_WoodOffset = 5.0f;
inline constexpr float k_FoodOffset = 0.0f;
/// A clear place for the pile is looked for this far from that point, in steps of this length, with nothing fixed this
/// near it
inline constexpr float k_ClearSearch = 45.0f;
inline constexpr float k_ClearStep = 1.5f;
inline constexpr float k_ClearRadius = 2.0f;

/// The town's place for the pile of a resource, none for a resource that has no pile
[[nodiscard]] constexpr std::optional<size_t> SlotOf(ResourceType type)
{
	switch (type)
	{
	case ResourceType::Food:
		return 0;
	case ResourceType::Wood:
		return 1;
	default:
		return std::nullopt;
	}
}

/// How far from the gathering place the pile of a resource is made
[[nodiscard]] constexpr float OffsetOf(ResourceType type)
{
	return type == ResourceType::Wood ? k_WoodOffset : k_FoodOffset;
}

/// The kind of pile a resource is kept in: the pile of magic wood for wood, the pile of magic food for anything else
[[nodiscard]] constexpr PotInfo PileOf(ResourceType type)
{
	return type == ResourceType::Food ? PotInfo::MagicFood : PotInfo::MagicWood;
}

/// What the town does with one of its temporary piles at its turn
enum class TurnStep : uint8_t
{
	/// It keeps the pile
	Keep,
	/// It forgets the pile, which has gone
	Forget,
	/// It removes the pile and forgets it
	Remove,
};
/// At its turn the town forgets a pile that has gone, and removes an empty pile once it has a storage pit that works.
/// A pile that still holds something stays.
[[nodiscard]] constexpr TurnStep AtTownTurn(bool pileThere, uint32_t amount, bool storagePitWorks)
{
	if (!pileThere)
	{
		return TurnStep::Forget;
	}
	return amount == 0 && storagePitWorks ? TurnStep::Remove : TurnStep::Keep;
}

/// What happens to a temporary pile when the town gets a storage pit that works
enum class NewPitStep : uint8_t
{
	/// Nothing: the pile has gone
	None,
	/// The empty pile is removed
	Remove,
	/// The pile becomes an ordinary pile, which sets up its reaction for the people to see
	Release,
};
[[nodiscard]] constexpr NewPitStep OnNewStoragePit(bool pileThere, uint32_t amount)
{
	if (!pileThere)
	{
		return NewPitStep::None;
	}
	return amount == 0 ? NewPitStep::Remove : NewPitStep::Release;
}

} // namespace openblack::ecs::temporary_store
