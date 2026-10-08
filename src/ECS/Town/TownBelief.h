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

#include <functional>
#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

// The town's belief: the per-player block of components::TownBelief, its per-turn fold (a step of the town's process)
// and the once-per-turn pass over every town (a row of the game turn). Adding belief is
// ecs::town_stores::AddToBelief.

namespace openblack
{
class EventManager;
} // namespace openblack

namespace openblack::ecs::components
{
struct Town;
struct TownBelief;
} // namespace openblack::ecs::components

namespace openblack::ecs::town_belief
{
/// The 8 player slots of every row; PlayerNames::NEUTRAL = 7
constexpr size_t k_Players = 8;
/// The REACTION count of the boredom multipliers
constexpr size_t k_Reactions = 41;
/// The TOWN_DESIRE count of the thresholds
constexpr size_t k_Desires = 17;
/// Init: the cap of every player
constexpr float k_InitialCap = 10.0f;
/// Init: every boredom multiplier
constexpr float k_InitialBoredom = 1.0f;
/// The fold's boredom ceiling: a sum below it is stored, otherwise the multiplier stays
constexpr float k_BoredomCeiling = 1.0f;
/// ProcessOncePerTurn runs on turn % 10 == 0
constexpr uint32_t k_ProcessEvery = 10;
/// The belief made an int amount for the belief sprites (DrawBelief, the fold)
constexpr double k_AmountScale = 10000.0;
/// ReduceBelief's draw threshold (a double)
constexpr double k_ReduceDrawThreshold = 0.005;
/// The believers ProcessOncePerTurn needs to say anything
constexpr float k_ToolTipMinimum = 0.001f;
/// The believers of the tooltip and the floating number
constexpr float k_ToolTipScale = 1000.0f;
/// HELP_TEXT: the "believers gained" tooltip, with the count as %3.0f
constexpr uint32_t k_ToolTipBelieversGained = 0xEE1;
/// The belief-sprite queue takes no entry once it holds 400
constexpr size_t k_MaxBeliefSprites = 400;
/// The colour of a DrawBelief without a player
constexpr uint32_t k_NoPlayerColour = 0xFFFFFFFF;
/// The start of RelativeBelief's best difference
constexpr float k_RelativeStart = -9999.0f;
/// RivalRecentRatio's start (the smallest "recent" belief of a rival that counts)
constexpr float k_RivalRecentStart = 0.008f;

// ---- Small methods --------------------------------------------------------------------------------------------------

/// Init: belief, pending, the reduce accumulator and lastAddedTurn 0, the cap 10; belief[neutral] = the town's
/// beliefInNeutralPlayer (raw, no clamp); every boredom multiplier 1.0; the desire thresholds from the town desire
/// info. recent and addedThisPeriod are NOT reset. Callers: the town constructor (before beliefInNeutralPlayer is set,
/// so belief[neutral] is 0 until the first fold) and SetTownEmpty
void Init(components::Town& town);
/// desireThreshold[d] = the town desire info's desireAffectsBeliefAfter (0 without the info)
void ResetDesireThresholds(components::TownBelief& belief);
/// belief[n] = cap[n] < v ? cap[n] : v. No lower clamp
void SetBelief(components::TownBelief& belief, PlayerNames player, float value);
/// belief[n] (0 for a slot out of range: openblack's guard)
[[nodiscard]] float GetBeliefInPlayer(const components::TownBelief& belief, PlayerNames player);
/// The same for a town entity (0 without one)
[[nodiscard]] float GetBeliefInPlayer(entt::entity town, PlayerNames player);
/// cap[P]
void SetCap(components::TownBelief& belief, PlayerNames player, float cap);
[[nodiscard]] float GetCap(const components::TownBelief& belief, PlayerNames player);
/// addedThisPeriod[n]
[[nodiscard]] float GetAddedThisPeriod(const components::TownBelief& belief, PlayerNames player);
/// added[n] == 0 ? 0 : added[n] / (belief[n] x 10). No caller in the original
[[nodiscard]] float GetAddedThisPeriodRatio(const components::TownBelief& belief, PlayerNames player);
/// SET_TOWN_BELIEF: the neutral player -> beliefInNeutralPlayer = f; then SetBelief(P, f)
void SetBeliefInPlayer(components::Town& town, PlayerNames player, float value);
/// belief[n] -= r (no clamp); with a town centre reduceAcc[n] += r and over 0.005 DrawBelief(-reduceAcc[n], centre, P)
/// then reduceAcc[n] = 0. That draw has a negative amount and so never shows anything
void ReduceBelief(entt::entity town, PlayerNames player, float r);
/// b = boredom[reaction] + f; below 0 -> 0
void AddToBoredomMultiplier(components::TownBelief& belief, size_t reaction, float f);
/// A town -> boredom[reaction], none -> 1.0
[[nodiscard]] float GetBoredomMultiplier(entt::entity town, size_t reaction);
/// The largest belief[i], i != n, from 0, strict >
[[nodiscard]] float GetMaxBeliefMeNotIncluded(const components::TownBelief& belief, PlayerNames player);
/// thing's player == P -> the belief of the slot i != P with the largest belief[i] - belief[P] (from -9999, strict >);
/// else belief[thing's player]. It returns belief[i] itself, not the difference
[[nodiscard]] float RelativeBelief(const components::TownBelief& belief, PlayerNames player, PlayerNames thingPlayer);
/// (Used by the computer player) over the slots 0..5 not P and not allied, the largest recent[i] above 0.008 (strict
/// >); none or belief[P] == 0 -> 0; else x = 2 belief[i] / belief[P] and min(x^2, 1). `allied` (pending: openblack has
/// no alliances) may be empty
[[nodiscard]] float RivalRecentRatio(const components::TownBelief& belief, PlayerNames player,
                                     const std::function<bool(PlayerNames)>& allied = {});
/// (Used when villagers are shuffled between abodes) the largest belief of the active non-allied players != P (from 0,
/// strict >) / belief[P]; 0 when belief[P] == 0. "Active" = the slots 0..6 flagged active; (approximate) openblack
/// keeps no active flag: the 7 non-neutral slots. `allied` as above
[[nodiscard]] float BeliefRatioVsRivals(const components::TownBelief& belief, PlayerNames player,
                                        const std::function<bool(PlayerNames)>& allied = {});

// ---- The turn -------------------------------------------------------------------------------------------------------

/// Once per turn per town, a step of the town's process: pending x beliefScale into belief and addedThisPeriod; the
/// boredom; the desires that cost the owner belief; the neutral belief pinned to beliefInNeutralPlayer; recent x 0.997;
/// the conversion to the strongest player
void Fold(entt::entity town);
/// A row of the game turn: on turn % 10 == 0 only, every town's addedThisPeriod back to 0, the local player's sum over
/// 0.001 -> the "believers gained" tooltip with sum x 1000 (help::tooltips::Force) and the yellow floating number
/// (pending), then CheckLosingBelief for the local player. Each player's summed belief goes to its GameStats
/// (game_stats::WorldBelief). Game.cpp calls it every turn
void ProcessOncePerTurn();
/// The "losing belief" help sprite; nothing on land 1
void CheckLosingBelief(PlayerNames player);
/// Town::owner = newP and the empty countdown 0. (pending) the rest of the original take-over: a sound, worship
/// percentage 0, the worship site and citadel unlinked, the magic types, a reaction, and what follows; the move to the
/// tail of the new owner's list is town_process' ProcessPlayers (approximate: TownsOf orders by Town::id). The
/// population goes to the new owner's GameStats (game_stats::TownTakenOver)
void TakeOverTown(PlayerNames player, entt::entity town);
/// The belief part of emptying a town (a step of the town's process): Init; SetBelief(i, beliefInNeutralPlayer) for
/// the 8 slots; an owner other than the neutral player -> TakeOverTown(neutral, town). (pending) not called yet:
/// nobody starts the empty countdown
void SetTownEmpty(entt::entity town);

/// The lost-town scale: SET_LOST_TOWN_SCALE (land script); 1.0 when the land balance is initialised. Kept by
/// land_balance (LostTownScale / SetLostTownScale), reset with it when a map loads
[[nodiscard]] float LostTownScale();

// ---- DrawBelief and the belief sprites ------------------------------------------------------------------------------

/// v = f x 10000 truncated; v <= 0 -> nothing; the point is thing's ground point + its height (uninitialised without a
/// thing in the original, openblack 0); the debug "B n" value spinner is not ported (off in a normal game); colour =
/// P's player colour, else 0xFFFFFFFF; then QueueBeliefSprite
void DrawBelief(float f, entt::entity thing, std::optional<PlayerNames> player);

/// One entry of the belief-sprite queue (the original's first field, an uninitialised local, is left out)
struct BeliefSprite
{
	glm::vec3 position {};
	int32_t amount {0};
	uint32_t colour {0};
};
/// Pushed when the queue holds fewer than 400 (the original's other condition is a constant flag that is always set).
/// The original's `thing` argument is never read
void QueueBeliefSprite(const glm::vec3& position, int32_t amount, uint32_t colour);
/// The belief-sprite renderer takes the LAST entry (LIFO). (pending) openblack has no belief-sprite rule yet: nobody
/// drains the queue, so nothing is drawn
[[nodiscard]] std::optional<BeliefSprite> PopBeliefSprite();
[[nodiscard]] size_t BeliefSpriteCount();

namespace detail
{
/// The help sprites the belief asks for: LosingBelief (list 10), GeneralBad (list 13), GeneralGood (list 14)
enum class HelpSprite
{
	LosingBelief,
	GeneralBad,
	GeneralGood,
};
/// Test hook: the belief-sprite queue emptied
void ClearForTests();
} // namespace detail

/// The game's handlers of the belief events: events::TownBeliefHelp plays the town's help sprite,
/// events::TownBeliefToolTip shows the tooltip. Added once when the game starts
void AddBeliefEventHandlers(EventManager& manager);
} // namespace openblack::ecs::town_belief
