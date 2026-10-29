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

/// Which things the scripts' "get dead at" finds: a villager or an animal that is dead
namespace openblack::script::dead_living
{

/// The game's numbers of the three states of a death
inline constexpr uint8_t k_SetDyingState = 13;
inline constexpr uint8_t k_DyingState = 14;
inline constexpr uint8_t k_DeadState = 15;

/// What the search for the dead looks at in a thing
struct LivingFacts
{
	enum class Kind : uint8_t
	{
		/// A creature or a thing that does not live. The search never finds it.
		Other,
		Villager,
		Animal,
	};
	Kind kind {Kind::Other};
	/// The death of the thing started: a villager died, or a kill hit an animal. The mark stays for all of its life.
	bool deathMark {false};
	/// The game's number of the state that the thing is in now
	uint8_t topState {0};
	/// The game's number of the state that a villager goes to at the end of its state
	uint8_t finalState {0};
	/// The life of an animal, 0 to 1
	float life {1.0f};
};

/// Whether the search for the dead finds the thing:
/// - Only a villager or an animal can be dead. A creature is never dead to this search.
/// - A thing with the death mark is dead.
/// - A thing in the dead state is dead.
/// - A villager is dead in the two states before the dead state, and when its state goes to the dying state.
/// - An animal is dead when its life is exactly 0.
[[nodiscard]] bool IsDead(const LivingFacts& facts) noexcept;

} // namespace openblack::script::dead_living
