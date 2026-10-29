/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameSoundRequests.h"

#include <algorithm>
#include <utility>

#include <spdlog/spdlog.h>

#include "AudioManagerInterface.h"
#include "ECS/Components/AudioEmitter.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "Sound/GameSoundEffects.h"

namespace openblack::audio
{

namespace
{
/// The audio manager, or null when the computer has none
AudioManagerInterface* Manager()
{
	return Locator::audio::has_value() ? &Locator::audio::value() : nullptr;
}

/// The bank of the loaded sound group a sound is in, None when it is in none
ScriptSoundBank BankOfSound(entt::id_type sound)
{
	auto* const audio = Manager();
	if (audio == nullptr)
	{
		return ScriptSoundBank::None;
	}
	for (const auto& [name, group] : audio->GetSoundGroups())
	{
		if (std::ranges::find(group.sounds, sound) != group.sounds.end())
		{
			return BankOfFile(name);
		}
	}
	return ScriptSoundBank::None;
}
} // namespace

bool GameSoundRequests::IsInstalled() const
{
	const auto* const audio = Manager();
	return audio != nullptr && audio->IsInstalled();
}

bool GameSoundRequests::IsDeviceLost() const
{
	const auto* const audio = Manager();
	return audio != nullptr && audio->IsDeviceLost();
}

bool GameSoundRequests::IsGameEffectHeard(const SoundEffectConditions& conditions, entt::id_type sound) const
{
	if (!Locator::resources::has_value())
	{
		return false;
	}
	const auto& sounds = Locator::resources::value().GetSounds();
	if (!sounds.Contains(sound))
	{
		return false;
	}
	// The bank only matters while the scripts have the game's sound off
	const auto bank = conditions.gameSoundOn ? ScriptSoundBank::InGame : BankOfSound(sound);
	const auto& handle = sounds.Handle(sound);
	if (SoundEffectHeard(conditions, bank, handle->userParam))
	{
		return true;
	}
	// The log may not be set up (tests)
	if (const auto logger = spdlog::get("audio"); logger != nullptr)
	{
		SPDLOG_LOGGER_DEBUG(logger, "Sound {} not heard (use {}): {}{}{}{}", handle->name, handle->userParam,
		                    conditions.scriptWideScreen ? "cut scene " : "", conditions.insideTemple ? "in the temple " : "",
		                    conditions.gameSoundOn ? "" : "game sound off ",
		                    conditions.creatureFightControl ? "fight controls" : "");
	}
	return false;
}

void GameSoundRequests::PlayGameEffect(entt::id_type sound, std::optional<glm::vec3> worldPosition)
{
	auto* const audio = Manager();
	if (audio != nullptr && IsGameEffectHeard(CurrentSoundEffectConditions(), sound))
	{
		audio->PlaySoundEffect(sound, worldPosition);
	}
}

entt::entity GameSoundRequests::StartGameEffect(entt::id_type sound, const SoundEffectOptions& options)
{
	auto* const audio = Manager();
	if (audio == nullptr || !IsGameEffectHeard(CurrentSoundEffectConditions(), sound))
	{
		return entt::null;
	}
	return audio->StartSoundEffect(sound, options);
}

entt::entity GameSoundRequests::StartSoundEffect(entt::id_type id, const SoundEffectOptions& options)
{
	auto* const audio = Manager();
	return audio != nullptr ? audio->StartSoundEffect(id, options) : entt::null;
}

void GameSoundRequests::StopSoundEffect(entt::id_type id)
{
	if (auto* const audio = Manager(); audio != nullptr)
	{
		audio->StopSoundEffect(id);
	}
}

bool GameSoundRequests::IsSoundEffectPlaying(entt::id_type id) const
{
	const auto* const audio = Manager();
	return audio != nullptr && audio->IsSoundEffectPlaying(id);
}

void GameSoundRequests::StopAllSoundEffects()
{
	if (auto* const audio = Manager(); audio != nullptr)
	{
		audio->StopAllSoundEffects();
	}
}

entt::entity GameSoundRequests::CreateEmitter(entt::id_type id, std::optional<glm::vec3> worldPosition, PlayType playType,
                                              entt::entity owner)
{
	auto* const audio = Manager();
	return audio != nullptr ? audio->CreateEmitter(id, worldPosition, playType, owner) : entt::null;
}

entt::entity GameSoundRequests::CreateOwnSoundEmitter(entt::id_type id, std::optional<glm::vec3> worldPosition,
                                                      PlayType playType)
{
	auto* const audio = Manager();
	return audio != nullptr ? audio->CreateOwnSoundEmitter(id, worldPosition, playType) : entt::null;
}

AnimEffectPlay GameSoundRequests::PlayAnimEffect(const std::string& bankName, std::span<const int32_t> keys, entt::entity owner,
                                                 const glm::vec3& position,
                                                 const std::optional<SoundEffectConditions>& heardUnder,
                                                 AnimEffectMeasure measure)
{
	auto* const audio = Manager();
	return audio != nullptr ? audio->PlayAnimEffect(bankName, keys, owner, position, heardUnder, measure) : AnimEffectPlay {};
}

void GameSoundRequests::EndAnimEffect(const std::string& bankName, std::span<const int32_t> keys, entt::entity owner,
                                      const glm::vec3& position, AnimEffectEnd end, AnimEffectMeasure measure)
{
	if (auto* const audio = Manager(); audio != nullptr)
	{
		audio->EndAnimEffect(bankName, keys, owner, position, end, measure);
	}
}

bool GameSoundRequests::EmitterExists(entt::entity emitter)
{
	auto* const audio = Manager();
	return audio != nullptr && audio->EmitterExists(emitter);
}

void GameSoundRequests::PlayEmitter(entt::entity emitter)
{
	if (auto* const audio = Manager(); audio != nullptr)
	{
		audio->PlayEmitter(emitter);
	}
}

void GameSoundRequests::StopEmitter(entt::entity emitter)
{
	if (auto* const audio = Manager(); audio != nullptr)
	{
		audio->StopEmitter(emitter);
	}
}

void GameSoundRequests::DestroyEmitter(entt::entity emitter)
{
	if (auto* const audio = Manager(); audio != nullptr)
	{
		audio->DestroyEmitter(emitter);
	}
}

float GameSoundRequests::GetProgress(entt::entity emitter)
{
	auto* const audio = Manager();
	return audio != nullptr ? audio->GetProgress(emitter) : 1.0f;
}

AudioStatus GameSoundRequests::GetStatus(entt::entity emitter)
{
	auto* const audio = Manager();
	return audio != nullptr ? audio->GetStatus(emitter) : AudioStatus {};
}

void GameSoundRequests::SetEmitterPosition(entt::entity emitter, const glm::vec3& position)
{
	if (auto* const audio = Manager(); audio != nullptr)
	{
		audio->SetEmitterPosition(emitter, position);
	}
}

void GameSoundRequests::SetEmitterFollows(entt::entity emitter, Follows follows)
{
	if (auto* const audio = Manager(); audio != nullptr)
	{
		audio->SetEmitterFollows(emitter, follows);
	}
}

void GameSoundRequests::ReleaseEmitterLoop(entt::entity emitter)
{
	if (auto* const audio = Manager(); audio != nullptr)
	{
		audio->ReleaseEmitterLoop(emitter);
	}
}

bool GameSoundRequests::IsEmitterLooping(entt::entity emitter)
{
	auto* const audio = Manager();
	return audio != nullptr && audio->IsEmitterLooping(emitter);
}

void GameSoundRequests::SetEmitterVolume(entt::entity emitter, uint32_t volume)
{
	if (auto* const audio = Manager(); audio != nullptr)
	{
		audio->SetEmitterVolume(emitter, volume);
	}
}

uint32_t GameSoundRequests::GetEmitterVolume(entt::entity emitter)
{
	auto* const audio = Manager();
	return audio != nullptr ? audio->GetEmitterVolume(emitter) : 0;
}

void GameSoundRequests::SetEmitterPitch(entt::entity emitter, uint32_t pitchPercent)
{
	if (auto* const audio = Manager(); audio != nullptr)
	{
		audio->SetEmitterPitch(emitter, pitchPercent);
	}
}

void GameSoundRequests::StopOwnedSounds(entt::entity owner)
{
	if (auto* const audio = Manager(); audio != nullptr)
	{
		audio->StopOwnedSounds(owner);
	}
}

void GameSoundRequests::ForgetSavedEmitters()
{
	if (auto* const audio = Manager(); audio != nullptr)
	{
		audio->ForgetSavedEmitters();
	}
}

Listener GameSoundRequests::GetListener() const
{
	const auto* const audio = Manager();
	return audio != nullptr ? audio->GetListener() : CameraListener();
}

void GameSoundRequests::PlaceOwnedEmitters(const std::function<std::optional<glm::vec3>(entt::entity owner)>& placeOf)
{
	auto* const audio = Manager();
	auto* const emitters = audio != nullptr ? audio->Emitters() : nullptr;
	if (emitters == nullptr)
	{
		return;
	}
	emitters->Each<AudioEmitter>([&placeOf](AudioEmitter& emitter) {
		if (emitter.owner == entt::null)
		{
			return;
		}
		if (const auto place = placeOf(emitter.owner); place.has_value())
		{
			emitter.position = *place;
		}
	});
}

void GameSoundRequests::StopAllSounds()
{
	if (auto* const audio = Manager(); audio != nullptr)
	{
		audio->Stop();
	}
}

void GameSoundRequests::ProcessAtmosphere(bool active)
{
	if (auto* const audio = Manager(); audio != nullptr)
	{
		audio->AtmosProcess(active);
	}
}

MusicPlayOptions ToMusicPlayOptions(const MusicRequest& request)
{
	return {
	    .bank = nullptr,
	    .volume = request.volume,
	    .startChunk = request.startChunk,
	    .startFrame = request.startFrame,
	    .loops = request.loops,
	    .sync = request.sync,
	    .fadeIn = request.fadeIn,
	    .pitchPercent = request.pitchPercent,
	    .position = request.position,
	    .minDistance = request.minDistance,
	    .maxDistance = request.maxDistance,
	    .distanceScale = request.distanceScale,
	    .onFinished = request.onFinished,
	};
}

bool GameSoundRequests::MusicPlay(const std::string& bankPath, const MusicRequest& request)
{
	auto* const audio = Manager();
	return audio != nullptr && audio->MusicPlay(bankPath, ToMusicPlayOptions(request));
}

void GameSoundRequests::MusicStop(bool fadeOut)
{
	if (auto* const audio = Manager(); audio != nullptr)
	{
		audio->MusicStop(fadeOut);
	}
}

void GameSoundRequests::MusicStopBank(const std::string& bankPath, bool fadeOut)
{
	if (auto* const audio = Manager(); audio != nullptr)
	{
		audio->MusicStopBank(bankPath, fadeOut);
	}
}

std::optional<MusicBankInfo> GameSoundRequests::GetMusicBankInfo(const std::string& bankPath)
{
	auto* const audio = Manager();
	return audio != nullptr ? audio->GetMusicBankInfo(bankPath) : std::nullopt;
}

std::vector<MusicChannelPlace> GameSoundRequests::GetPlayingMusicChannels() const
{
	std::vector<MusicChannelPlace> playing;
	const auto* const audio = Manager();
	const auto* const music = audio != nullptr ? audio->GetMusic() : nullptr;
	if (music == nullptr)
	{
		return playing;
	}
	for (const auto& channel : music->GetChannels())
	{
		if (channel.active)
		{
			playing.push_back({.groupId = channel.groupId, .playingChunk = channel.playingChunk});
		}
	}
	return playing;
}

CustomMusicPlayerInterface* GameSoundRequests::GetCustomMusic()
{
	auto* const audio = Manager();
	return audio != nullptr ? audio->GetCustomMusic() : nullptr;
}

void GameSoundRequests::AddMusicEntry(const std::string& name)
{
	if (auto* const audio = Manager(); audio != nullptr)
	{
		audio->AddMusicEntry(name);
	}
}

void GameSoundRequests::CreateSoundGroup(const std::string& name)
{
	if (auto* const audio = Manager(); audio != nullptr)
	{
		audio->CreateSoundGroup(name);
	}
}

void GameSoundRequests::AddToSoundGroup(const std::string& name, entt::id_type id)
{
	if (auto* const audio = Manager(); audio != nullptr)
	{
		audio->AddToSoundGroup(name, id);
	}
}

void GameSoundRequests::AddAnimEffects(const std::string& bankName, AnimEffectTable table)
{
	if (auto* const audio = Manager(); audio != nullptr)
	{
		audio->AddAnimEffects(bankName, std::move(table));
	}
}

const std::map<std::string, SoundGroup>& GameSoundRequests::GetSoundGroups()
{
	static const std::map<std::string, SoundGroup> k_NoGroups;
	auto* const audio = Manager();
	return audio != nullptr ? audio->GetSoundGroups() : k_NoGroups;
}

float GameSoundRequests::GetSfxVolume()
{
	auto* const audio = Manager();
	return audio != nullptr ? audio->GetSfxVolume() : 0.0f;
}

float GameSoundRequests::GetMusicVolume()
{
	auto* const audio = Manager();
	return audio != nullptr ? audio->GetMusicVolume() : 0.0f;
}

} // namespace openblack::audio
