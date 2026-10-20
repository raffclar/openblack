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
#include <cstdint>

#include <filesystem>
#include <functional>
#include <map>
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
	/// Keeps the bindings of the Controls page with the profile played, by the place of each action in the list
	virtual void SetCurrentKeys(const std::map<uint32_t, player_profiles::KeptKey>& keys) = 0;
	/// Keeps how good or evil the player is now with the profile played, for their next new game to start them there
	virtual void SetCurrentAlignment(double alignment) = 0;

	/// The game started at that time of the system clock, in seconds
	virtual void StartPlaying(int64_t now) = 0;
	/// The game ends at that time: the seconds played since it started are added to the profile played, which is kept
	virtual void StopPlaying(int64_t now) = 0;
	/// The seconds the player played, those kept with the profile and those since the game started
	[[nodiscard]] virtual uint32_t TimePlayed(int64_t now) const = 0;
	/// The player saved or loaded a game in the save game room. The profile keeps the count with its next change.
	virtual void CountSave() = 0;
	virtual void CountLoad() = 0;
	/// An autosave on the land: gives how many autosaves the profile played counts for the land, then counts one more.
	/// Zero without a profile. This does not write the profiles file: the save gives its writer the job of
	/// ProfilesFileJob after the files of the save.
	virtual uint32_t TakeLandAutosave(uint32_t land) = 0;
	/// A job that writes the profiles as they are now into their file, on the thread that runs it. None when the game
	/// keeps no file of the profiles.
	[[nodiscard]] virtual std::function<bool()> ProfilesFileJob() const { return {}; }
};

} // namespace openblack::ecs::systems
