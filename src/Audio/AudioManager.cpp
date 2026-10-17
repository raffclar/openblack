/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "AudioManager.h"

#include <cmath>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <MusicCues.h>
#include <PackFile.h>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>

#include "AudioPlayerInterface.h"
#include "Camera/Camera.h"
#include "Common/MachineClock.h"
#include "Common/RandomNumberManager.h"
#include "Common/StringUtils.h"
#include "ECS/Registry.h"
#include "FileSystem/FileSystemInterface.h"
#include "HearingRange.h"
#include "ListenerFrame.h"
#include "Locator.h"
#include "Resources/Resources.h"
#include "SoundDecoder.h"

using namespace openblack::ecs::components;

namespace openblack::audio
{

namespace
{
// The game's default sample play options, which a bank header overrides where it says so
constexpr uint32_t k_MaxVolume = 127;
constexpr uint32_t k_DefaultPitchPercent = 100;
constexpr float k_DefaultMinDistance = 1.0f;
constexpr float k_DefaultMaxDistance = 9999.0f;
constexpr float k_DefaultDistanceScale = 0.3f;
} // namespace

AudioManager::AudioManager()
    : AudioManager(std::make_unique<AudioPlayer>())
{
}

AudioManager::AudioManager(std::unique_ptr<AudioPlayerInterface> player)
    : _audioPlayer(std::move(player))
{
	_audioPlayer->Initialize();
	// The atmosphere's random numbers are seeded from the date as the game reads it (pinned in a seeded run)
	_atmos = std::make_unique<AtmosPlayer>(static_cast<VoiceBackend&>(*this),
	                                       [] { return static_cast<uint32_t>(machine_clock::UnixTime()); });
	_musicStreams = std::make_unique<MusicStreamBackend>(*_audioPlayer);
	_musicPlayer = std::make_unique<MusicPlayer>(*_musicStreams);
	// The custom music player reads its tracks through the file system, and picks shuffled tracks with its own random
	// numbers
	_customMusicRandom.seed(static_cast<uint32_t>(machine_clock::UnixTime()));
	_customMusic = std::make_unique<CustomMusicPlayer>(
	    *_audioPlayer,
	    [](const std::filesystem::path& path) -> std::vector<uint8_t> {
		    try
		    {
			    return Locator::filesystem::value().ReadAll(path);
		    }
		    catch (const std::exception&)
		    {
			    return {};
		    }
	    },
	    [this](uint32_t range) { return range == 0 ? 0u : static_cast<uint32_t>(_customMusicRandom() % range); });
}

AudioManager::~AudioManager()
{
	_customMusic.reset();
	_musicPlayer.reset();
	_musicStreams.reset();
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<AudioEmitter>([this](entt::entity entity, const AudioEmitter&) { DestroyEmitter(entity); });

	// Stops the atmosphere voices before their buffers go
	_atmos.reset();
	for (const auto buffer : _decodedBuffers)
	{
		_audioPlayer->DeleteBuffer(buffer);
	}
}

namespace
{
/// The name of the sound an emitter plays, for the log
std::string EmitterSoundName(const AudioEmitter& emitter)
{
	auto& sounds = Locator::resources::value().GetSounds();
	return sounds.Contains(emitter.soundId) ? sounds.Handle(emitter.soundId)->name : fmt::format("#{}", emitter.soundId);
}

/// How an emitter is described in the log: its entity, its sound, 2D or where it is
// Only the debug and trace logging uses it, which release builds leave out
[[maybe_unused]] std::string DescribeEmitter(entt::entity entity, const AudioEmitter& emitter)
{
	return fmt::format("{} {} {}{}", static_cast<uint32_t>(entity), emitter.spatial ? "3D" : "2D", EmitterSoundName(emitter),
	                   emitter.spatial
	                       ? fmt::format(" at ({}, {}, {})", emitter.position.x, emitter.position.y, emitter.position.z)
	                       : std::string());
}

// The log lines below are left out of release builds, which would otherwise find these parameters unused
void LogEmitterStart([[maybe_unused]] entt::entity entity, [[maybe_unused]] const AudioEmitter& emitter)
{
	// Where a placed sound is heard from: to the listener's right (or left), ahead and above
	[[maybe_unused]] const auto heard = [&emitter]() {
		if (!emitter.spatial)
		{
			return std::string();
		}
		const auto listener = Locator::audio::value().GetListener();
		const auto at = audio::ToListenerFrame(emitter.position, listener.origin, listener.forward, listener.up);
		return fmt::format(", heard {:.1f} m {}, {:.1f} m ahead, {:.1f} m up", std::abs(at.x), at.x < 0.0f ? "left" : "right",
		                   at.y, at.z);
	};
	SPDLOG_LOGGER_DEBUG(spdlog::get("audio"), "Emitter {} starts: volume {} pitch {}%{}{}", DescribeEmitter(entity, emitter),
	                    emitter.volume, emitter.pitchPercent, emitter.loop == PlayType::Repeat ? ", looping" : "", heard());
}

void LogNotStarted([[maybe_unused]] const Sound& sound, [[maybe_unused]] const glm::vec3& position,
                   [[maybe_unused]] std::string_view why)
{
	SPDLOG_LOGGER_DEBUG(spdlog::get("audio"), "Sound {} at ({}, {}, {}) not started: {}", sound.name, position.x, position.y,
	                    position.z, why);
}

/// The wave format tag of MPEG sample data
constexpr uint16_t k_WaveFormatMpeg = 0x0050;

/// Why a sample got no voice to start on
[[nodiscard]] std::string_view NoVoiceReason(const VoiceChoice& choice)
{
	return choice.slot.has_value() ? "it plays once and is playing" : "all the voices play samples of higher priority";
}

float ListenerDistance(const glm::vec3& position)
{
	return glm::distance(Locator::audio::value().GetListener().origin, position);
}

std::string TooFar(const glm::vec3& position, float range)
{
	return fmt::format("{:.1f} m from the listener, beyond its {} m", ListenerDistance(position), range);
}
} // namespace

Listener openblack::audio::CameraListener()
{
	if (!Locator::camera::has_value())
	{
		return {};
	}
	const auto& camera = Locator::camera::value();
	return {.origin = camera.GetOrigin(), .forward = camera.GetForward(), .up = camera.GetUp()};
}

void AudioManager::Stop()
{
	_musicPlayer->Stop(false);
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<AudioEmitter>([this](entt::entity entity, const AudioEmitter&) { DestroyEmitter(entity); });
	_atmos->Process(false);
}

void AudioManager::Update()
{
	const auto listener = GetListener();
	_audioPlayer->UpdateListener(listener.origin, listener.forward, listener.up);

	// 3D emitters follow the listener, finished emitters are released
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<AudioEmitter>([this](entt::entity entity, AudioEmitter& emitter) {
		emitter.state = _audioPlayer->GetStatus(emitter.sourceId);
		if (emitter.state == AudioStatus::Stopped)
		{
			SPDLOG_LOGGER_DEBUG(spdlog::get("audio"), "Emitter {} has finished", DescribeEmitter(entity, emitter));
			DestroyEmitter(entity);
			return;
		}
		if (emitter.spatial)
		{
			PositionSource(emitter.sourceId, ToListenerFrame(emitter.position));
		}
		_audioPlayer->SetVolume(emitter.sourceId, EmitterVolume(emitter));
	});

	// A voice is free again when its sample stops, also when a system removes the emitter
	for (size_t slot = 0; slot < _voiceOccupants.size(); ++slot)
	{
		const auto& occupant = _voiceOccupants.at(slot);
		const bool emitterGone = occupant.emitter != entt::null && !EmitterExists(occupant.emitter);
		const bool atmosStopped = occupant.atmos != k_InvalidHandle && !IsPlaying(occupant.atmos);
		const bool empty = occupant.emitter == entt::null && occupant.atmos == k_InvalidHandle;
		if (emitterGone || atmosStopped || (empty && _voicePool.Slots()[slot].playing))
		{
			ReleaseVoice(slot);
		}
	}

	// The music thread's work: streams are fed and fade every tick
	const auto now = std::chrono::steady_clock::now();
	_musicStreams->SetOutputVolume(_globalVolume * _musicVolume);
	_musicPlayer->Update(std::chrono::duration_cast<std::chrono::microseconds>(now - _lastMusicUpdate));
	_lastMusicUpdate = now;
	_customMusic->Update();
	// Positional music, like 3D emitters, is heard from where it is placed as the listener moves
	_musicStreams->ForEachPlaced([this](SourceId source, const MusicPlacement& placement) {
		PositionSource(source, ToListenerFrame(placement.position));
	});

	// Atmosphere voices are positioned relative to the listener and only follow the volume settings here
	for (const auto& [handle, voice] : _atmosVoices)
	{
		_audioPlayer->SetVolume(voice.source, voice.gain * _globalVolume * _sfxVolume);
	}
}

float AudioManager::EmitterVolume(const AudioEmitter& emitter) const
{
	// A placed voice past its maximum distance is silent
	if (emitter.spatial && IsBeyondMaxDistance(ListenerDistance(emitter.position), emitter.maxDistance))
	{
		return 0.0f;
	}
	return emitter.gain * _globalVolume * (emitter.music ? _musicVolume : _sfxVolume);
}

void AudioManager::StopOutOfRangeEmitters()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> outOfRange;
	registry.Each<const AudioEmitter>([&outOfRange](entt::entity entity, const AudioEmitter& emitter) {
		if (emitter.spatial && emitter.owner != entt::null && emitter.state != AudioStatus::Stopped &&
		    !IsWithinPlayRange(ListenerDistance(emitter.position), emitter.maxDistance))
		{
			outOfRange.push_back(entity);
		}
	});
	for (const auto entity : outOfRange)
	{
		[[maybe_unused]] const auto& emitter = registry.Get<const AudioEmitter>(entity);
		SPDLOG_LOGGER_DEBUG(spdlog::get("audio"), "Emitter {} stops: {}", DescribeEmitter(entity, emitter),
		                    TooFar(emitter.position, emitter.maxDistance));
		DestroyEmitter(entity);
	}
}

