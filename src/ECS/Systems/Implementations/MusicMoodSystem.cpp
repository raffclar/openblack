/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "MusicMoodSystem.h"

#include <filesystem>
#include <vector>

#include <spdlog/spdlog.h>

#include "Common/MachineClock.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ProfileFolderSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Sound/CustomMusicPlayerInterface.h"
#include "Sound/SoundRequestInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
/// The folder of the custom music, in the profile folder
constexpr std::string_view k_CustomMusicFolder = "custom_music";

audio::CustomMusicPlayerInterface* Player()
{
	return Locator::sounds::has_value() ? Locator::sounds::value().GetCustomMusic() : nullptr;
}
} // namespace

void MusicMoodSystem::LoadFolder()
{
	auto* player = Player();
	if (player == nullptr || !Locator::profileFolderSystem::has_value() || !Locator::filesystem::has_value())
	{
		return;
	}
	const auto folder = Locator::profileFolderSystem::value().MakeFolder(k_CustomMusicFolder);
	std::vector<std::filesystem::path> files;
	if (folder.has_value())
	{
		Locator::filesystem::value().Iterate(*folder, false,
		                                     [&files](const std::filesystem::path& file) { files.push_back(file); });
	}
	player->SetFiles(files);
	if (const auto logger = spdlog::get("audio"); logger != nullptr)
	{
		logger->info("Custom music: {} tracks in {}", player->GetTracks().size(),
		             folder.has_value() ? folder->string() : std::string("no folder"));
	}
}

void MusicMoodSystem::UpdateOnGameTurn(bool reset)
{
	const auto player = []() -> std::optional<uint32_t> {
		const auto* player = Player();
		// The music moods work only while the custom music player has tracks
		if (player == nullptr || !player->IsRunning())
		{
			return std::nullopt;
		}
		// A track counts only while it plays
		if (player->GetPlayState() != audio::CustomMusicPlayerInterface::PlayState::Playing)
		{
			return 0u;
		}
		return music_mood::TrackId(player->GetSongName());
	};
	const auto listen = music_mood::ForGame(NetworkGame(), player);
	if (const auto packet = _controller.Turn(music_mood::k_TurnSeconds, reset, listen, Feelings(), machine_clock::UnixTime());
	    packet.has_value())
	{
		Tell(*packet);
	}
}

void MusicMoodSystem::ReadRecords(const std::optional<std::vector<music_mood::Record>>& records, bool own)
{
	Tell(_controller.ReadRecords(records, own, Feelings()));
}

std::vector<music_mood::Record> MusicMoodSystem::WriteRecords()
{
	return _controller.WriteRecords(Feelings());
}

std::vector<music_mood::Record> MusicMoodSystem::GetRecords() const
{
	const auto& records = _controller.GetRecords();
	return {records.begin(), records.end()};
}

bool MusicMoodSystem::NetworkGame()
{
	return Locator::time::has_value() && Locator::time::value().IsNetworkGame();
}

std::optional<entt::entity> MusicMoodSystem::LocalCreature()
{
	if (!Locator::playerSystem::has_value())
	{
		return std::nullopt;
	}
	const auto& players = Locator::playerSystem::value();
	const auto creature = players.GetPrimaryCreature(
	    (Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE));
	if (!creature.has_value() || !Locator::entitiesRegistry::value().Valid(*creature))
	{
		return std::nullopt;
	}
	return creature;
}

std::optional<music_mood::Emotion> MusicMoodSystem::Feelings()
{
	const auto creature = LocalCreature();
	if (!creature.has_value() || !Locator::creatureMindSystem::has_value())
	{
		return std::nullopt;
	}
	float fear = 0.0f;
	const auto* mind = Locator::entitiesRegistry::value().TryGet<const components::CreatureMindState>(*creature);
	if (mind != nullptr && mind->desires.has_value())
	{
		fear = (*mind->desires)[creature_desires::Desire::Fear].value;
	}
	return music_mood::Emotion {.mood = Locator::creatureMindSystem::value().GetAverageFeedback(), .fear = fear};
}

void MusicMoodSystem::Tell(const music_mood::Packet& packet)
{
	// The change goes to the turns as a command of the player, also when no track starts. The creature reacts at the
	// turn of the command. A network game has no music moods, so the music of this computer does not change the world
	// of the others.
	if (NetworkGame() || !Locator::playerCommandSystem::has_value())
	{
		return;
	}
	const auto creature = LocalCreature();
	if (!creature.has_value())
	{
		return;
	}
	Locator::playerCommandSystem::value().Submit(
	    player_commands::MusicMood {.creature = *creature, .track = packet.id, .mood = packet.emotion.mood});
}

void MusicMoodSystem::DoMusicMood(const player_commands::MusicMood& command)
{
	// Only a command of a track that starts makes the creature react. Then the player's recent feedback starts again.
	// The creature can go from the world before the turn of the command
	if (command.track == 0 || NetworkGame() || !Locator::creatureMindSystem::has_value() ||
	    !Locator::entitiesRegistry::value().Valid(command.creature))
	{
		return;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} hears music track {:08x} with mood {}",
	                    entt::to_integral(command.creature), command.track, command.mood);
	auto& minds = Locator::creatureMindSystem::value();
	minds.ReactToMusic(command.creature, command.mood);
	minds.ClearAverageFeedback();
}
