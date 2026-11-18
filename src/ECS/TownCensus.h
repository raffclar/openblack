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

#include <array>
#include <optional>
#include <span>
#include <unordered_map>
#include <unordered_set>
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

/// The number of kinds of disciple work, with no work as the kind 0
inline constexpr size_t k_DiscipleKinds = 13;

/// The people of a town as a walk of all villagers counts them
struct TownCount
{
	uint32_t people {0};
	/// The people that are not children
	uint32_t adults {0};
	/// The disciples of each kind of work. The kind 0 (no work) stays at 0.
	std::array<uint32_t, k_DiscipleKinds> disciples {};
	bool operator==(const TownCount&) const = default;
};

/// What one villager adds to the count of its town
struct CountedVillager
{
	entt::entity town {entt::null};
	bool adult {false};
	/// The kind of its disciple work, or 0 for none
	uint8_t disciple {0};
	bool operator==(const CountedVillager&) const = default;
};

/// The counts of the people of each town. The counts change with each villager that changes, so that a read does not
/// walk all villagers.
struct Counts
{
	/// The resets of the registry when the counts were made. Other values mean that the counts are old.
	std::optional<uint32_t> resets;
	/// The counts must come again from a walk of all villagers
	bool recount {true};
	std::unordered_map<entt::entity, TownCount> towns;
	/// What each villager adds now to the count of its town
	std::unordered_map<entt::entity, CountedVillager> villagers;
	/// The villagers that changed since the last read
	std::unordered_set<entt::entity> changed;
	/// The villagers whose villager or disciple component goes. The store holds the component until the signal ends,
	/// and the walk counts it until then, so these stay in this set until the store does not hold the component.
	std::unordered_set<entt::entity> goingVillagers;
	std::unordered_set<entt::entity> goingDisciples;
};

struct Caches
{
	Members members;
	Towns towns;
	Counts counts;
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

/// The count of the people of a town, the same as a walk of all villagers gives
[[nodiscard]] TownCount CountOf(const Registry& registry, entt::entity town);
/// Tell the counts that the town, the life stage or the disciple work of a villager can be different now. The signals
/// of the villager and disciple components do this too. A direct change to a field of a component must call this.
void VillagerChanged(const Registry& registry, entt::entity villager);
/// Tell the counts that the villager or disciple component of an entity goes now
void VillagerGoing(const Registry& registry, entt::entity villager);
void DiscipleGoing(const Registry& registry, entt::entity villager);
/// Tell the counts to come again from a walk of all villagers at the next read
void RecountVillagers(const Registry& registry);
/// True when the counts of all towns are the same as a walk of all villagers gives. The debug builds test this.
[[nodiscard]] bool CountsMatchWalk(const Registry& registry);

} // namespace openblack::ecs::town_census
