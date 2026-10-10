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

namespace openblack::help::profile
{
class HelpProfile;
}

namespace openblack::ecs::systems
{

/// The help system's profile of what the player has done, kept for the whole session. Nothing is counted, and its clock
/// stands still, while a script's cut scene has the cinema bars in.
class HelpProfileSystemInterface
{
public:
	virtual ~HelpProfileSystemInterface() = default;

	/// The player does one of the things counted
	virtual void Trigger(uint32_t event) = 0;
	/// A game turn ends
	virtual void ProcessTurn() = 0;

	[[nodiscard]] virtual const help::profile::HelpProfile& Get() const = 0;
	[[nodiscard]] virtual help::profile::HelpProfile& Get() = 0;
};

} // namespace openblack::ecs::systems
