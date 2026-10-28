/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ScriptMusicSystem.h"

#include <chrono>
#include <exception>
#include <string>

#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>

#include "Audio/AudioManagerInterface.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack::ecs::systems;

void ScriptMusicSystem::ProcessTurn()
{
	// The music plays for the real time of a turn at the game's speed, whatever time the frames take
	const auto speed = Locator::time::has_value() ? Locator::time::value().GetSpeed() : 1.0f;
	const auto turnMicroseconds =
	    std::chrono::duration_cast<std::chrono::microseconds>(TimeSystemInterface::k_TurnDuration).count();
	const auto turnTime =
	    std::chrono::microseconds(static_cast<int64_t>(static_cast<float>(turnMicroseconds) / (speed > 0.0f ? speed : 1.0f)));
	_music.Turn(turnTime, [this](audio::MusicType type) { return Timing(type); });
}

void ScriptMusicSystem::Reset()
{
	_music.Restore(audio::MusicType::None, false, _music.GetLyrics());
}

bool ScriptMusicSystem::HasMusic() const
{
	const bool soundDevice = Locator::audio::has_value() && Locator::audio::value().IsInstalled();
	const bool networkGame = Locator::time::has_value() && Locator::time::value().IsNetworkGame();
	const bool dedicatedServer = Locator::config::has_value() && Locator::config::value().dedicatedServer;
	return audio::WorldHasMusic(soundDevice, networkGame, dedicatedServer);
}

std::shared_ptr<const openblack::audio::MusicTiming> ScriptMusicSystem::Timing(audio::MusicType type) const
{
	const auto path = std::string(audio::GetMusicBankPath(type));
	// Without the music, no bank is there to play, as in a game that started with no sound device
	if (!_banksLoaded || path.empty() || !HasMusic() || !Locator::resources::has_value())
	{
		return nullptr;
	}
	auto& timings = Locator::resources::value().GetMusicTimings();
	const auto id = entt::hashed_string(path.c_str()).value();
	if (!timings.Contains(id))
	{
		try
		{
			if (Locator::filesystem::value().Exists(Locator::filesystem::value().FindPath(path)))
			{
				timings.Load(id, resources::MusicTimingLoader::FromDiskTag {}, path);
			}
		}
		catch (const std::exception& error)
		{
			if (const auto logger = spdlog::get("audio"); logger != nullptr)
			{
				SPDLOG_LOGGER_WARN(logger, "The music bank {} does not read: {}", path, error.what());
			}
		}
		if (!timings.Contains(id))
		{
			timings.Load(id, resources::MusicTimingLoader::EmptyTag {});
		}
	}
	return timings.Handle(id).handle();
}

void ScriptMusicSystem::SaveState(reflection::BinaryWriter& out) const
{
	reflection::Save(out, _music.GetType());
	reflection::Save(out, _music.IsStarted());
	reflection::Save(out, _music.GetLyrics().line);
	reflection::Save(out, _music.GetLyrics().word);
}

bool ScriptMusicSystem::LoadState(reflection::BinaryReader& in)
{
	auto type = audio::MusicType::None;
	bool started = false;
	audio::MusicLyrics lyrics;
	reflection::Load(in, type);
	reflection::Load(in, started);
	reflection::Load(in, lyrics.line);
	reflection::Load(in, lyrics.word);
	if (in.Failed())
	{
		return false;
	}
	_music.Restore(type, started, lyrics);
	return true;
}