BufferId AudioManager::CreateBuffer(ChannelLayout layout, const std::vector<int16_t>& buffer, int sampleRate)
{
	return _audioPlayer->CreateBuffer(layout, buffer, sampleRate);
}

void AudioManager::PlayEmitter(entt::entity emitter)
{
	auto& registry = Locator::entitiesRegistry::value();
	assert(registry.AnyOf<AudioEmitter>(emitter));
	auto& component = registry.Get<AudioEmitter>(emitter);
	_audioPlayer->SetVolume(component.sourceId, EmitterVolume(component));
	_audioPlayer->StartSource(component.sourceId);
	component.state = AudioStatus::Playing;
}

void AudioManager::PauseEmitter(entt::entity emitter)
{
	auto& registry = Locator::entitiesRegistry::value();
	assert(registry.AnyOf<AudioEmitter>(emitter));
	auto& component = registry.Get<AudioEmitter>(emitter);
	_audioPlayer->PauseSource(component.sourceId);
}

void AudioManager::StopEmitter(entt::entity emitter)
{
	auto& registry = Locator::entitiesRegistry::value();
	assert(registry.AnyOf<AudioEmitter>(emitter));
	auto& component = registry.Get<AudioEmitter>(emitter);
	SPDLOG_LOGGER_DEBUG(spdlog::get("audio"), "Emitter {} stopped", DescribeEmitter(emitter, component));
	_audioPlayer->StopSource(component.sourceId);
}

