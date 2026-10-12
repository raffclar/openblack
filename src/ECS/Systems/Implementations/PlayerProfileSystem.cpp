/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "PlayerProfileSystem.h"

#include <cstdlib>

#include <chrono>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

#include <spdlog/spdlog.h>

#include "Gui/TextDatabase.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
/// A developer's run plays as this profile, never kept
player_profiles::Profile DeveloperProfile()
{
	return {.name = std::string(player_profiles::k_DeveloperName),
	        .creatureFile = std::string(player_profiles::k_DeveloperCreatureFile),
	        .creatureName = std::string(player_profiles::k_DeveloperName),
	        .symbol = 0};
}
} // namespace

PlayerProfileSystem::Machine PlayerProfileSystem::ThisMachine()
{
	return {.accountName =
	            []() {
		            for (const char* variable : {"USERNAME", "USER"})
		            {
			            // NOLINTNEXTLINE(concurrency-mt-unsafe): read on the game's thread
			            if (const char* name = std::getenv(variable); name != nullptr && *name != '\0')
			            {
				            return gui::ToUtf16(name);
			            }
		            }
		            return std::u16string {};
	            },
	        .ticks =
	            []() {
		            const auto now = std::chrono::steady_clock::now().time_since_epoch();
		            return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
	            }};
}

PlayerProfileSystem::PlayerProfileSystem(player_profiles::Store store)
    : PlayerProfileSystem(std::move(store), ThisMachine())
{
}

PlayerProfileSystem::PlayerProfileSystem(player_profiles::Store store, Machine machine)
    : _store(std::move(store))
    , _machine(std::move(machine))
{
	if (_store.developer)
	{
		const auto profile = DeveloperProfile();
		_profiles = {.profiles = {profile}, .current = profile.name};
		return;
	}
	if (!_store.folder.has_value())
	{
		return;
	}
	std::ifstream stream(*_store.folder / k_ProfilesFile, std::ios::binary);
	if (!stream)
	{
		return;
	}
	const std::string text {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
	_profiles = player_profiles::Read(text);
}

bool PlayerProfileSystem::CurrentProfileHasCreature() const
{
	const auto file = CreatureFile();
	if (!file.has_value() || !_store.creatureMinds.has_value())
	{
		return false;
	}
	std::error_code error;
	return std::filesystem::exists(*_store.creatureMinds / *file, error);
}

std::optional<player_profiles::Profile> PlayerProfileSystem::GetCurrentProfile() const
{
	if (const auto* profile = player_profiles::Current(_profiles); profile != nullptr)
	{
		return *profile;
	}
	return std::nullopt;
}

std::optional<std::string> PlayerProfileSystem::CreatureFile() const
{
	if (const auto* profile = player_profiles::Current(_profiles); profile != nullptr)
	{
		return profile->creatureFile;
	}
	return std::nullopt;
}

player_profiles::NameCheck PlayerProfileSystem::CheckNewName(std::u16string_view name) const
{
	return player_profiles::CheckNewName(_profiles, name);
}

void PlayerProfileSystem::CreateProfile(std::u16string_view name, int symbol)
{
	const auto accountName = _machine.accountName ? _machine.accountName() : std::u16string {};
	const auto ticks = _machine.ticks ? _machine.ticks() : 0u;
	const auto& made = player_profiles::Add(_profiles, name, accountName, player_profiles::CreatureFileName(ticks), symbol);
	// The profile made is the one played from now on
	_profiles.current = made.name;
	if (const auto logger = spdlog::get("game"); logger != nullptr)
	{
		SPDLOG_LOGGER_INFO(logger, "Made the player profile \"{}\", its creature to be kept in {}", made.name,
		                   made.creatureFile);
	}
	Save();
}

void PlayerProfileSystem::Save() const
{
	if (_store.developer || !_store.folder.has_value())
	{
		return;
	}
	std::error_code error;
	std::filesystem::create_directories(*_store.folder, error);
	const auto path = *_store.folder / k_ProfilesFile;
	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	stream << player_profiles::Write(_profiles);
	if (const auto logger = spdlog::get("game"); !stream && logger != nullptr)
	{
		SPDLOG_LOGGER_WARN(logger, "The player profiles could not be written to {}", path.string());
	}
}
