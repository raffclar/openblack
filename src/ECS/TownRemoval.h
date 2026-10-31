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

#include <vector>

#include <entt/entity/entity.hpp>

namespace openblack::ecs
{
class Registry;
}

/// The removal of a town from the game. The game removes a town at the moment one of its buildings leaves it, if the
/// town then has no building, no planned building and no person.
namespace openblack::ecs::town_removal
{

/// What a town keeps after one of its buildings leaves it
struct Left
{
	/// Its buildings
	size_t buildings {0};
	/// The buildings that it plans to build
	size_t plans {0};
	/// Its adults and children, those with no home too
	uint32_t people {0};
};

/// Whether the town goes from the game when one of its buildings leaves it
[[nodiscard]] constexpr bool GoesWhenBuildingLeaves(const Left& left)
{
	return left.buildings == 0 && left.plans == 0 && left.people == 0;
}

/// What the town keeps now: its valid buildings and plans, and the villagers that belong to it
[[nodiscard]] Left LeftIn(const Registry& registry, entt::entity town);

/// The things that go or change with a town, in the registry alone
struct Dependants
{
	/// The animals of the town: each one becomes an animal of no town
	std::vector<entt::entity> animals;
	/// The reward chests that the game gave to the town: they go
	std::vector<entt::entity> rewards;
	/// The flags of the town's desires: they go
	std::vector<entt::entity> desireFlags;
};

/// The things that go or change with the town
[[nodiscard]] Dependants DependantsOf(const Registry& registry, entt::entity town);

/// The registry forgets the town. In this sequence:
/// 1. Its animals become animals of no town.
/// 2. Its reward chests and the flags of its desires go.
/// 3. The list of all the towns forgets its number, and its entity goes.
/// Other things that keep the town, such as a field or a fish farm, keep it as it was. The town is no longer valid for
/// them.
void Forget(Registry& registry, entt::entity town);

} // namespace openblack::ecs::town_removal