void AudioManager::DestroyEmitter(entt::entity emitter)
{
	auto& registry = Locator::entitiesRegistry::value();
	assert(registry.AnyOf<AudioEmitter>(emitter));
	auto& component = registry.Get<AudioEmitter>(emitter);
	if (component.state != AudioStatus::Stopped)
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("audio"), "Emitter {} stopped and removed", DescribeEmitter(emitter, component));
	}
	_audioPlayer->StopSource(component.sourceId);
	_audioPlayer->DeleteSource(component.sourceId);
	if (const auto slot = VoiceOf(emitter))
	{
		ReleaseVoice(*slot);
	}
	registry.Destroy(emitter);
}

VoiceStart AudioManager::MakeVoiceStart(const Sound& sound, std::optional<glm::vec3> worldPosition, PlayType playType)
{
	const auto overrides = [&sound](pack::AudioBankOverride flag) {
		return (sound.overrideFlags & static_cast<uint32_t>(flag)) != 0;
	};

	// Playback rate in percent with a random deviation of up to pitchDeviation percent either way
	auto pitch = overrides(pack::AudioBankOverride::Pitch) && sound.pitch != 0 ? static_cast<uint32_t>(sound.pitch)
	                                                                           : k_DefaultPitchPercent;
	const auto deviation = (pitch * static_cast<uint32_t>(sound.pitchDeviation)) / 100;
	if (deviation != 0)
	{
		pitch = pitch - deviation + Locator::rng::value().NextValue(0u, deviation * 2);
	}

	int32_t loopCount = overrides(pack::AudioBankOverride::Loop) ? sound.loop : 0;
	if (playType == PlayType::Repeat)
	{
		loopCount = -1;
	}

	return VoiceStart {
	    .bankName = {},
	    .sampleId = sound.id,
	    .volume =
	        overrides(pack::AudioBankOverride::Volume) ? std::min<uint32_t>(sound.headerVolume, k_MaxVolume) : k_MaxVolume,
	    .pitchPercent = pitch,
	    .pitch = static_cast<float>(pitch) / 100.0f,
	    .loopCount = loopCount,
	    .positional = worldPosition.has_value(),
	    .position = worldPosition ? ToListenerFrame(*worldPosition) : glm::zero<glm::vec3>(),
	    .minDistance = overrides(pack::AudioBankOverride::MinDist) ? sound.minDistance : k_DefaultMinDistance,
	    .maxDistance = overrides(pack::AudioBankOverride::MaxDist) ? sound.maxDistance : k_DefaultMaxDistance,
	    .distanceScale = overrides(pack::AudioBankOverride::Scale) ? sound.distanceScale : k_DefaultDistanceScale,
	};
}

entt::entity AudioManager::CreateEmitter(entt::id_type id, std::optional<glm::vec3> worldPosition, PlayType playType,
                                         entt::entity owner)
{
	VoiceChoice choice;
	return CreateVoiceEmitter(id, worldPosition, playType, owner, k_CallerPlayType, choice);
}

entt::entity AudioManager::CreateVoiceEmitter(entt::id_type id, std::optional<glm::vec3> worldPosition, PlayType playType,
                                              entt::entity owner, pack::AudioBankLoop callerPlayType, VoiceChoice& choice)
{
	auto& sounds = Locator::resources::value().GetSounds();
	if (!sounds.Contains(id))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("audio"), "Sound {} is not loaded", id);
		choice = {};
		return entt::null;
	}
	auto sound = sounds.Handle(id);
	choice = AcquireVoice(*sound, OwnerKey(owner), callerPlayType);
	if (!choice.start)
	{
		LogNotStarted(*sound, worldPosition.value_or(glm::zero<glm::vec3>()), NoVoiceReason(choice));
		return entt::null;
	}
	const auto start = MakeVoiceStart(*sound, worldPosition, playType);
	const auto source = CreateSource(*sound, start);

	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<AudioEmitter>(entity, AudioEmitter {
	                                          .sourceId = source,
	                                          .soundId = id,
	                                          .priority = sound->priority,
	                                          .spatial = worldPosition.has_value(),
	                                          .position = worldPosition.value_or(glm::zero<glm::vec3>()),
	                                          .gain = _atmos->VolumeToGain(start.volume),
	                                          .volume = start.volume,
	                                          .pitchPercent = start.pitchPercent,
	                                          .minDistance = start.minDistance,
	                                          .maxDistance = start.maxDistance,
	                                          .distanceScale = start.distanceScale,
	                                          .loop = start.loopCount < 0 ? PlayType::Repeat : PlayType::Once,
	                                          .state = AudioStatus::Initial,
	                                          .music = false,
	                                          .owner = owner,
	                                          .bank = sound->bank,
	                                          .group = sound->group,
	                                      });
	BindVoice(*choice.slot, *sound, entity, k_InvalidHandle);
	LogEmitterStart(entity, registry.Get<AudioEmitter>(entity));
	return entity;
}

