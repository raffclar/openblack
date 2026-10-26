/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "HelpSpeechSystem.h"

#include <utility>

#include <spdlog/spdlog.h>

#include "Audio/AudioManagerInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
/// Whether the length of its sample times a spoken line, not the sound device
bool TimedByLength(const audio::AudioManagerInterface& audio)
{
	return audio::TimedByLength(audio.IsDeviceLost(), Locator::time::value().GetFixedFrameTime().has_value());
}
} // namespace

void HelpSpeechSystem::SetTable(audio::HelpSpeechTable table)
{
	_table = std::move(table);
	_voices.Clear();
}

void HelpSpeechSystem::Say(uint32_t text, audio::SpeechVoice voice, std::optional<glm::vec3> position)
{
	const auto sample = _table.Find(text);
	if (!sample)
	{
		return;
	}
	if (!Locator::resources::value().GetSounds().Contains(sample->sound))
	{
		return;
	}
	// Placed only when the script gives a position, and it stays there. The voice rules know the line by its voice.
	auto& audio = Locator::audio::value();
	const auto emitter = audio.StartSoundEffect(
	    sample->sound,
	    {.position = position, .voiceOwner = audio::VoiceOwner::Number, .ownerNumber = audio::OwnerNumberOf(voice)});
	// The scripts time the line by the length of its sample on their own clock, whatever the sound device does
	const audio::SilentPlayback playback {
	    .startMs = _clockMs,
	    .lengthMs = Locator::resources::value().GetSounds().Handle(sample->sound)->duration * 1000.0f,
	};
	_voices.Add(voice, *sample, emitter, playback, TimedByLength(audio));
	// Each spoken line goes into the log, so that you can examine a run that has no sound
	SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Help text {} is said at turn {}: bank {} sample {}, voice {}, {} ms", text,
	                   Locator::time::value().GetTurn(), static_cast<int>(sample->bank), sample->sample,
	                   static_cast<int>(voice), playback.lengthMs);
}

bool HelpSpeechSystem::IsSaying(uint32_t text, audio::SpeechVoice voice)
{
	const auto sample = _table.Find(text);
	if (!sample)
	{
		return false;
	}
	const auto now = _clockMs;
	return _voices.IsSaying(voice, *sample, [now](entt::entity /*emitter*/, const audio::SilentPlayback& playback) {
		return playback.IsPlaying(now);
	});
}

size_t HelpSpeechSystem::GetSpokenTextCount() const
{
	return _table.GetSpokenCount();
}

void HelpSpeechSystem::Stop(audio::SpeechVoice voice, audio::SpeechBank bank)
{
	auto& audio = Locator::audio::value();
	_voices.Stop(voice, bank, [&audio](entt::entity emitter) {
		if (audio.EmitterExists(emitter))
		{
			audio.StopEmitter(emitter);
		}
	});
}
