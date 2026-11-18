/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>
#include <unordered_map>
#include <unordered_set>

#include <entt/entity/entity.hpp>

namespace openblack::ecs
{
class Registry;
}

/// The count of the members of the group of each reaction. A member holds the reaction and did not leave its group.
/// The counts change with each living thing whose reaction changes, so that a read does not walk all living things.
namespace openblack::ecs::reaction_groups
{

/// What one living thing adds to the counts: the reaction that it holds, and the reaction whose group it left
struct Held
{
	uint32_t reaction {0};
	uint32_t left {0};
	bool operator==(const Held&) const = default;
};

struct Counts
{
	/// The resets of the registry when the counts were made. Other values mean that the counts are old.
	std::optional<uint32_t> resets;
	/// The counts must come again from a walk of all living things
	bool recount {true};
	std::unordered_map<uint32_t, uint32_t> members;
	/// What each living thing adds now to the counts
	std::unordered_map<entt::entity, Held> held;
	/// The living things whose reaction changed since the last read
	std::unordered_set<entt::entity> changed;
	/// The living things whose reaction component goes. The walk counts the component until the store does not hold
	/// it, so these stay in this set until then.
	std::unordered_set<entt::entity> going;
};

/// How many living things hold the reaction and did not leave its group, the same as a walk of all living things gives
[[nodiscard]] uint32_t MembersOf(const Registry& registry, uint32_t reaction);
/// Tell the counts that the reaction that a living thing holds, or the reaction whose group it left, can be different
/// now. The signals of the component do this when it comes. A direct change to a field of the component must call this.
void Changed(const Registry& registry, entt::entity living);
/// Tell the counts that the reaction component of a living thing goes now
void Going(const Registry& registry, entt::entity living);
/// Tell the counts to come again from a walk of all living things at the next read
void Recount(const Registry& registry);
/// True when the counts of all reactions are the same as a walk of all living things gives. The debug builds test this.
[[nodiscard]] bool CountsMatchWalk(const Registry& registry);

} // namespace openblack::ecs::reaction_groups