void AudioManager::CreateBuffer(Sound& sound)
{
	std::vector<int16_t> decodeBuffer;
	auto sampleRate = sound.sampleRate;
	for (size_t i = 0; i < sound.buffer.size(); ++i)
	{
		// Decoded already when it was loaded, the same way
		const auto result = sound.decoded && i < sound.decoded->size() ? std::move((*sound.decoded)[i])
		                                                               : DecodeSound(sound.buffer[i], sound.sampleRate);
		const auto part = sound.buffer.size() > 1 ? fmt::format(" part {}", i) : std::string();
		if (!result.sound)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("audio"), "Unable to decode sound {}{} ({}): {}", sound.name, part,
			                    ToString(result.container), result.error);
			continue;
		}
		for (const auto& warning : result.warnings)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("audio"), "Sound {}{} ({}): {}", sound.name, part, ToString(result.container),
			                   warning);
		}

		const auto& decoded = *result.sound;
		SPDLOG_LOGGER_TRACE(spdlog::get("audio"), "Decoded sound {}{}: {}, {} Hz, {}, {:.3f}s, {} padding frames removed",
		                    sound.name, part, ToString(result.container), decoded.sampleRate,
		                    decoded.channelLayout == ChannelLayout::Mono ? "mono" : "stereo", decoded.Duration(),
		                    result.trimmedFrames);
		sound.channelLayout = decoded.channelLayout;
		// Play at the rate the data was recorded at, the bank header can disagree
		sampleRate = decoded.sampleRate;
		decodeBuffer.insert(decodeBuffer.end(), decoded.samples.begin(), decoded.samples.end());
	}
	sound.decoded.reset();
	sound.bufferId = CreateBuffer(sound.channelLayout, decodeBuffer, sampleRate);
	// A loop over part of the sample, as the game's mixer plays it: from the start, round its loop while looping, and on
	// to the end once let go
	const auto frames = static_cast<int32_t>(decodeBuffer.size() / (sound.channelLayout == ChannelLayout::Stereo ? 2 : 1));
	if (sound.loopStart >= 0 && sound.loopEnd > sound.loopStart && sound.loopStart < frames)
	{
		_audioPlayer->SetLoopPoints(sound.bufferId, sound.loopStart, std::min(sound.loopEnd + 1, frames));
	}
	sound.duration = _audioPlayer->GetDuration(sound.bufferId);
	sound.sizeInBytes = decodeBuffer.size() * sizeof(decodeBuffer[0]);
}

bool AudioManager::EmitterExists(entt::entity emitter)
{
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(emitter) && registry.AnyOf<AudioEmitter>(emitter);
}

float AudioManager::GetProgress(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	assert(registry.AnyOf<AudioEmitter>(entity));
	auto& emitter = registry.Get<AudioEmitter>(entity);
	auto sizeInBytes = Locator::resources::value().GetSounds().Handle(emitter.soundId)->sizeInBytes;
	return _audioPlayer->GetProgress(sizeInBytes, emitter.sourceId);
}

AudioStatus AudioManager::GetStatus(entt::entity emitter)
{
	auto& registry = Locator::entitiesRegistry::value();
	assert(registry.AnyOf<AudioEmitter>(emitter));
	auto& component = registry.Get<AudioEmitter>(emitter);
	return _audioPlayer->GetStatus(component.sourceId);
}

const Sound& AudioManager::GetSound(entt::id_type id)
{
	return Locator::resources::value().GetSounds().Handle(id);
}

void AudioManager::PlaySound(entt::id_type id, PlayType playType)
{
	const auto entity = CreateEmitter(id, std::nullopt, playType, entt::null);
	if (entity != entt::null)
	{
		PlayEmitter(entity);
	}
}

void AudioManager::PlaySoundEffect(entt::id_type id, std::optional<glm::vec3> worldPosition)
{
	auto& sounds = Locator::resources::value().GetSounds();
	if (sounds.Contains(id))
	{
		const auto sound = sounds.Handle(id);
		// A sound with a place doesn't start further from the camera than its maximum distance
		const auto range = SoundEffectStartRange(sound->maxDistance, k_DefaultMaxDistance);
		if (worldPosition.has_value() && !IsWithinStartRange(ListenerDistance(*worldPosition), range))
		{
			LogNotStarted(*sound, *worldPosition, TooFar(*worldPosition, range));
			return;
		}
	}
	const auto entity = CreateEmitter(id, worldPosition, PlayType::Once, entt::null);
	if (entity != entt::null)
	{
		PlayEmitter(entity);
	}
}

void AudioManager::StopSoundEffect(entt::id_type id)
{
	const auto bank = BankOf(id);
	for (auto playing = FindPlaying(entt::null, bank, id, 0); playing != entt::null;
	     playing = FindPlaying(entt::null, bank, id, 0))
	{
		DestroyEmitter(playing);
	}
}

bool AudioManager::IsSoundEffectPlaying(entt::id_type id) const
{
	return FindPlaying(entt::null, BankOf(id), id, 0) != entt::null;
}

uint32_t AudioManager::BankOf(entt::id_type id)
{
	auto& sounds = Locator::resources::value().GetSounds();
	return sounds.Contains(id) ? sounds.Handle(id)->bank : 0;
}

