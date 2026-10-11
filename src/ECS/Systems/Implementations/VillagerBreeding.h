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

#include <entt/entity/fwd.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
}

/// Villagers making children: lovers meeting and kissing, a pregnancy and its countdown, the birth of a child at its
/// mother's side, a child following its mother about, and the breeder disciple drawing villagers of the other sex to it.
/// The rules are in ECS/VillagerBreedingRules.h; the state functions are the living action system's.
namespace openblack::ecs::villager_breeding
{

/// Whether a villager is of an age to make love
[[nodiscard]] bool IsSexuallyActive(entt::entity villager);
/// Whether a villager is a pregnant woman
[[nodiscard]] bool IsPregnant(entt::entity villager);

/// Two villagers meet to make love: they walk to stand a short way apart, or start at once when touching
void GoAndHaveSexWith(entt::entity villager, entt::entity mate);
/// Picked up while kissing, a villager's partner lets it go and decides afresh
void LetPartnerGo(entt::entity villager);
/// A woman's own business at her checks and at home: her pregnancy counts down by the turns since her last check, and
/// when her time comes she starts giving birth wherever she is. 1 when she did.
uint32_t WomanSpecial(components::LivingAction& action);
/// Going to bed: when the town feels sleep with all its heart, a woman at home with a male of her home in, or the women
/// at home with a man going to bed, may conceive
void BedTimeConceptions(entt::entity villager);
/// A villager dies or goes: the children whose mother it was lose her
void OrphanChildrenOf(entt::entity villager);

/// What a child does: home when it has grown or is hungry, what its town wants of a child, or follow its mother
uint32_t ChildDecideWhatToDo(components::LivingAction& action);

/// A breeder disciple sets about its work: home when pregnant, else to a spot by a home of its town to walk about,
/// while its town isn't too far over full. Whether it did.
bool SetupBreederDisciple(components::LivingAction& action);
/// How urgently a villager heeds a breeder: only one of an age and of the other sex, not under a script
[[nodiscard]] uint32_t ReactToBreederPriority(entt::entity villager, entt::entity breeder);

uint32_t StartHavingSex(components::LivingAction& action);
uint32_t HavingSex(components::LivingAction& action);
uint32_t StopHavingSex(components::LivingAction& action);
/// Leaving the states of love for any other: the villager's partner, still in the same state, stops too
bool ExitSex(components::LivingAction& action, VillagerStates next);
/// A partner no longer about is forgotten
bool SexValidate(components::LivingAction& action);

uint32_t StartsGivingBirth(components::LivingAction& action);
uint32_t GivingBirth(components::LivingAction& action);
uint32_t GivenBirth(components::LivingAction& action);
uint32_t ChildFollowsMother(components::LivingAction& action);

uint32_t BreederDisciple(components::LivingAction& action);
bool EnterBreeder(components::LivingAction& action, VillagerStates previous, VillagerStates next);
bool ExitBreeder(components::LivingAction& action, VillagerStates next);
uint32_t BreederJustLanded(components::LivingAction& action);
uint32_t ReactToBreeder(components::LivingAction& action);

} // namespace openblack::ecs::villager_breeding
