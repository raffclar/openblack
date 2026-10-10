/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

namespace openblack::ecs::systems
{

/// The players who play on this computer, each with a profile of their own
class PlayerProfileSystemInterface
{
public:
	virtual ~PlayerProfileSystemInterface() = default;

	/// How many player profiles there are
	[[nodiscard]] virtual size_t GetProfileCount() const = 0;
	/// Whether the current profile's creature has been kept: its mind was saved as the player left a land
	[[nodiscard]] virtual bool CurrentProfileHasCreature() const = 0;
};

} // namespace openblack::ecs::systems
