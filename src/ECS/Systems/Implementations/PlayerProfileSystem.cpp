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

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <system_error>
#include <tuple>
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

std::optional<std::filesystem::path> PlayerProfileSystem::SavedGamesFolder() const
{
	const auto* profile = player_profiles::Current(_profiles);
	if (profile == nullptr || !_store.folder.has_value())
	{
		return std::nullopt;
	}
	// Each player has a folder in the profile folder, with the name that the game makes from the name of the profile
	return *_store.folder / player_profiles::ProfileKey(gui::ToUtf16(profile->name)) / "Saved Games";
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

player_profiles::Profile* PlayerProfileSystem::CurrentProfile()
{
	const auto found = std::ranges::find(_profiles.profiles, _profiles.current, &player_profiles::Profile::name);
	return found != _profiles.profiles.end() ? &*found : nullptr;
}

bool PlayerProfileSystem::SelectProfile(std::u16string_view name)
{
	if (!player_profiles::Select(_profiles, name))
	{
		return false;
	}
	if (const auto logger = spdlog::get("game"); logger != nullptr)
	{
		SPDLOG_LOGGER_INFO(logger, "Playing as the player profile \"{}\"", _profiles.current);
	}
	Save();
	return true;
}

bool PlayerProfileSystem::DeleteProfile(std::u16string_view name)
{
	const auto* profile = player_profiles::Find(_profiles, name);
	const auto creatureFile = profile != nullptr ? profile->creatureFile : std::string {};
	if (!player_profiles::Remove(_profiles, name))
	{
		return false;
	}
	// The profile's creature goes with it: its mind file is deleted, but its physique file beside it stays, as in the
	// game
	if (_store.creatureMinds.has_value() && !creatureFile.empty())
	{
		std::error_code error;
		std::filesystem::remove(*_store.creatureMinds / creatureFile, error);
	}
	if (const auto logger = spdlog::get("game"); logger != nullptr)
	{
		SPDLOG_LOGGER_INFO(logger, "Deleted the player profile \"{}\"", gui::ToUtf8(name));
	}
	Save();
	return true;
}

void PlayerProfileSystem::EditCurrentProfile(std::u16string_view creatureName, int symbol)
{
	auto* profile = CurrentProfile();
	if (profile == nullptr)
	{
		return;
	}
	const auto before = *profile;
	player_profiles::Edit(*profile, creatureName, symbol);
	if (*profile != before)
	{
		Save();
	}
}

void PlayerProfileSystem::SetCurrentSettings(const player_profiles::Settings& settings)
{
	auto* profile = CurrentProfile();
	if (profile == nullptr || profile->settings == settings)
	{
		return;
	}
	profile->settings = settings;
	Save();
}

void PlayerProfileSystem::SetCurrentAlignment(double alignment)
{
	auto* profile = CurrentProfile();
	if (profile == nullptr || profile->alignment == alignment)
	{
		return;
	}
	profile->alignment = alignment;
	Save();
}

void PlayerProfileSystem::StartPlaying(int64_t now)
{
	_gameStarted = now;
}

void PlayerProfileSystem::StopPlaying(int64_t now)
{
	auto* profile = CurrentProfile();
	if (profile == nullptr)
	{
		return;
	}
	profile->statistics.secondsPlayed = player_profiles::TimePlayed(profile->statistics.secondsPlayed, _gameStarted, now);
	_gameStarted = now;
	Save();
}

uint32_t PlayerProfileSystem::TimePlayed(int64_t now) const
{
	const auto* profile = player_profiles::Current(_profiles);
	return player_profiles::TimePlayed(profile != nullptr ? profile->statistics.secondsPlayed : 0u, _gameStarted, now);
}

void PlayerProfileSystem::CountSave()
{
	if (auto* profile = CurrentProfile(); profile != nullptr)
	{
		++profile->statistics.saves;
	}
}

void PlayerProfileSystem::CountLoad()
{
	if (auto* profile = CurrentProfile(); profile != nullptr)
	{
		++profile->statistics.loads;
	}
}

uint32_t PlayerProfileSystem::TakeLandAutosave(uint32_t land)
{
	auto* profile = CurrentProfile();
	if (profile == nullptr)
	{
		return 0;
	}
	const auto count = profile->landAutosaves[land];
	profile->landAutosaves[land] = count + 1;
	return count;
}

std::function<bool()> PlayerProfileSystem::ProfilesFileJob() const
{
	if (_store.developer || !_store.folder.has_value())
	{
		return {};
	}
	// The job keeps a copy of the text, so the profiles can change before it runs
	return [folder = *_store.folder, text = player_profiles::Write(_profiles)]() {
		std::error_code error;
		std::filesystem::create_directories(folder, error);
		const auto path = folder / k_ProfilesFile;
		std::ofstream stream(path, std::ios::binary | std::ios::trunc);
		stream << text;
		if (const auto logger = spdlog::get("game"); !stream && logger != nullptr)
		{
			SPDLOG_LOGGER_WARN(logger, "The player profiles could not be written to {}", path.string());
		}
		return static_cast<bool>(stream);
	};
}

void PlayerProfileSystem::Save() const
{
	if (const auto job = ProfilesFileJob(); job)
	{
		std::ignore = job();
	}
}
