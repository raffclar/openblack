/*****************************************************************************
 * Copyright (c) 2018-2020 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "SoundHandler.h"

#include <fstream>
#include <stdexcept>
#include <vector>
#include <random>

namespace openblack::audio
{

SoundHandler::SoundHandler(std::unique_ptr<AudioLoader>&& loader, std::unique_ptr<AudioPlayer>&& player) : _audioLoader(std::move(loader)), _audioPlayer(std::move(player))
{
	addSoundPriority("CRITICAL");         // For example Advisors and hand influence circle noise
	addSoundPriority("GUI");              // GUI and gesture miracle casting
	addSoundPriority("MUSIC");            // Self-explanatory
	addSoundPriority("AMBIENCE");         // High altitude, swamp ambience, mountain ambience
	addSoundPriority("PLAYER");           // Player actions such as miracles, hand movement/actions (tapping water)
	addSoundPriority("CREATURE");         // The creatures get their own priority
	addSoundPriority("GAMEPLAY_AMBIENT"); // "Villagers need food", "Death" wisper, "Birth" wisper, "Need homes"
	_audioPlayer->SetVolume(.5f);
}

void SoundHandler::RegisterSoundPack(std::unique_ptr<SoundPack>& soundPack)
{
	_soundPacks.push_back(std::move(soundPack));
	auto last = _soundPacks.back();
	for (auto& [id, sound] : last->GetSounds()) _soundIdLookup[id] = last;
}

void SoundHandler::Tick()
{
	// De-allocate sound player resources for stopped emitters

	// Update player state


	// Update listener states

	// Update active user emitter states

	// Sort activated emitters

	// Release hardware resources
//	_audioPlayer->CleanUp(_emitters);
	// Assign hardware resources

	// Update emitters with hardware resources

	// Cull emitters stopped emitters
	for (auto iterator = _emitters.begin(); iterator != _emitters.end();)
	{
		auto& [id, emitter] = *iterator;
		auto audioStatus = emitter.audioSourceId;

		if (_audioPlayer->GetAudioStatus(audioStatus) == AudioStatus::Stopped)
		{
			_audioPlayer->CleanUpEmitter(emitter);
			iterator = _emitters.erase(iterator);
		}
		else
			iterator++;
	}
}

std::unique_ptr<Sound>& SoundHandler::GetSound(SoundId id)
{
	auto& sound = _soundIdLookup[id]->GetSound(id);

	if (!sound->loaded)
	{
		_audioLoader->ToPCM16(*sound);
		sound->loaded = true;
	}

	return sound;
}

std::vector<AudioEmitter> SoundHandler::GetActiveEmitters() const
{
	std::vector<AudioEmitter> actives;

	for (auto& [emitterId, emitter] : _emitters)
	{
		actives.push_back(emitter);
	}

	return actives;
}

AudioEmitterId SoundHandler::CreateEmitter(const std::vector<SoundId> ids, glm::vec3& position, glm::vec3& velocity, glm::vec2& radius, float volume, bool world)
{
	std::vector<SoundId> out;
	std::sample(ids.begin(), ids.end(), std::back_inserter(out), 1, std::mt19937{std::random_device{}()});
	return CreateEmitter(out[0], position, velocity, radius, volume, world);
}

void SoundHandler::PlaySound(SoundId id)
{
	auto unusedPosVel = glm::vec3(1);
	auto unusedRadius = glm::vec2(1);
	CreateEmitter(id, unusedPosVel, unusedPosVel, unusedRadius, 1.0, false);
}

AudioEmitterId SoundHandler::CreateEmitter(SoundId id, glm::vec3& position, glm::vec3& velocity, glm::vec2& radius, float volume, bool world)
{
	auto& sound = GetSound(id);
	AudioEmitterId emitterId = _emitters.size();
	AudioEmitter emitter = { 0, 0, id, 0, position, velocity, radius, volume, sound->loopType, world};
	_audioPlayer->SetupEmitter(emitter, *sound);
	_emitters[emitterId] = emitter;

	_audioPlayer->PlayEmitter(_emitters[emitterId]);
	return emitterId;
}

void SoundHandler::DestroyEmitter(AudioEmitterId id) {
	_audioPlayer->StopEmitter(_emitters[id]);
	_emitters.erase(id);
}

void SoundHandler::addSoundPriority(std::string name)
{
	_soundPriorities[name] = _soundPriorities.size();
}

int SoundHandler::GetSoundPriority(std::string name)
{
	return _soundPriorities[name];
}

void SoundHandler::SetGlobalVolume(float volume) {
	_audioPlayer->SetVolume(volume);
}

std::unique_ptr<SoundHandler> createSoundHandler()
{
	auto converter = std::make_unique<FfmpegLoader>();
	auto player = std::make_unique<OpenAlPlayer>();
	return std::make_unique<SoundHandler>(std::move(converter), std::move(player));
}
} // namespace openblack::audio
