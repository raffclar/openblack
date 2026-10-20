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
/// The real time since the game started, in milliseconds
uint32_t NowMs()
{
	return static_cast<uint32_t>(Locator::time::value().GetElapsedTime().count());
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
	const audio::SilentPlayback playback {
	    .startMs = NowMs(),
	    .lengthMs = Locator::resources::value().GetSounds().Handle(sample->sound)->duration * 1000.0f,
	};
	_voices.Add(voice, *sample, emitter, playback, audio.IsDeviceLost());
	// Each spoken line goes into the log, so that you can examine a run that has no sound
	SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Help text {} is said: bank {} sample {}, voice {}, {} ms", text,
	                   static_cast<int>(sample->bank), sample->sample, static_cast<int>(voice), playback.lengthMs);
}

bool HelpSpeechSystem::IsSaying(uint32_t text, audio::SpeechVoice voice)
{
	const auto sample = _table.Find(text);
	if (!sample)
	{
		return false;
	}
	auto& audio = Locator::audio::value();
	const bool lost = audio.IsDeviceLost();
	const auto now = NowMs();
	return _voices.IsSaying(voice, *sample, [&audio, lost, now](entt::entity emitter, const audio::SilentPlayback& playback) {
		// Not yet started or paused with the game is still saying it
		const bool sounding = audio.EmitterExists(emitter) && audio.GetStatus(emitter) != audio::AudioStatus::Stopped;
		return audio::PlaysOn(sounding, lost, playback, now);
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
