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

#include <optional>
#include <string>
#include <string_view>

#include "Common/PlayerProfiles.h"

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
	/// The profile being played, if there is one
	[[nodiscard]] virtual std::optional<player_profiles::Profile> GetCurrentProfile() const = 0;
	/// The file the current profile's creature is kept in between lands, none without a profile
	[[nodiscard]] virtual std::optional<std::string> CreatureFile() const = 0;
	/// Whether a new profile may take the name
	[[nodiscard]] virtual player_profiles::NameCheck CheckNewName(std::u16string_view name) const = 0;
	/// Makes a profile of the name with the player's symbol, plays as it and keeps it. The name must have been accepted
	virtual void CreateProfile(std::u16string_view name, int symbol) = 0;
};

} // namespace openblack::ecs::systems
