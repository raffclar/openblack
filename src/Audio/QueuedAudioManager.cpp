/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "QueuedAudioManager.h"

#include <string_view>
#include <utility>

#include "Common/MainThreadQueue.h"

using namespace openblack;
using namespace openblack::audio;

QueuedAudioManager::QueuedAudioManager(std::unique_ptr<AudioManagerInterface> audio, MainThreadQueue& queue)
    : _audio(std::move(audio))
    , _queue(queue)
{
}

QueuedAudioManager::~QueuedAudioManager() = default;

template <typename Call>
void QueuedAudioManager::Post(Call&& call, std::source_location where)
{
	_queue.CheckHeld(MainThreadApi::Audio, "a call with no result", where);
	_queue.Post(MainThreadApi::Audio, std::forward<Call>(call));
}

void QueuedAudioManager::Direct(std::string_view what, std::source_location where) const
{
	_queue.CheckHeld(MainThreadApi::Audio, what, where);
}

bool QueuedAudioManager::IsInstalled() const
{
	Direct("IsInstalled");
	return _audio->IsInstalled();
}

bool QueuedAudioManager::IsDeviceLost() const
{
	Direct("IsDeviceLost");
	return _audio->IsDeviceLost();
}

void QueuedAudioManager::SimulateDeviceLoss(bool lost)
{
	Post([audio = _audio.get(), lost] { audio->SimulateDeviceLoss(lost); });
}

void QueuedAudioManager::Stop()
{
	Post([audio = _audio.get()] { audio->Stop(); });
}

void QueuedAudioManager::Update()
{
	Post([audio = _audio.get()] { audio->Update(); });
}

void QueuedAudioManager::ProcessTurn()
{
	Post([audio = _audio.get()] { audio->ProcessTurn(); });
}

void QueuedAudioManager::StopOutOfRangeEmitters()
{
	Post([audio = _audio.get()] { audio->StopOutOfRangeEmitters(); });
}

BufferId QueuedAudioManager::CreateBuffer(ChannelLayout layout, const std::vector<int16_t>& buffer, int sampleRate)
{
	Direct("CreateBuffer");
	return _audio->CreateBuffer(layout, buffer, sampleRate);
}

void QueuedAudioManager::CreateBuffer(Sound& sound)
{
	Direct("CreateBuffer");
	_audio->CreateBuffer(sound);
}

void QueuedAudioManager::PlayEmitter(entt::entity emitter)
{
	Post([audio = _audio.get(), emitter] { audio->PlayEmitter(emitter); });
}

void QueuedAudioManager::PauseEmitter(entt::entity emitter)
{
	Post([audio = _audio.get(), emitter] { audio->PauseEmitter(emitter); });
}

void QueuedAudioManager::StopEmitter(entt::entity emitter)
{
	Post([audio = _audio.get(), emitter] { audio->StopEmitter(emitter); });
}

void QueuedAudioManager::DestroyEmitter(entt::entity emitter)
{
	Post([audio = _audio.get(), emitter] { audio->DestroyEmitter(emitter); });
}

void QueuedAudioManager::ForgetSavedEmitters()
{
	Post([audio = _audio.get()] { audio->ForgetSavedEmitters(); });
}

ecs::Registry* QueuedAudioManager::Emitters()
{
	Direct("Emitters");
	return _audio->Emitters();
}

entt::entity QueuedAudioManager::CreateEmitter(entt::id_type id, std::optional<glm::vec3> worldPosition, PlayType playType,
                                               entt::entity owner)
{
	Direct("CreateEmitter");
	return _audio->CreateEmitter(id, worldPosition, playType, owner);
}

entt::entity QueuedAudioManager::CreateOwnSoundEmitter(entt::id_type id, std::optional<glm::vec3> worldPosition,
                                                       PlayType playType)
{
	Direct("CreateOwnSoundEmitter");
	return _audio->CreateOwnSoundEmitter(id, worldPosition, playType);
}

bool QueuedAudioManager::EmitterExists(entt::entity emitter)
{
	Direct("EmitterExists");
	return _audio->EmitterExists(emitter);
}

float QueuedAudioManager::GetProgress(entt::entity emitter)
{
	Direct("GetProgress");
	return _audio->GetProgress(emitter);
}

AudioStatus QueuedAudioManager::GetStatus(entt::entity emitter)
{
	Direct("GetStatus");
	return _audio->GetStatus(emitter);
}

void QueuedAudioManager::SetGlobalVolume(float volume)
{
	Post([audio = _audio.get(), volume] { audio->SetGlobalVolume(volume); });
}

void QueuedAudioManager::SetSfxVolume(float volume)
{
	Post([audio = _audio.get(), volume] { audio->SetSfxVolume(volume); });
}

