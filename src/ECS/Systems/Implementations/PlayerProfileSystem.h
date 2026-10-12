/*******************************************************************************
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
#include <string>
#include <string_view>

#include "Common/PlayerProfiles.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

/// The profiles are kept in a file of openblack's own folder, never the game's, and each profile's creature in the
/// creature-mind folder beside it. A developer's run plays as a profile of its own, never kept
class PlayerProfileSystem final: public PlayerProfileSystemInterface
{
public:
	/// What a new profile is made from: the name of the computer's account, and the system clock in milliseconds
	struct Machine
	{
		std::function<std::u16string()> accountName;
		std::function<uint32_t()> ticks;
	};

	/// The profiles' file in the store's folder
	static constexpr std::string_view k_ProfilesFile = "Profiles.txt";

	explicit PlayerProfileSystem(player_profiles::Store store = {});
	PlayerProfileSystem(player_profiles::Store store, Machine machine);

	[[nodiscard]] size_t GetProfileCount() const final { return _profiles.profiles.size(); }
	[[nodiscard]] bool CurrentProfileHasCreature() const final;
	[[nodiscard]] std::optional<player_profiles::Profile> GetCurrentProfile() const final;
	[[nodiscard]] std::optional<std::string> CreatureFile() const final;
	[[nodiscard]] player_profiles::NameCheck CheckNewName(std::u16string_view name) const final;
	void CreateProfile(std::u16string_view name, int symbol) final;
	[[nodiscard]] const player_profiles::Profiles& GetProfiles() const final { return _profiles; }
	bool SelectProfile(std::u16string_view name) final;
	bool DeleteProfile(std::u16string_view name) final;
	void EditCurrentProfile(std::u16string_view creatureName, int symbol) final;
	void SetCurrentSettings(const player_profiles::Settings& settings) final;

	/// The computer's own: its account's name, and the milliseconds since it started
	[[nodiscard]] static Machine ThisMachine();

private:
	void Save() const;
	[[nodiscard]] player_profiles::Profile* CurrentProfile();

	player_profiles::Store _store;
	Machine _machine;
	player_profiles::Profiles _profiles;
};

} // namespace openblack::ecs::systems
