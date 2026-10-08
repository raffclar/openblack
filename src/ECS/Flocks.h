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
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

/// The original's flock for any Living, villagers and animals alike. components::Flock::members is the original's
/// member list (head, tail, next / prev per node) **reversed**: front() is the tail, the leader, back() is the head.
/// The list is kept ascending by the Living's flock order byte from the head: a new member goes before the first
/// node from the head with an order >= its own, so a member with 0 (every Living but a leader) becomes the new head
/// (push_back). The animal AI's own flock code (ECS/AnimalAI.cpp) follows the same convention.
namespace openblack::ecs::flocks
{

/// The id of a flock a script makes (FLOCK_CREATE, FLOCK_ATTACH)
inline constexpr int32_t k_ScriptFlockId = 0xABA52;

/// The flock info every flock creator passes (the citadel, FLOCK_CREATE, the flock spell, the vortex).
/// (inferred) one record: its row 0
inline constexpr uint32_t k_DefaultFlockInfo = 0;

/// A new flock entity (the registry is the flock list): the info, the player and the id, SetDomainCentrePos(pos) with
/// no member, the saved domain centre = pos, domainRadius = 80, flockDistance = 30; maxMembers is not set
entt::entity Create(const glm::vec3& position, std::optional<uint32_t> info, std::optional<PlayerNames> player, int32_t id);
/// The same with no info and no player
entt::entity Create(const glm::vec3& position, int32_t id);
/// The same at the living's Pos with id 0, then AddMember(living)
entt::entity CreateFor(entt::entity living);

/// The flock of a villager or an animal, entt::null for none
[[nodiscard]] entt::entity FlockOf(entt::entity living);
/// The byte the member list is sorted by (0 unless AddLeader set it)
[[nodiscard]] uint8_t OrderOf(entt::entity living);
/// A flock already -> RemoveLiving(it, living, true); then the living's flock = flock
void SetFlock(entt::entity living, entt::entity flock);

/// Out of its old flock first (which goes when emptied); already a member -> false ("Living already in Flock" is the
/// callers'); else inserted by its order, ++count, SetFlock. True when added
bool AddMember(entt::entity flock, entt::entity living);
/// The tail already -> nothing; the order = the tail's + 1 (5 for an empty flock); a member -> moved to its new place
/// (no SetFlock); else AddMember
void AddLeader(entt::entity flock, entt::entity living);
/// Not a member -> its flock link cleared, false; else unlinked, --count, the link cleared and, with deleteWhenEmpty
/// and no member left, the flock is deleted. True when removed
bool RemoveLiving(entt::entity flock, entt::entity living, bool deleteWhenEmpty);
/// RemoveLiving, then a flock of its own (CreateFor) with this one's domainRadius and flockDistance. The new flock
entt::entity SeparateIntoNewFlock(entt::entity flock, entt::entity living, bool deleteWhenEmpty);
/// From the other's head, while it has a member: keeper maxMembers += other maxMembers (each step), clamped to an
/// animal member's maxFlockSize, then AddMember(keeper, it), which empties and deletes the other at the end
void Merge(entt::entity keeper, entt::entity other);

/// The tail: the leader (as IsLeader, GetFlockPos and SetDomainCentrePos use it).
/// (openblack) the first available member from the tail, as the animal AI's LeaderOf
[[nodiscard]] entt::entity Leader(entt::entity flock);
/// A flock and its tail is this living
[[nodiscard]] bool IsLeader(entt::entity living);
/// The member count
[[nodiscard]] uint32_t Size(entt::entity flock);
/// The members from the head (the original's walking order: FindLiving, disbanding by id, the script loops)
[[nodiscard]] std::vector<entt::entity> MembersFromHead(entt::entity flock);
/// Count 0 -> null; r = GameRand(count - 1) when exclude and count > 1, else GameRand(count); then from the head the
/// first member at an index >= r that is not `exclude`
[[nodiscard]] entt::entity RandomMember(entt::entity flock, entt::entity exclude);
/// From the head, the first member `match` accepts (entt::null for none)
[[nodiscard]] entt::entity FindLiving(entt::entity flock, const std::function<bool(entt::entity)>& match);

/// The tail's Pos, else the flock's domain centre
[[nodiscard]] glm::vec3 GetFlockPos(entt::entity flock);
/// The tail's destination (a villager's WallHug::goal, an animal's AnimalBrain::goal) and the flock's domain centre
void SetDomainCentrePos(entt::entity flock, const glm::vec3& position);
/// No flock -> false; else GetDistanceInMetres(the domain centre, pos) <= domainRadius x factor
[[nodiscard]] bool PosWithinDomain(entt::entity living, glm::vec2 position, float factor);

} // namespace openblack::ecs::flocks
