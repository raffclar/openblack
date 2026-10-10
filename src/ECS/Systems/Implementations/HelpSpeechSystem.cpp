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

#include "Audio/AudioManagerInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;

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
	const auto emitter = Locator::audio::value().StartSoundEffect(sample->sound, {.position = position});
	_voices.Add(voice, *sample, emitter);
}

bool HelpSpeechSystem::IsSaying(uint32_t text, audio::SpeechVoice voice)
{
	const auto sample = _table.Find(text);
	if (!sample)
	{
		return false;
	}
	auto& audio = Locator::audio::value();
	return _voices.IsSaying(voice, *sample, [&audio](entt::entity emitter) {
		// Not yet started or paused with the game is still saying it
		return audio.EmitterExists(emitter) && audio.GetStatus(emitter) != audio::AudioStatus::Stopped;
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
