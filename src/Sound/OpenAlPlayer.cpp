/*****************************************************************************
 * Copyright (c) 2018-2020 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "OpenAlPlayer.h"

#include <string>

void alErrorCheck(std::string message, std::string file, uint64_t line)
{
	std::string errorMessage;
	ALenum error = alGetError();
	switch (error)
	{
	case AL_NO_ERROR:
		return;
	case AL_INVALID_NAME:
		errorMessage = "AL_INVALID_NAME: a bad name (ID) was passed to an OpenAL function";
		break;
	case AL_INVALID_ENUM:
		errorMessage = "AL_INVALID_ENUM: an invalid enum value was passed to an OpenAL function";
		break;
	case AL_INVALID_VALUE:
		errorMessage = "AL_INVALID_VALUE: an invalid value was passed to an OpenAL function";
		break;
	case AL_INVALID_OPERATION:
		errorMessage = "AL_INVALID_OPERATION: the requested operation is not valid";
		break;
	case AL_OUT_OF_MEMORY:
		errorMessage = "AL_OUT_OF_MEMORY: the requested operation resulted in OpenAL running out of memory";
		break;
	default:
		errorMessage = "UNKNOWN AL ERROR: " + std::to_string(error);
	}

	spdlog::error("OpenAL error: {} with call \"{}\" at file \"{}\" on line {}", error, message, file, line);
}

#define alCheckCall(FUNCTION_CALL)\
	FUNCTION_CALL;\
	alErrorCheck(#FUNCTION_CALL, __FILE__, __LINE__)

using namespace openblack::audio;

OpenAlPlayer::OpenAlPlayer()
	: _device(alcOpenDevice(nullptr), DeleteAlDevice)
	, _context(alcCreateContext(_device.get(), nullptr), DeleteAlContext)
{
	alCheckCall(alcMakeContextCurrent(_context.get()));
	UpdateListenerState(glm::vec3(1), glm::vec3(0), glm::vec3(0), glm::vec3(0));
	_volume = 0;
}

void OpenAlPlayer::SetVolume(float volume) {
	_volume = volume;
}

void OpenAlPlayer::SetVolume(AudioSourceId id, float volume) {
	alCheckCall(alSourcef(id, AL_GAIN, volume));
}

float OpenAlPlayer::GetVolume(AudioSourceId id) const
{
	return _volume;
}

int GetSoundFormat(Sound& sound)
{
	if (sound.format == SoundFormat::MonoS16)
		return AL_FORMAT_MONO16;
	else if (sound.format == SoundFormat::StereoS16)
		return AL_FORMAT_STEREO16;
	else
		throw std::runtime_error("Unknown buffer format");
}

void OpenAlPlayer::SetupEmitter(AudioEmitter& emitter, Sound& sound) const
{
	auto& soundData = sound.bytes;
	auto soundFormat = GetSoundFormat(sound);
	alCheckCall(alGenSources(1, &emitter.audioSourceId));
	alCheckCall(alGenBuffers(1, &emitter.audioBufferId));
	alCheckCall(alBufferData(emitter.audioBufferId, soundFormat, soundData.data(), soundData.size(), sound.sampleRate));
}

void OpenAlPlayer::PlayEmitter(AudioEmitter& emitter) const
{
	auto soundSource = emitter.audioSourceId;
	auto soundBuffer = emitter.audioBufferId;
	UpdateEmitterState(emitter);
	alCheckCall(alSourcef(soundSource, AL_PITCH, 1.f));
	alCheckCall(alSourcef(soundSource, AL_GAIN, 1.f));
	alCheckCall(alSourcei(soundSource, AL_LOOPING, AL_FALSE));
	alCheckCall(alSourcei(soundSource, AL_BUFFER, soundBuffer));
	alCheckCall(alSourcef(soundSource, AL_GAIN, _volume));
	alCheckCall(alSourcePlay(soundSource));
}

void OpenAlPlayer::UpdateEmitterState(AudioEmitter& emitter) const
{
	glm::vec3 pos;
	glm::vec3 vel;

	if (!emitter.world)
	{
		alCheckCall(alGetListener3f(AL_POSITION, &pos.z, &pos.y, &pos.x));
		alCheckCall(alGetListener3f(AL_VELOCITY, &vel.z, &vel.y, &vel.x));
	}
	else
	{
		pos = emitter.position;
		vel = emitter.velocity;
	}

	alCheckCall(alSource3f(emitter.audioSourceId, AL_POSITION, pos.z, pos.y, pos.x));
	alCheckCall(alSource3f(emitter.audioSourceId, AL_VELOCITY, vel.z, vel.y, vel.x));
	alCheckCall(alSourcef(emitter.audioSourceId, AL_GAIN, _volume));
}

void OpenAlPlayer::CleanUpEmitter(AudioEmitter& emitter) const
{
	Destroy(emitter.audioSourceId, emitter.audioBufferId);
	emitter.audioSourceId = 0;
	emitter.audioBufferId = 0;
}

void OpenAlPlayer::StopEmitter(AudioEmitter& emitter) const
{
	Destroy(emitter.audioSourceId, emitter.audioBufferId);
}

void OpenAlPlayer::CleanUp(std::map<AudioEmitterId, AudioEmitter>& emitters)
{
	for (auto iterator = emitters.cbegin(); iterator != emitters.cend(); iterator++)
	{
		auto& [id, emitter] = *iterator;

		if (GetAudioStatus(emitter.audioSourceId) == AudioStatus::Stopped)
			Destroy(emitter.audioSourceId, emitter.audioBufferId);
	}
}

void OpenAlPlayer::Stop(AudioSourceId sourceId) const
{
	ALint status;
	alCheckCall(alGetSourcei(sourceId, AL_SOURCE_STATE, &status));
	if (status != AL_STOPPED) alCheckCall(alSourceStop(sourceId));
}

void OpenAlPlayer::Destroy(AudioSourceId sourceId, AudioBufferId bufferId) const
{
	ALint status;
	alCheckCall(alGetSourcei(sourceId, AL_SOURCE_STATE, &status));
	if (status != AL_STOPPED) alCheckCall(alSourceStop(sourceId));
	alCheckCall(alDeleteSources(1, &sourceId));
	alCheckCall(alDeleteBuffers(1, &bufferId));
}

void OpenAlPlayer::DeleteAlDevice(ALCdevice* device)
{
	alCheckCall(alcCloseDevice(device));
}

void OpenAlPlayer::DeleteAlContext(ALCcontext* context) {
	alCheckCall(alcDestroyContext(context));
}

const AudioStatus OpenAlPlayer::GetAudioStatus(AudioSourceId id) const
{
	ALint status;
	alCheckCall(alGetSourcei(id, AL_SOURCE_STATE, &status));

	switch (status)
	{
	case AL_STOPPED:
		return AudioStatus::Stopped;
	case AL_PAUSED:
		return AudioStatus::Paused;
	case AL_PLAYING:
		return AudioStatus::Playing;
	default:
		throw std::runtime_error("Unknown audio status");
	}
}

float OpenAlPlayer::GetAudioProgress(AudioEmitter& emitter) const
{
	ALfloat offset;
	ALint size;
	alCheckCall(alGetSourcef(emitter.audioSourceId, AL_BYTE_OFFSET, &offset));
	alCheckCall(alGetBufferi(emitter.audioBufferId, AL_SIZE, &size));
	return offset / (float)size;
}

void OpenAlPlayer::UpdateListenerState(glm::vec3 pos, glm::vec3 vel, glm::vec3 front, glm::vec3 up) const
{
	alCheckCall(alListener3f(AL_POSITION, pos.z, pos.y, pos.x));
	alCheckCall(alListener3f(AL_VELOCITY, vel.z, vel.y, vel.x));
	ALfloat listenerOri[] = {front.x, front.y, front.z, up.x, up.y, up.z};
	alCheckCall(alListenerfv(AL_ORIENTATION, listenerOri));
}