entt::entity AudioManager::StartSoundEffect(entt::id_type id, const SoundEffectOptions& options)
{
	auto& sounds = Locator::resources::value().GetSounds();
	if (!sounds.Contains(id))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("audio"), "Sound {} is not loaded", id);
		return entt::null;
	}
	auto sound = sounds.Handle(id);
	const auto range = SoundEffectStartRange(sound->maxDistance, options.maxDistance.value_or(k_DefaultMaxDistance));
	if (options.position.has_value() && !IsWithinStartRange(ListenerDistance(*options.position), range))
	{
		LogNotStarted(*sound, *options.position, TooFar(*options.position, range));
		return entt::null;
	}
	// The voice rules apply the play type of the bank header, else the play type of the caller. A sound that plays once
	// does not start again while it plays, and a sound that restarts starts again on its voice.
	const auto choice = AcquireVoice(*sound, OwnerKey(options), options.voicePlayType.value_or(k_CallerPlayType));
	if (!choice.start)
	{
		LogNotStarted(*sound, options.position.value_or(glm::zero<glm::vec3>()), NoVoiceReason(choice));
		return entt::null;
	}
	auto start = MakeVoiceStart(*sound, options.position, options.playType);
	if (options.pitchPercent.has_value())
	{
		start.pitchPercent = *options.pitchPercent;
		start.pitch = static_cast<float>(start.pitchPercent) / 100.0f;
	}
	if (options.volume.has_value())
	{
		start.volume = std::min<uint32_t>(*options.volume, k_MaxVolume);
	}
	start.minDistance = options.minDistance.value_or(start.minDistance);
	start.maxDistance = options.maxDistance.value_or(start.maxDistance);
	const auto source = CreateSource(*sound, start);
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<AudioEmitter>(entity, AudioEmitter {
	                                          .sourceId = source,
	                                          .soundId = id,
	                                          .priority = sound->priority,
	                                          .spatial = options.position.has_value(),
	                                          .position = options.position.value_or(glm::zero<glm::vec3>()),
	                                          .gain = _atmos->VolumeToGain(start.volume),
	                                          .volume = start.volume,
	                                          .pitchPercent = start.pitchPercent,
	                                          .minDistance = start.minDistance,
	                                          .maxDistance = start.maxDistance,
	                                          .distanceScale = start.distanceScale,
	                                          .loop = start.loopCount < 0 ? PlayType::Repeat : PlayType::Once,
	                                          .state = AudioStatus::Initial,
	                                          .music = false,
	                                          .owner = options.owner,
	                                          .bank = sound->bank,
	                                          .group = sound->group,
	                                      });
	BindVoice(*choice.slot, *sound, entity, k_InvalidHandle);
	LogEmitterStart(entity, registry.Get<AudioEmitter>(entity));
	PlayEmitter(entity);
	return entity;
}

void AudioManager::SetEmitterPosition(entt::entity emitter, const glm::vec3& position)
{
	if (!EmitterExists(emitter))
	{
		return;
	}
	Locator::entitiesRegistry::value().Get<AudioEmitter>(emitter).position = position;
}

void AudioManager::ReleaseEmitterLoop(entt::entity emitter)
{
	if (!EmitterExists(emitter))
	{
		return;
	}
	auto& component = Locator::entitiesRegistry::value().Get<AudioEmitter>(emitter);
	SPDLOG_LOGGER_DEBUG(spdlog::get("audio"), "Emitter {} let go: plays to the end of its pass",
	                    DescribeEmitter(emitter, component));
	_audioPlayer->SetLooping(component.sourceId, false);
	component.loop = PlayType::Once;
}

bool AudioManager::IsEmitterLooping(entt::entity emitter)
{
	return EmitterExists(emitter) && Locator::entitiesRegistry::value().Get<AudioEmitter>(emitter).loop == PlayType::Repeat;
}

void AudioManager::SetEmitterVolume(entt::entity emitter, uint32_t volume)
{
	if (!EmitterExists(emitter))
	{
		return;
	}
	auto& component = Locator::entitiesRegistry::value().Get<AudioEmitter>(emitter);
	SPDLOG_LOGGER_TRACE(spdlog::get("audio"), "Emitter {} volume {} -> {}", DescribeEmitter(emitter, component),
	                    component.volume, std::min<uint32_t>(volume, k_MaxVolume));
	component.volume = std::min<uint32_t>(volume, k_MaxVolume);
	component.gain = _atmos->VolumeToGain(component.volume);
	_audioPlayer->SetVolume(component.sourceId, EmitterVolume(component));
}

void AudioManager::SetEmitterPitch(entt::entity emitter, uint32_t pitchPercent)
{
	if (!EmitterExists(emitter))
	{
		return;
	}
	auto& component = Locator::entitiesRegistry::value().Get<AudioEmitter>(emitter);
	if (component.pitchPercent == pitchPercent)
	{
		return;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("audio"), "Emitter {} pitch {}% -> {}%", DescribeEmitter(emitter, component),
	                    component.pitchPercent, pitchPercent);
	component.pitchPercent = pitchPercent;
	_audioPlayer->SetPitch(component.sourceId, static_cast<float>(pitchPercent) / 100.0f);
}

uint32_t AudioManager::GetEmitterVolume(entt::entity emitter)
{
	return EmitterExists(emitter) ? Locator::entitiesRegistry::value().Get<AudioEmitter>(emitter).volume : 0;
}

void AudioManager::StopOwnedSounds(entt::entity owner)
{
	if (owner == entt::null)
	{
		return;
	}
	std::vector<entt::entity> owned;
	Locator::entitiesRegistry::value().Each<const AudioEmitter>([&](entt::entity entity, const AudioEmitter& emitter) {
		if (emitter.owner == owner)
		{
			owned.push_back(entity);
		}
	});
	for (const auto entity : owned)
	{
		DestroyEmitter(entity);
	}
}

void AudioManager::StopAllSoundEffects()
{
	std::vector<entt::entity> effects;
	Locator::entitiesRegistry::value().Each<const AudioEmitter>([&effects](entt::entity entity, const AudioEmitter& emitter) {
		if (!emitter.music)
		{
			effects.push_back(entity);
		}
	});
	for (const auto entity : effects)
	{
		DestroyEmitter(entity);
	}
}

void AudioManager::AddAnimEffects(const std::string& bankName, AnimEffectTable table)
{
	_animEffects.insert_or_assign(bankName, std::move(table));
}