void QueuedAudioManager::SetMusicVolume(float volume)
{
	Post([audio = _audio.get(), volume] { audio->SetMusicVolume(volume); });
}

float QueuedAudioManager::GetGlobalVolume()
{
	Direct("GetGlobalVolume");
	return _audio->GetGlobalVolume();
}

float QueuedAudioManager::GetSfxVolume()
{
	Direct("GetSfxVolume");
	return _audio->GetSfxVolume();
}

float QueuedAudioManager::GetMusicVolume()
{
	Direct("GetMusicVolume");
	return _audio->GetMusicVolume();
}

void QueuedAudioManager::PlayMusic(const std::string& packPath, PlayType type)
{
	Post([audio = _audio.get(), packPath, type] { audio->PlayMusic(packPath, type); });
}

void QueuedAudioManager::StopMusic()
{
	Post([audio = _audio.get()] { audio->StopMusic(); });
}

bool QueuedAudioManager::MusicPlay(const std::string& bankPath, const MusicPlayOptions& options)
{
	Direct("MusicPlay");
	return _audio->MusicPlay(bankPath, options);
}

void QueuedAudioManager::MusicStop(bool fadeOut)
{
	Post([audio = _audio.get(), fadeOut] { audio->MusicStop(fadeOut); });
}

void QueuedAudioManager::MusicStopBank(const std::string& bankPath, bool fadeOut)
{
	Post([audio = _audio.get(), bankPath, fadeOut] { audio->MusicStopBank(bankPath, fadeOut); });
}

bool QueuedAudioManager::MusicIsActive() const
{
	Direct("MusicIsActive");
	return _audio->MusicIsActive();
}

std::optional<MusicBankInfo> QueuedAudioManager::GetMusicBankInfo(const std::string& bankPath)
{
	Direct("GetMusicBankInfo");
	return _audio->GetMusicBankInfo(bankPath);
}

const MusicPlayer* QueuedAudioManager::GetMusic() const
{
	Direct("GetMusic");
	return _audio->GetMusic();
}

void QueuedAudioManager::PlaySound(entt::id_type id, PlayType type)
{
	Post([audio = _audio.get(), id, type] { audio->PlaySound(id, type); });
}

void QueuedAudioManager::PlaySoundEffect(entt::id_type id, std::optional<glm::vec3> worldPosition)
{
	Post([audio = _audio.get(), id, worldPosition] { audio->PlaySoundEffect(id, worldPosition); });
}

void QueuedAudioManager::StopSoundEffect(entt::id_type id)
{
	Post([audio = _audio.get(), id] { audio->StopSoundEffect(id); });
}

void QueuedAudioManager::StopNumberedSoundEffect(entt::id_type id, uint32_t ownerNumber)
{
	Post([audio = _audio.get(), id, ownerNumber] { audio->StopNumberedSoundEffect(id, ownerNumber); });
}

bool QueuedAudioManager::IsNumberedSoundEffectPlaying(entt::id_type id, uint32_t ownerNumber) const
{
	Direct("IsNumberedSoundEffectPlaying");
	return _audio->IsNumberedSoundEffectPlaying(id, ownerNumber);
}

entt::entity QueuedAudioManager::StartSoundEffect(entt::id_type id, const SoundEffectOptions& options)
{
	Direct("StartSoundEffect");
	return _audio->StartSoundEffect(id, options);
}

void QueuedAudioManager::SetEmitterPosition(entt::entity emitter, const glm::vec3& position)
{
	Post([audio = _audio.get(), emitter, position] { audio->SetEmitterPosition(emitter, position); });
}

void QueuedAudioManager::SetEmitterFollows(entt::entity emitter, Follows follows)
{
	Post([audio = _audio.get(), emitter, follows] { audio->SetEmitterFollows(emitter, follows); });
}

void QueuedAudioManager::ReleaseEmitterLoop(entt::entity emitter)
{
	Post([audio = _audio.get(), emitter] { audio->ReleaseEmitterLoop(emitter); });
}

bool QueuedAudioManager::IsEmitterLooping(entt::entity emitter)
{
	Direct("IsEmitterLooping");
	return _audio->IsEmitterLooping(emitter);
}

void QueuedAudioManager::SetEmitterVolume(entt::entity emitter, uint32_t volume)
{
	Post([audio = _audio.get(), emitter, volume] { audio->SetEmitterVolume(emitter, volume); });
}

void QueuedAudioManager::SetEmitterPitch(entt::entity emitter, uint32_t pitchPercent)
{
	Post([audio = _audio.get(), emitter, pitchPercent] { audio->SetEmitterPitch(emitter, pitchPercent); });
}

