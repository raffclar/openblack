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
#include <span>
#include <vector>

#include "Creature/CreatureIdleMind.h"
#include "Creature/CreaturePlanner.h"

/// The creature loses a thing that it uses when the thing goes before the creature completes the work. Then the
/// creature stops, decides again and tells the player that it lost the thing.
namespace openblack::creature_lost_thing
{

/// The things that the creature uses now
struct InUse
{
	/// The things of its plan (what the desire is about, what it acts on, what it uses), when it has a plan with a step
	/// that is not done
	std::vector<uint32_t> planThings;
	/// The things of its steps, from the current step to the last
	std::vector<uint32_t> stepThings;
	/// What its hand holds
	std::optional<uint32_t> held;
	/// The things that the creature itself ate or used up from its hand. The game removes such a thing in the last step
	/// of the agenda, and the plan stops at the same time. Thus the creature does not lose the thing.
	std::vector<uint32_t> usedByItself;
};

/// The things of the plan
[[nodiscard]] std::vector<uint32_t> PlanThings(const creature_planner::Plan& plan);
/// The things of the steps of the agenda, from the step given to the last
[[nodiscard]] std::vector<uint32_t> StepThings(std::span<const creature_mind::Step> agenda, size_t from);

/// Whether the creature lost a thing that it uses:
/// - a thing of its plan or its steps is gone;
/// - the thing in its hand is gone;
/// - a thing of its steps is not there for it, as a dying villager.
[[nodiscard]] bool Lost(const InUse& inUse, const std::function<bool(uint32_t)>& gone,
                        const std::function<bool(uint32_t)>& notThere);

/// Why the creature failed: it lost the thing that it looked for
constexpr uint32_t k_LostThing = 8;

} // namespace openblack::creature_lost_thing
