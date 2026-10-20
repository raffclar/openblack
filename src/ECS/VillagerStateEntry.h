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

namespace openblack::ecs
{

/// What the entry of a villager state says when the villager goes into the state
struct VillagerStateEntry
{
	enum class Outcome : uint8_t
	{
		/// The state does not take the villager: nothing changes
		Refused,
		/// The state takes the villager
		Entered,
		/// The entry sent the villager on to other states itself: the change counts as done, but the state is not set
		SentOn,
	};

	/// A yes or no from an entry that only takes or refuses the villager
	// NOLINTNEXTLINE(google-explicit-constructor): entries that give a yes or no fill the same table
	constexpr VillagerStateEntry(bool entered)
	    : outcome(entered ? Outcome::Entered : Outcome::Refused)
	{
	}
	// NOLINTNEXTLINE(google-explicit-constructor): an entry gives its outcome as it is
	constexpr VillagerStateEntry(Outcome result)
	    : outcome(result)
	{
	}

	[[nodiscard]] constexpr bool Refused() const { return outcome == Outcome::Refused; }
	[[nodiscard]] constexpr bool SentOn() const { return outcome == Outcome::SentOn; }

	Outcome outcome;
};

} // namespace openblack::ecs