uint32_t QueuedAudioManager::GetEmitterVolume(entt::entity emitter)
{
	Direct("GetEmitterVolume");
	return _audio->GetEmitterVolume(emitter);
}

void QueuedAudioManager::StopOwnedSounds(entt::entity owner)
{
	Post([audio = _audio.get(), owner] { audio->StopOwnedSounds(owner); });
}

void QueuedAudioManager::StopAllSoundEffects()
{
	Post([audio = _audio.get()] { audio->StopAllSoundEffects(); });
}

void QueuedAudioManager::AddAnimEffects(const std::string& bankName, AnimEffectTable table)
{
	Post([audio = _audio.get(), bankName, table = std::move(table)]() mutable {
		audio->AddAnimEffects(bankName, std::move(table));
	});
}

AnimEffectPlay QueuedAudioManager::PlayAnimEffect(const std::string& bankName, std::span<const int32_t> keys,
                                                  entt::entity owner, const glm::vec3& position,
                                                  const std::optional<SoundEffectConditions>& heardUnder,
                                                  AnimEffectMeasure measure)
{
	Direct("PlayAnimEffect");
	return _audio->PlayAnimEffect(bankName, keys, owner, position, heardUnder, measure);
}

void QueuedAudioManager::EndAnimEffect(const std::string& bankName, std::span<const int32_t> keys, entt::entity owner,
                                       const glm::vec3& position, AnimEffectEnd end, AnimEffectMeasure measure)
{
	Post([audio = _audio.get(), bankName, keys = std::vector<int32_t>(keys.begin(), keys.end()), owner, position, end,
	      measure] { audio->EndAnimEffect(bankName, keys, owner, position, end, measure); });
}

const Sound& QueuedAudioManager::GetSound(entt::id_type id)
{
	Direct("GetSound");
	return _audio->GetSound(id);
}

void QueuedAudioManager::CreateSoundGroup(const std::string& name)
{
	Post([audio = _audio.get(), name] { audio->CreateSoundGroup(name); });
}

void QueuedAudioManager::AddToSoundGroup(const std::string& name, entt::id_type id)
{
	Post([audio = _audio.get(), name, id] { audio->AddToSoundGroup(name, id); });
}

const SoundGroup& QueuedAudioManager::GetSoundGroup(const std::string& name)
{
	Direct("GetSoundGroup");
	return _audio->GetSoundGroup(name);
}

const std::map<std::string, SoundGroup>& QueuedAudioManager::GetSoundGroups()
{
	Direct("GetSoundGroups");
	return _audio->GetSoundGroups();
}

void QueuedAudioManager::AddMusicEntry(const std::string& name)
{
	Post([audio = _audio.get(), name] { audio->AddMusicEntry(name); });
}

const std::vector<std::string>& QueuedAudioManager::GetMusicTracks() const
{
	Direct("GetMusicTracks");
	return _audio->GetMusicTracks();
}

uint32_t QueuedAudioManager::AtmosRegisterBank(const std::string& bankName,
                                               const std::vector<pack::AudioBankSampleHeader>& headers, uint16_t atmosCount)
{
	Direct("AtmosRegisterBank");
	return _audio->AtmosRegisterBank(bankName, headers, atmosCount);
}

void QueuedAudioManager::AtmosReleaseBank(uint32_t bank)
{
	Post([audio = _audio.get(), bank] { audio->AtmosReleaseBank(bank); });
}

void QueuedAudioManager::AtmosSetBankVolume(uint32_t bank, int32_t volume)
{
	Post([audio = _audio.get(), bank, volume] { audio->AtmosSetBankVolume(bank, volume); });
}

void QueuedAudioManager::AtmosSetGroup(uint32_t bank, uint32_t group)
{
	Post([audio = _audio.get(), bank, group] { audio->AtmosSetGroup(bank, group); });
}

void QueuedAudioManager::AtmosProcess(bool active)
{
	Post([audio = _audio.get(), active] { audio->AtmosProcess(active); });
}

const AtmosPlayer* QueuedAudioManager::GetAtmos() const
{
	Direct("GetAtmos");
	return _audio->GetAtmos();
}

Listener QueuedAudioManager::GetListener() const
{
	Direct("GetListener");
	return _audio->GetListener();
}

void QueuedAudioManager::SetListenerOverride(std::optional<Listener> listener)
{
	Post([audio = _audio.get(), listener] { audio->SetListenerOverride(listener); });
}

CustomMusicPlayerInterface* QueuedAudioManager::GetCustomMusic()
{
	Direct("GetCustomMusic");
	return _audio->GetCustomMusic();
}
