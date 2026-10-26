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
	// The clock of the channel, so that the piece continues from its place after a load
	const auto channel = _music.GetChannelState();
	reflection::Save(out, channel.has_value());
	if (!channel.has_value())
	{
		return;
	}
	reflection::Save(out, channel->type);
	reflection::Save(out, channel->loops);
	reflection::Save(out, static_cast<int64_t>(channel->time.count()));
	reflection::Save(out, channel->ticks);
	reflection::Save(out, channel->checked.chunk);
	reflection::Save(out, channel->checked.frame);
	reflection::Save(out, channel->current);
	reflection::Save(out, channel->fadeTicks.has_value());
	reflection::Save(out, channel->fadeTicks.value_or(0));
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
	// A save from before the channel was kept has no clock: the piece then gives no sound and does not end
	if (in.Remaining() == 0)
	{
		return true;
	}
	bool hasChannel = false;
	reflection::Load(in, hasChannel);
	if (!hasChannel)
	{
		return !in.Failed();
	}
	audio::ScriptMusic::ChannelState channel;
	int64_t time = 0;
	bool fades = false;
	uint32_t fadeTicks = 0;
	reflection::Load(in, channel.type);
	reflection::Load(in, channel.loops);
	reflection::Load(in, time);
	reflection::Load(in, channel.ticks);
	reflection::Load(in, channel.checked.chunk);
	reflection::Load(in, channel.checked.frame);
	reflection::Load(in, channel.current);
	reflection::Load(in, fades);
	reflection::Load(in, fadeTicks);
	if (in.Failed())
	{
		return false;
	}
	channel.time = std::chrono::microseconds(time);
	channel.fadeTicks = fades ? std::optional(fadeTicks) : std::nullopt;
	_music.RestoreChannel(channel, [this](audio::MusicType music) { return Timing(music); });
	return true;
}
