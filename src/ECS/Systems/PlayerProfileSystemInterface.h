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

#include <filesystem>
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
	/// The folder the current profile's saved games are kept in, as the file of what crosses between lands; none without
	/// a profile or a place to keep it
	[[nodiscard]] virtual std::optional<std::filesystem::path> SavedGamesFolder() const { return std::nullopt; }
	/// Whether a new profile may take the name
	[[nodiscard]] virtual player_profiles::NameCheck CheckNewName(std::u16string_view name) const = 0;
	/// Makes a profile of the name with the player's symbol, plays as it and keeps it. The name must have been accepted
	virtual void CreateProfile(std::u16string_view name, int symbol) = 0;
	/// Every profile, in the order the Players page lists them, and the one played
	[[nodiscard]] virtual const player_profiles::Profiles& GetProfiles() const = 0;
	/// Plays as the profile of the name from now on, and keeps that choice; false when no profile has the name
	virtual bool SelectProfile(std::u16string_view name) = 0;
	/// Forgets the profile of the name for good; false when no profile has the name. Forgetting the one played leaves
	/// none played
	virtual bool DeleteProfile(std::u16string_view name) = 0;
	/// Keeps the creature's name and the symbol the player gave on the Players page with the profile played
	virtual void EditCurrentProfile(std::u16string_view creatureName, int symbol) = 0;
	/// Keeps the options the player set with the profile played
	virtual void SetCurrentSettings(const player_profiles::Settings& settings) = 0;
};

} // namespace openblack::ecs::systems
