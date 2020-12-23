/*****************************************************************************
 * Copyright (c) 2018-2020 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

extern "C" {
#include <AL/al.h>
#include <AL/alc.h>
}

#include <spdlog/spdlog.h>

#include "AudioPlayer.h"
#include <string>
#include <vector>

namespace openblack::audio
{

class OpenAlPlayer: public AudioPlayer
{
public:
	OpenAlPlayer();
	void CleanUp(std::map<AudioEmitterId, AudioEmitter>& emitters) override;
	std::string GetName() const override  { return "OpenAL Player"; };
	float GetVolume() const override { return _volume; };
	void SetVolume(float volume) override;
	void OpenAlPlayer::SetListenerPosition(glm::vec3 pos, glm::vec3 vel, glm::vec3 front, glm::vec3 up) const override;

	void SetupEmitter(AudioEmitter& emitter, Sound& sound) const override;
	void PlayEmitter(AudioEmitter& emitter) const override;
	void CleanUpEmitter(AudioEmitter& emitter) const override;
	void StopEmitter(AudioEmitter& emitter) const override;
	float GetAudioProgress(AudioEmitter& emitter) const override;

	void SetVolume(AudioSourceId id, float volume) override;
	float GetVolume(AudioSourceId id) const override;
	const AudioStatus GetAudioStatus(AudioSourceId id) const override;
private:
	void Stop(AudioSourceId sourceId, AudioBufferId bufferId) const;
	void Destroy(AudioSourceId sourceId, AudioBufferId bufferId) const;
	static void DeleteAlDevice(ALCdevice* device);
	static void DeleteAlContext(ALCcontext* context);
	std::unique_ptr<ALCdevice, decltype(&DeleteAlDevice)> _device;
	std::unique_ptr<ALCcontext, decltype(&DeleteAlContext)> _context;
	float _volume;
};
} // namespace openblack::audio