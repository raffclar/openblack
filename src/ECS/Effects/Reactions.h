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

#include <vector>

#include <entt/entity/entity.hpp>

#include "Enums.h"

// A reaction: "something to react to" started by an object (a predator, a food pile, a thrown object, a fire, a
// spell, a teleport stone...). CreateReaction makes it and spreads it ONCE over a spiral of map cells (SpreadReaction)
// to the Living there, each class with its own handler (the animals' in ECS/AnimalFlee.cpp, the villagers' in
// ECS/Systems/Implementations/VillagerReactions.cpp). The per-turn re-spreading is behind a debug flag the shipped
// game never sets. The parts every Living shares are here: its records, the score and the rule to switch from the
// reaction it takes. Wiki: docs/bw1-notes/animals.md and magic.md.

namespace openblack::ecs::effects::reactions
{
struct Reaction
{
	uint32_t id {0};
	entt::entity initiator {entt::null};
	openblack::Reaction type {openblack::Reaction::None};
	PlayerNames player {PlayerNames::NEUTRAL};
	uint32_t turnCreated {0}; ///< 0 when made; the turn when created with `stamp`, or when a Living first takes it
	float radius {0.0f};      ///< info.whetherReactionGrows ? 1 : info.maxReactionDistance, or what its creator
	                          ///< sets (the spell shield: the shield's radius + 30)
	bool available {true};
	bool stealth {false}; ///< Off when made
};

/// The Living classes a reaction reaches (the class of each object of the cell's list)
enum class LivingClass
{
	Villager,
	Animal,
	Creature,
};

/// The reaction applied to one Living of the cell: `distance` is the one the original computes there,
/// (|dz| + |dx|) / 2 in metres from the initiator
using LivingReactionHandler = void (*)(entt::entity living, const Reaction& reaction, float distance);
void SetLivingReactionHandler(LivingClass living, LivingReactionHandler handler);
/// The shut-down of a reaction for the followers of a class: it is marked shut down, then while the reaction has
/// followers the head one stops reacting and sets its state, before the reaction is deleted.
/// RemoveAllReactionsInitiatedByObject / RemoveAllReactionsOfTypeInitiatedBy shut down each one they remove, in list
/// order; Prune does the same for the ones whose initiator went. Only the classes that keep
/// the original's follower link register one (the villagers' slot reactions, VillagerReactions.cpp); for the others
/// nothing changes
using LivingShutDownHandler = void (*)(uint32_t reaction);
void SetLivingShutDownHandler(LivingClass living, LivingShutDownHandler handler);

/// 0 for REACTION -1; else the new reaction, spread
/// at once (SpreadReaction), and its id
uint32_t CreateReaction(entt::entity initiator, openblack::Reaction type, PlayerNames player, bool stamp);

/// (max(1, trunc(radius x 0.2)))^2 x the reaction power (1) map cells of a spiral from the initiator's cell, the
/// ones within the radius; each cell's Living, in the cell's order, to its class's handler.
/// (inferred) The reaction power is 1 for every initiator: it is 1.0 for a plain object, but the overrides of a spell
/// (its strength) and a tree (its life: ECS/Life.h) are not ported, so a spell or tree initiator (MagicTree) spreads
/// over the unscaled count.
/// TODO(reactions): the stealth branch (stealth set and the cell's first object another Living: GameRand(1000) >
/// stealthRandomChance skips the cell, else stealth is cleared) is not ported; no creator sets stealth.
/// (approximate) The cells are openblack's map grid, rebuilt once per turn (and before a spread outside the turn), not
/// the original's lists that follow every move; a cell's order is the grid's (the registry's), not the original's
/// insertion order.
/// The spread centre is the initiator's position: its Transform, or, for an initiator with none,
/// components::Spell::position - a Spell entity has no Transform in openblack and IS the initiator of the shield
/// reactions (13 / 35 / 36).
void SpreadReaction(uint32_t reaction);

/// Shuts down and removes every reaction the object started
void RemoveAllReactionsInitiatedByObject(entt::entity initiator);
/// Every reaction the object started gets `available` (HandleApplyResult: true)
void SetAvailable(entt::entity initiator, bool available);
/// When the object is put in the hand: its reactions whose info has whetherReactionFinishesIfInitiatorInHand set are
/// no longer available
void SetUnavailableInHand(entt::entity initiator);
/// Shuts down and removes the object's reactions of that type
void RemoveAllReactionsOfTypeInitiatedBy(entt::entity initiator, openblack::Reaction type);
/// A deleted initiator takes its reactions with it
void Prune();

/// The reaction moves to another initiator (a tree's fire to its DeadTree)
void SetInitiator(uint32_t reaction, entt::entity initiator);
/// turnCreated = the game turn (the struck spell shield refreshes its reaction this way)
void Stamp(uint32_t reaction);
/// turnCreated = the turn if still 0 (a Living takes it for the first time)
void MarkStarted(uint32_t reaction, uint32_t turn);
/// The reaction's radius (the spell shield writes the radius of its REACT_TO_MAGIC_SHIELD)
void SetRadius(uint32_t reaction, float radius);

/// The first reaction the object started, 0 none
[[nodiscard]] uint32_t GetReactionInitiatedBy(entt::entity initiator);
/// The first reaction of that type the object started, 0 none
[[nodiscard]] uint32_t GetReactionOfTypeInitiatedBy(entt::entity initiator, openblack::Reaction type);

[[nodiscard]] const std::vector<Reaction>& All();
/// The reaction by its id, nullptr when gone
[[nodiscard]] const Reaction* Find(uint32_t id);
/// Not removed, available and its initiator still there (inferred)
[[nodiscard]] bool IsAvailable(const Reaction* reaction);

// ---- the parts of Living every class shares

/// The Living's records (components::ReactionRecords): a type it knows only when more than `again` turns went
/// since (its turn renewed); a new one is added (the oldest dropped past 3); on the way the other types older than
/// 1800 turns are forgotten. True: it may react.
bool Records(entt::entity living, uint8_t type, uint32_t again, uint32_t now);
/// The record's turn (0 if none)
[[nodiscard]] uint32_t RecordTurn(entt::entity living, uint8_t type);
/// When a Living stops reacting: the record of the type it stops reacting to gets the turn
void RefreshRecord(entt::entity living, uint8_t type, uint32_t now);

/// The score of a reaction for a Living at that distance, 0..255: 0 when its info does not react to the type (the
/// living info's isReacting[type]) or beyond maxReactionDistance; else priority x (1 + 0.5 howImportantIsDistance
/// (max - d) / max), truncated
[[nodiscard]] uint32_t Score(uint8_t type, bool reactsToType, uint32_t priority, float distance);

/// The switch from the reaction a Living takes to a new one: the new one scores more
/// (the table's restart flag taken as 1) and the current one lasted h = max(10, cur / new x 20 - 10) seconds (1
/// second if the current one is 16 REACT_TO_HAND_PICK_UP); `seconds` = (turn - its record's turn) / 10
[[nodiscard]] bool MaySwitch(float currentScore, float newScore, float seconds, uint8_t currentType);

/// On a switch to a new reaction: the record of the type gets the turn; without one a new record {type, now} is made
/// and, when the list already holds 3, its head (the oldest) is dropped first. No "again" test and no 1800-turn
/// pruning (unlike Records). The new record goes at the tail
void SetReactionDoneWhen(entt::entity living, uint8_t type, uint32_t now);
/// The distance of the reaction a Living already follows, from the Living's MapCoords to the centre of the map cell
/// of the reaction's position (its high words; as AnimalFlee.cpp does for the animals). 0 when the initiator has no
/// position (openblack, guard)
[[nodiscard]] float DistanceToReactionCell(entt::entity living, const Reaction& reaction);
/// The standard turns to react (most types use it): (int)((1 + 0.5 x (R - d) / R x howImportantIsDistance) x
/// numGameTurnsForNormalThingsToReact), R = 10 + maxReactionDistance; every step in float precision
[[nodiscard]] uint32_t StandardTurnsToReact(uint8_t type, float distance);
/// The standard turns before reacting again: the same with
/// numGameTurnsForNormalThingsBeforeReactingAgain
[[nodiscard]] uint32_t StandardTurnsBeforeReactingAgain(uint8_t type, float distance);
/// The type table's flag: a Living may switch to another reaction of the same type. True only for 10 REACT_TO_FIRE,
/// 28 and 35
[[nodiscard]] bool SameTypeSwitch(uint8_t type);

/// The game turn (game_clock::Turn): the one clock of the reactions' stamps and of every Living's
/// records (villagers and animals)
[[nodiscard]] uint32_t Turn();
/// The start of a game turn (before the Living): the reactions whose initiator was deleted go (in the original when
/// the object is deleted; here once per turn, inferred)
void BeginTurn();
/// The end of the game turn's logic: from here to the next BeginTurn a spread rebuilds the map cells first (the map
/// script and the debug hooks create reactions outside the turn, when the cells are not up to date)
void EndTurn();
/// A land is loaded: no reactions, outside a turn (the handlers stay; the turn is the game clock's)
void Clear();
} // namespace openblack::ecs::effects::reactions