AnimEffectPlay AudioManager::PlayAnimEffect(const std::string& bankName, std::span<const int32_t> keys, entt::entity owner,
                                            const glm::vec3& position, const std::optional<SoundEffectConditions>& heardUnder)
{
	const auto effects = _animEffects.find(bankName);
	if (effects == _animEffects.end())
	{
		return {};
	}
	// Any of the samples of the effect
	const auto samples = effects->second.Find(keys);
	if (samples.empty())
	{
		return {};
	}
	const auto sample = samples[Locator::rng::value().NextValue<size_t>(0, samples.size() - 1)];
	const auto id = entt::hashed_string(fmt::format("{}/{}", bankName, sample).c_str()).value();
	auto& sounds = Locator::resources::value().GetSounds();
	if (!sounds.Contains(id))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("audio"), "Sound {}/{} of an animation effect is not loaded", bankName, sample);
		return {.outcome = AnimEffectPlay::Outcome::NotLoaded, .emitter = entt::null, .sample = sample};
	}
	const auto sound = sounds.Handle(id);
	// The sample's own say in when it is heard, as for the scripts' sound effects
	if (heardUnder.has_value() && !SoundEffectHeard(*heardUnder, BankOfFile(bankName), sound->userParam))
	{
		LogNotStarted(*sound, position, "its sample may not be heard now");
		return {.outcome = AnimEffectPlay::Outcome::NotHeard, .emitter = entt::null, .sample = sample};
	}

	// The sample doesn't start further from the camera than its bank's maximum distance, applied to the voice or not
	const auto range = AnimEffectStartRange(sound->maxDistance);
	if (!IsWithinStartRange(ListenerDistance(position), range))
	{
		LogNotStarted(*sound, position, TooFar(position, range));
		return {.outcome = AnimEffectPlay::Outcome::TooFar, .emitter = entt::null, .sample = sample};
	}

	// The library plays an animation effect with the restart play type, unless the bank header gives another
	VoiceChoice choice;
	const auto entity = CreateVoiceEmitter(id, position, PlayType::Once, owner, pack::AudioBankLoop::Restart, choice);
	if (entity == entt::null)
	{
		const auto outcome = choice.slot.has_value() ? AnimEffectPlay::Outcome::AlreadyPlaying
		                     : sounds.Contains(id)   ? AnimEffectPlay::Outcome::NoVoice
		                                             : AnimEffectPlay::Outcome::NotLoaded;
		return {.outcome = outcome, .emitter = entt::null, .sample = sample};
	}
	PlayEmitter(entity);
	return {.outcome = AnimEffectPlay::Outcome::Played, .emitter = entity, .sample = sample};
}

entt::entity AudioManager::FindPlaying(entt::entity owner, entt::id_type bank, entt::id_type id, uint16_t group) const
{
	auto found = entt::entity {entt::null};
	Locator::entitiesRegistry::value().Each<const AudioEmitter>([&](entt::entity entity, const AudioEmitter& emitter) {
		if (found != entt::null || emitter.owner != owner || emitter.bank != bank || emitter.state == AudioStatus::Stopped)
		{
			return;
		}
		if (emitter.soundId == id || (group != 0 && emitter.group == group))
		{
			found = entity;
		}
	});
	return found;
}

void AudioManager::CreateSoundGroup(const std::string& name)
{
	_soundGroups[name] = SoundGroup();
}

void AudioManager::AddMusicEntry(const std::string& name)
{
	_music.emplace_back(name);
}

void AudioManager::AddToSoundGroup(const std::string& name, entt::id_type id)
{
	_soundGroups[name].sounds.emplace_back(id);
}

const SoundGroup& AudioManager::GetSoundGroup(const std::string& name)
{
	return _soundGroups[name];
}

const std::map<std::string, SoundGroup>& AudioManager::GetSoundGroups()
{
	return _soundGroups;
}

void AudioManager::PlayMusic(const std::string& packPath, PlayType type)
{
	_musicPlayer->Stop(false);
	MusicPlay(packPath, MusicPlayOptions {.loops = type == PlayType::Repeat ? -1 : 0});
}

void AudioManager::StopMusic()
{
	_musicPlayer->Stop(false);
}

namespace
{
/// The game names its music in lower case, which the files on a case sensitive file system may not be
std::optional<std::filesystem::path> FindIgnoringCase(const std::filesystem::path& path)
{
	if (path.is_absolute())
	{
		return std::filesystem::exists(path) ? std::optional(path) : std::nullopt;
	}
	auto found = Locator::filesystem::value().GetGamePath();
	for (const auto& part : path.relative_path())
	{
		auto match = found / part;
		if (!std::filesystem::exists(match))
		{
			const auto wanted = string_utils::LowerCase(part.string());
			std::error_code error;
			for (const auto& entry : std::filesystem::directory_iterator(found, error))
			{
				if (string_utils::LowerCase(entry.path().filename().string()) == wanted)
				{
					match = entry.path();
					break;
				}
			}
		}
		if (!std::filesystem::exists(match))
		{
			return std::nullopt;
		}
		found = match;
	}
	return found;
}
} // namespace

