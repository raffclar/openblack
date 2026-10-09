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

/// A villager's own needs: how much it wants food and rest, how strongly they press it before its town's desires, and
/// the odds that it stops for a second between things. Pure, tested on made-up villagers.
namespace openblack::ecs::villager_needs
{

/// A child is pressed at least this much by its own needs
constexpr float k_ChildLeastTrigger = 0.11f;
/// The lesser of its two needs adds this share of itself to the greater
constexpr float k_LesserNeedShare = 0.5f;
/// A poisoned villager pauses as if it had half its life
constexpr float k_PoisonedLifeShare = 0.5f;
/// How much the life it is missing weighs on its chance to pause
constexpr float k_PauseLifeWeight = 0.5f;

/// How much it wants food: none when full, all when empty, rising as the cube of how empty it is
[[nodiscard]] float DesireForFood(float food);

/// How much it wants rest at a life: none at full life, all at the life at which it goes home hurt or below
[[nodiscard]] float LifeDesireFromLife(float life, float goHomeLife);

/// Whether it is hungry: its food at or under the hungry mark
[[nodiscard]] constexpr bool IsHungry(float food, float hungryForFood)
{
	return food <= hungryForFood;
}

struct TriggerInputs
{
	/// Just woken by a knock on its home: its own needs don't press it for the next thing it does
	bool woken {false};
	bool hungry {false};
	bool child {false};
	float foodDesire {0.0f};
	float lifeDesire {0.0f};
	/// Its need for rest only counts past this
	float ownDesireThreshold {0.0f};
};

/// How strongly its own needs press it, from 0 to 1: its town's desires must be stronger than this, past their own
/// triggers, for it to take them up
[[nodiscard]] float OwnDesiresTrigger(const TriggerInputs& in);

/// Sees to its own needs when either is past a threshold, the stronger first: food when that is the stronger, else
/// rest, falling back to the other. `sleep` and `eat` try them and say whether it went. Whether it went for either.
[[nodiscard]] bool SatisfyOwnDesire(float foodDesire, float lifeDesire, float threshold, const std::function<bool()>& sleep,
                                    const std::function<bool()>& eat);

/// Whether it stops for a second before going into its next state, given a draw from 0 to 1: always a little, and the
/// more the less life it has
[[nodiscard]] bool PausesForASecond(float draw, float life, bool poisoned, float pauseChance);

} // namespace openblack::ecs::villager_needs
