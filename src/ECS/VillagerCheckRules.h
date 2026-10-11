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

#include "Enums.h"

/// When a villager's needs are looked at, and what the looks decide. Every turn a villager loses the life its state
/// costs; in states that look at its needs, one with no life left dies, and every so often its age, wounds, growing up
/// and belly are looked at. Pure, tested on made-up villagers.
namespace openblack::ecs::villager_check_rules
{

/// Its age is looked at about once in this many turns, each villager at its own time
constexpr uint32_t k_OldAgeCheckTurns = 800;

/// Which state's life cost a villager pays this turn: a moving state's own, else the state it works towards; and which
/// state's table row decides whether its needs are looked at: the other one
struct DrainStates
{
	VillagerStates drain;
	VillagerStates checks;
};
[[nodiscard]] constexpr DrainStates DrainAndCheckStates(VillagerStates top, VillagerStates final, VillagerStates workingTowards,
                                                        bool topMovesWithGround)
{
	return topMovesWithGround ? DrainStates {.drain = top, .checks = final}
	                          : DrainStates {.drain = workingTowards, .checks = top};
}

/// Whether its needs are due a look: more turns than the gap between looks have passed since the last
[[nodiscard]] constexpr bool ChecksDue(uint32_t turnsSinceChecked, uint32_t checkGap)
{
	return turnsSinceChecked > checkGap;
}

/// Whether its age is due a look at this check: its own turn in every few hundred fell since the last check
[[nodiscard]] constexpr bool OldAgeDue(uint32_t turn, uint32_t ownPhase, uint32_t turnsSinceChecked)
{
	return (turn + ownPhase) % k_OldAgeCheckTurns < turnsSinceChecked;
}

/// What decides whether a wounded villager is sent home
struct WoundedInputs
{
	float life {1.0f};
	/// Under this life it goes home
	float goHomeLife {0.3f};
	bool atHome {false};
	/// Its state sends the wounded home, and doesn't keep them where they are
	bool stateSendsHome {false};
	bool stateKeepsWounded {false};
	/// It is on its way to food it found, and how full it is against the hungry mark: a hungry one goes on to the food
	bool goingForFood {false};
	float food {1.0f};
	float hungryForFood {0.5f};
};
/// Whether it goes home to recover
[[nodiscard]] constexpr bool WoundedGoesHome(const WoundedInputs& in)
{
	return in.life < in.goHomeLife && !in.atHome && in.stateSendsHome && !in.stateKeepsWounded &&
	       (!in.goingForFood || in.hungryForFood < in.food);
}

/// Whether a death at a check counts as from chanting: it was going home or to sleep from worship
[[nodiscard]] constexpr bool ChantedLast(VillagerStates workingTowards)
{
	return workingTowards == VillagerStates::GoHomeFromWorship || workingTowards == VillagerStates::ArrivesHomeFromWorship ||
	       workingTowards == VillagerStates::SleepInTentFromWorship;
}

/// What a villager with no life left at a check dies of
[[nodiscard]] constexpr DeathReason ExhaustionDeath(bool chanted)
{
	return chanted ? DeathReason::Chant : DeathReason::Exhaustion;
}

} // namespace openblack::ecs::villager_check_rules
