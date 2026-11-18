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
#include <span>
#include <unordered_map>
#include <vector>

#include <entt/entity/entity.hpp>

namespace openblack::ecs
{
class Registry;
}

/// The villagers, abodes and miracle dispensers of each town, and each town by its number. A walk of the world finds
/// the same things in the same order. One walk fills the lists for all towns, and the lists stay until the registry
/// tells of a change to the towns or their members.
namespace openblack::ecs::town_census
{

/// What the registry was when the lists were made. Other values mean that the lists are old.
struct MembersKey
{
	uint32_t resets {0};
	uint64_t changes {0};
	size_t villagers {0};
	size_t abodes {0};
	size_t dispensers {0};
	bool operator==(const MembersKey&) const = default;
};

struct TownsKey
{
	uint32_t resets {0};
	uint64_t changes {0};
	size_t towns {0};
	bool operator==(const TownsKey&) const = default;
};

/// The lists of the members of the towns, in the order of the walk of each kind
struct Members
{
	std::optional<MembersKey> key;
	std::unordered_map<entt::entity, std::vector<entt::entity>> people;
	std::unordered_map<uint32_t, std::vector<entt::entity>> abodes;
	std::unordered_map<entt::entity, std::vector<entt::entity>> dispensers;
};

/// Each town number with the last town of the walk that has it, as a walk finds it
struct Towns
{
	std::optional<TownsKey> key;
	std::unordered_map<uint32_t, entt::entity> byNumber;
};

struct Caches
{
	Members members;
	Towns towns;
};

/// The villagers of a town, in the order of a walk of all villagers
[[nodiscard]] std::span<const entt::entity> PeopleOf(const Registry& registry, entt::entity town);
/// The abodes that have a town number, in the order of a walk of all abodes
[[nodiscard]] std::span<const entt::entity> AbodesOf(const Registry& registry, uint32_t townNumber);
/// The miracle dispensers of a town, in the order of a walk of all miracle dispensers
[[nodiscard]] std::span<const entt::entity> DispensersOf(const Registry& registry, entt::entity town);
/// The town that has a number: the last of a walk of all towns that has it, or none
[[nodiscard]] entt::entity TownByNumber(const Registry& registry, uint32_t townNumber);
/// The same for a number kept as a signed value: the town whose number reads as this value
[[nodiscard]] entt::entity TownBySignedNumber(const Registry& registry, int32_t townNumber);

} // namespace openblack::ecs::town_census