std::shared_ptr<const MusicBank> AudioManager::LoadMusicBank(const std::string& bankPath)
{
	if (const auto found = _musicBanks.find(bankPath); found != _musicBanks.end())
	{
		return found->second;
	}
	auto& registered = _musicBanks[bankPath];
	const auto path = FindIgnoringCase(bankPath);
	if (!path)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("audio"), "Music bank {} not found", bankPath);
		return nullptr;
	}
	// Only the bank's headers are read: its music stays in the file
	pack::PackFile pack;
	if (pack.OpenAudioIndex(*path) != pack::PackResult::Success || pack.GetAudioSampleHeaders().empty())
	{
		SPDLOG_LOGGER_WARN(spdlog::get("audio"), "Music bank {} could not be read", path->string());
		return nullptr;
	}
	auto bank = std::make_shared<MusicBank>();
	bank->path = bankPath;
	const auto& headers = pack.GetAudioSampleHeaders();
	const auto& first = headers.front();
	const auto overrides = [&first](pack::AudioBankOverride flag) {
		return (first.overrideFlags & static_cast<uint32_t>(flag)) != 0;
	};
	bank->groupId = first.group;
	if (overrides(pack::AudioBankOverride::Volume))
	{
		bank->bankVolume = std::min<uint32_t>(first.volume, MusicPlayer::k_MaxVolume);
	}
	if (overrides(pack::AudioBankOverride::Loop))
	{
		bank->loopOverride = first.loop;
	}
	if (overrides(pack::AudioBankOverride::MinDist))
	{
		bank->minDistance = first.minDist;
	}
	if (overrides(pack::AudioBankOverride::MaxDist))
	{
		bank->maxDistance = first.maxDist;
	}
	if (overrides(pack::AudioBankOverride::Scale))
	{
		bank->distanceScale = first.scale;
	}
	bank->musicMaxDistance = first.maxDist;
	bank->chunkCount = static_cast<uint32_t>(headers.size());
	for (auto& cue : pack::ReadMusicCues(headers))
	{
		bank->cues.push_back({.chunk = cue.chunk, .frame = cue.frame, .text = std::move(cue.text)});
	}
	bank->chunkSampleRates.reserve(headers.size());
	std::vector<std::pair<uint64_t, uint32_t>> spans;
	spans.reserve(headers.size());
	for (uint32_t i = 0; i < headers.size(); ++i)
	{
		bank->chunkSampleRates.push_back(headers[i].sampleRate);
		spans.push_back(pack.GetAudioSampleFileSpan(i).value_or(std::pair<uint64_t, uint32_t> {0, 0}));
	}
	bank->readChunk = [file = *path, spans = std::move(spans)](uint32_t chunk) {
		std::vector<uint8_t> data;
		if (chunk >= spans.size())
		{
			return data;
		}
		const auto [offset, size] = spans[chunk];
		std::ifstream stream(file, std::ios::binary);
		if (!stream.seekg(static_cast<std::streamoff>(offset)))
		{
			return data;
		}
		data.resize(size);
		if (!stream.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size)))
		{
			data.clear();
		}
		return data;
	};
	registered = bank;
	return bank;
}

bool AudioManager::MusicPlay(const std::string& bankPath, const MusicPlayOptions& options)
{
	auto withBank = options;
	withBank.bank = LoadMusicBank(bankPath);
	return _musicPlayer->Play(withBank);
}

void AudioManager::MusicStop(bool fadeOut)
{
	_musicPlayer->Stop(fadeOut);
}

void AudioManager::MusicStopBank(const std::string& bankPath, bool fadeOut)
{
	if (const auto bank = LoadMusicBank(bankPath))
	{
		_musicPlayer->StopBank(bank, fadeOut);
	}
}

bool AudioManager::MusicIsActive() const
{
	return _musicPlayer->IsActive();
}

std::optional<MusicBankInfo> AudioManager::GetMusicBankInfo(const std::string& bankPath)
{
	const auto bank = LoadMusicBank(bankPath);
	if (!bank)
	{
		return std::nullopt;
	}
	return MusicBankInfo {.groupId = bank->groupId, .chunkCount = bank->GetChunkCount(), .maxDistance = bank->musicMaxDistance};
}

uint32_t AudioManager::AtmosRegisterBank(const std::string& bankName, const std::vector<pack::AudioBankSampleHeader>& headers,
                                         uint16_t atmosCount)
{
	return _atmos->RegisterBank(bankName, headers, atmosCount);
}

void AudioManager::AtmosReleaseBank(uint32_t bank)
{
	_atmos->ReleaseBank(bank);
}

void AudioManager::AtmosSetBankVolume(uint32_t bank, int32_t volume)
{
	_atmos->SetBankVolume(bank, volume);
}

void AudioManager::AtmosSetGroup(uint32_t bank, uint32_t group)
{
	_atmos->SetGroup(bank, group);
}

void AudioManager::AtmosProcess(bool active)
{
	_atmos->Process(active);
}

VoiceBackend::Handle AudioManager::Start(const VoiceStart& start)
{
	const auto stringId = fmt::format("{}/{}", start.bankName, start.sampleId);
	const entt::id_type id = entt::hashed_string(stringId.c_str());
	auto& sounds = Locator::resources::value().GetSounds();
	if (!sounds.Contains(id))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("audio"), "Atmosphere sample {} is not loaded", stringId);
		return k_InvalidHandle;
	}
	auto sound = sounds.Handle(id);
	const auto choice = AcquireVoice(*sound, VoicePool::k_AtmosOwner, start.playType);
	if (!choice.start)
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("audio"), "Atmosphere sample {} not started: {}", stringId, NoVoiceReason(choice));
		return k_InvalidHandle;
	}
	const auto source = CreateSource(*sound, start);
	const auto gain = _atmos->VolumeToGain(start.volume);
	_audioPlayer->SetVolume(source, gain * _globalVolume * _sfxVolume);
	_audioPlayer->StartSource(source);

	const auto handle = _nextAtmosVoice++;
	_atmosVoices.emplace(handle, AtmosVoice {.source = source, .gain = gain, .slot = *choice.slot});
	BindVoice(*choice.slot, *sound, entt::null, handle);
	SPDLOG_LOGGER_DEBUG(spdlog::get("audio"), "Atmosphere voice {}: {} volume {} pitch {}% at ({}, {})", handle, stringId,
	                    start.volume, start.pitchPercent, start.position.x, start.position.y);
	return handle;
}

