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

#include <entt/entity/entity.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
}

/// Disciples in the game: a villager held in the player's hand takes up, each turn, the work of what it is held near
/// (see disciple), glowing and showing the work's symbol; put down it sets about that work and keeps at it, waiting by
/// the town centre while there is none. The state functions are the living action system's.
namespace openblack::ecs::villager_disciple
{

/// The villager's work as a disciple, none for an ordinary villager
[[nodiscard]] VillagerDisciple TypeOf(entt::entity villager);
/// Sets the villager to a work, or frees it with none; a trader keeps the town it trades with
void SetType(entt::entity villager, VillagerDisciple type, entt::entity tradeTown = entt::null);

/// A villager is taken into the player's hand: it remembers the work it had
void PickedUp(entt::entity villager);
/// A villager let go by the hand. One dropped by force by its own player's hand (or one of nobody's) is freed.
void LetGo(entt::entity villager, std::optional<PlayerNames> player, bool forced);
/// A villager the hand put down where it stood: the player at this machine hears the guidance's line when its work
/// changed while held
void PutDown(entt::entity villager, std::optional<PlayerNames> player);
/// A hand (of the player or of a creature) released a villager, but did not put it on the land below it. The physics
/// did not accept the villager, or the villager flies. The villager gets back the work that it had at the pick-up. The
/// work that the hand showed to it has no effect.
void NotPutDown(entt::entity villager);

/// Each turn in the hand it takes up the work of what it is held near
uint32_t InHand(components::LivingAction& action);
/// Its first turn down after the hand let it go: the player is judged for a new disciple
void TouchDown(components::LivingAction& action);
/// Up from its landing: a disciple sets about its work by the thing it was put down by, walking up to it or going
/// straight into its work. Whether it did (otherwise it decides as any villager).
bool SetAboutWorkOnLanding(components::LivingAction& action);
/// A disciple deciding what to do sets about its work, or waits by the town centre with none. 1 when it did either.
uint32_t DecideAsDisciple(components::LivingAction& action);

/// Walking up to the thing it was put by and looking at it, then a builder builds it and others decide
uint32_t InspectObject(components::LivingAction& action);
/// Waiting by the town centre, looking for work again now and then
uint32_t DiscipleNothingToDo(components::LivingAction& action);
bool EnterDiscipleNothingToDo(components::LivingAction& action, VillagerStates previous, VillagerStates next);
/// With no work to be had, it walks to wait by a town's centre (or the town itself without one), as its own town's
/// disciples do; a trader waits by the town it trades with. Whether it went.
bool SetDiscipleNothingToDo(components::LivingAction& action, std::optional<entt::entity> town);

/// A town changed hands: its disciples living in its homes that work for it give up their work, and the missionaries
/// preaching in it stop
void TownClaimed(entt::entity town);

/// Every frame: the disciples' glows, and the symbol of the one in the hand or under it
void UpdateMarks();

} // namespace openblack::ecs::villager_disciple
