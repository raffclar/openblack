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

#include <functional>
#include <optional>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "ScriptHeaders/ScriptEnums.h"

/// The script containers and the script commands on them: a town (script type 9), a dance (10, (pending): openblack
/// has no dance) and a flock (11, ECS/Flocks.h). The
/// command handlers in CHLApi.cpp call these; the ids they return are the entities (the script's ids).
namespace openblack::ecs::script_containers
{

/// Towns, dances and flocks are containers, every other thing is not
[[nodiscard]] bool IsContainer(entt::entity thing);

/// The type's loop function: each member (controlled by the script first when a script flag is set, (pending) its
/// writer: never here), then `fn`, which stops the walk when it returns true. A town's members are in the town's
/// villager order (its abodes, each one's inhabitants, then the homeless); a flock's from its head. False for a type
/// without one (the callers' "No Loop function for type")
bool ForEachMember(entt::entity container, const std::function<bool(entt::entity)>& fn);

/// CALL_IN 055: the type's find function with the filter !IsInScript && script_type::Matches when
/// `excludingScripted`, else script_type::Matches. A town finds villagers (types 4 / 5), (pending) animals (6) and its
/// storage pit (16, no filter); another type logs "Looking for strange type in Town". entt::null for none
[[nodiscard]] entt::entity Find(entt::entity container, script::ObjectType type, uint32_t subtype, bool excludingScripted);

/// FLOCK_CREATE 036: a flock with the default flock info, added as a created script thing (controlled). The new
/// flock
entt::entity CreateFlock(const glm::vec3& position);

/// FLOCK_ATTACH 037: pops the leader flag, the target and the obj; a dance target (pending), a flock target (the
/// target controlled by the script first), a town target, else "Thing not added to id". The id it pushes (0 for none)
uint32_t Attach(uint32_t object, uint32_t target, bool asLeader);
/// FLOCK_DETACH 038: pops the container, then the obj id. The id it pushes
uint32_t Detach(uint32_t container, uint32_t object);
/// FLOCK_DISBAND 039: every member out, from the head: an animal a flock of its own (unless a game flag is set,
/// (pending) that flag: clear here), a villager only removed; its script reference released; a controlled one gets
/// the IN_SCRIPT state. A town or an abode: nothing; the container is not deleted
void Disband(uint32_t container);
/// ID_SIZE 040: a flock's size, a dance's (pending), a town's adults + children, a football's (pending); nullopt for
/// another thing ("Cannot Find Flock/Dance/Town Size", 0)
[[nodiscard]] std::optional<float> Size(uint32_t container);
/// CHANGE_INNER_OUTER_PROPERTIES 056 on a flock: outer != 0 -> domain radius = outer, inner != 0 -> flock distance =
/// inner, calm always ((pending) its reader), each truncated. (pending) a weather thing's own version
void ChangeInnerOuter(uint32_t object, float inner, float outer, float calm);

} // namespace openblack::ecs::script_containers