SourceId AudioManager::CreateSource(Sound& sound, const VoiceStart& start)
{
	if (sound.bufferId == 0)
	{
		CreateBuffer(sound);
		_decodedBuffers.push_back(sound.bufferId);
	}

	// Sources are positioned relative to the listener, in the game's listener frame
	const auto source = _audioPlayer->CreateSource(start.pitch, true);
	// QMixer plays a sample loopCount + 1 times
	const auto loop = start.loopCount < 0;
	const auto playCount = loop ? 1 : start.loopCount + 1;
	for (int32_t i = 0; i < playCount; ++i)
	{
		_audioPlayer->QueueBuffer(source, sound.bufferId);
	}
	_audioPlayer->SetLooping(source, loop);
	if (start.positional)
	{
		// Up to the maximum distance, the game's distance gain is OpenAL's inverse clamped model with the scale as
		// the roll-off. Past it, the voice is silenced with its volume.
		_audioPlayer->SetDistanceAttenuation(source, start.minDistance, start.maxDistance, start.distanceScale);
		PositionSource(source, start.position);
	}
	else
	{
		_audioPlayer->SetDistanceAttenuation(source, 1.0f, 1.0f, 0.0f);
		PositionSource(source, std::nullopt);
	}
	return source;
}

void AudioManager::PositionSource(SourceId source, std::optional<glm::vec3> listenerPosition)
{
	if (!listenerPosition)
	{
		_audioPlayer->SetPosition(source, glm::zero<glm::vec3>());
		return;
	}
	// The game's listener frame is x right, y forward, z up; OpenAL's is x right, y up, -z forward
	const auto& position = *listenerPosition;
	_audioPlayer->SetPosition(source, glm::vec3(position.x, position.z, -position.y));
}

glm::vec3 AudioManager::ToListenerFrame(glm::vec3 worldPosition)
{
	const auto listener = Locator::audio::value().GetListener();
	return audio::ToListenerFrame(worldPosition, listener.origin, listener.forward, listener.up);
}

bool AudioManager::IsPlaying(Handle handle) const
{
	const auto iter = _atmosVoices.find(handle);
	return iter != _atmosVoices.end() && _audioPlayer->GetStatus(iter->second.source) == AudioStatus::Playing;
}

void AudioManager::SetVolume(Handle handle, uint32_t volume)
{
	const auto iter = _atmosVoices.find(handle);
	if (iter == _atmosVoices.end())
	{
		return;
	}
	iter->second.gain = _atmos->VolumeToGain(volume);
	_audioPlayer->SetVolume(iter->second.source, iter->second.gain * _globalVolume * _sfxVolume);
}

void AudioManager::Stop(Handle handle)
{
	const auto iter = _atmosVoices.find(handle);
	if (iter == _atmosVoices.end())
	{
		return;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("audio"), "Atmosphere voice {} stopped", handle);
	_audioPlayer->StopSource(iter->second.source);
	_audioPlayer->DeleteSource(iter->second.source);
	if (_voiceOccupants.at(iter->second.slot).atmos == handle)
	{
		ReleaseVoice(iter->second.slot);
	}
	_atmosVoices.erase(iter);
}

VoiceChoice AudioManager::AcquireVoice(const Sound& sound, uint64_t ownerKey, pack::AudioBankLoop callerPlayType)
{
	const bool headerPlayType = (sound.overrideFlags & static_cast<uint32_t>(pack::AudioBankOverride::LoopType)) != 0;
	const VoiceRequest request {
	    .owner = ownerKey,
	    .bank = sound.bank,
	    .sample = static_cast<uint32_t>(sound.id),
	    .group = sound.group,
	    .priority = static_cast<uint16_t>(sound.priority),
	    .playType = headerPlayType ? sound.loopType : callerPlayType,
	};
	const auto choice = _voicePool.Choose(request);
	if (!choice.start)
	{
		return choice;
	}
	// The new sample stops what played on the voice
	const auto occupant = _voiceOccupants.at(*choice.slot);
	if (occupant.emitter != entt::null && EmitterExists(occupant.emitter))
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("audio"), "Sound {} takes voice {} from emitter {}", sound.name, *choice.slot,
		                    static_cast<uint32_t>(occupant.emitter));
		DestroyEmitter(occupant.emitter);
	}
	else if (occupant.atmos != k_InvalidHandle)
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("audio"), "Sound {} takes voice {} from atmosphere voice {}", sound.name, *choice.slot,
		                    occupant.atmos);
		Stop(occupant.atmos);
	}
	_voicePool.Occupy(*choice.slot, request);
	return choice;
}

void AudioManager::BindVoice(size_t slot, const Sound& sound, entt::entity emitter, Handle atmos)
{
	_voiceOccupants.at(slot) = VoiceOccupant {
	    .emitter = emitter,
	    .atmos = atmos,
	    .forgetSample = sound.waveFormat == k_WaveFormatMpeg,
	};
}

void AudioManager::ReleaseVoice(size_t slot)
{
	_voicePool.Finish(slot, _voiceOccupants.at(slot).forgetSample);
	_voiceOccupants.at(slot) = {};
}

std::optional<size_t> AudioManager::VoiceOf(entt::entity emitter) const
{
	const auto iter = std::ranges::find(_voiceOccupants, emitter, &VoiceOccupant::emitter);
	if (iter == _voiceOccupants.end())
	{
		return std::nullopt;
	}
	return static_cast<size_t>(std::distance(_voiceOccupants.begin(), iter));
}

uint64_t AudioManager::OwnerKey(entt::entity owner)
{
	return VoiceOwnerKey(VoiceOwner::Entity,
	                     owner == entt::null ? std::nullopt : std::optional<uint32_t>(entt::to_integral(owner)), 0, 0);
}

uint64_t AudioManager::OwnerKey(const SoundEffectOptions& options)
{
	const auto owner = options.owner == entt::null ? std::nullopt : std::optional<uint32_t>(entt::to_integral(options.owner));
	const auto key = VoiceOwnerKey(options.voiceOwner, owner, options.ownerNumber, _ownSoundSerial);
	if (options.voiceOwner == VoiceOwner::OwnSound)
	{
		++_ownSoundSerial;
	}
	return key;
}

} // namespace openblack::audio
