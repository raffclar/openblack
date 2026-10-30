/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Sound/SoundRequestInterface.h"

namespace openblack::audio
{

struct MusicPlayOptions;

/// The audio library's answer to the sound requests of the world. Each request goes at once to the audio manager
/// (Locator::audio), so the sounds play as they did before the world used the requests. With no audio manager, nothing
/// plays and each question gets the answer of a computer without sound.
class GameSoundRequests final: public SoundRequestInterface
{
public:
	[[nodiscard]] bool IsInstalled() const override;
	[[nodiscard]] bool IsDeviceLost() const override;

	[[nodiscard]] bool IsGameEffectHeard(const SoundEffectConditions& conditions, entt::id_type sound) const override;
	void PlayGameEffect(entt::id_type sound, std::optional<glm::vec3> worldPosition) override;
	entt::entity StartGameEffect(entt::id_type sound, const SoundEffectOptions& options) override;

	entt::entity StartSoundEffect(entt::id_type id, const SoundEffectOptions& options) override;
	void StopSoundEffect(entt::id_type id) override;
	[[nodiscard]] bool IsSoundEffectPlaying(entt::id_type id) const override;
	void StopAllSoundEffects() override;

	entt::entity CreateEmitter(entt::id_type id, std::optional<glm::vec3> worldPosition, PlayType playType,
	                           entt::entity owner) override;
	entt::entity CreateOwnSoundEmitter(entt::id_type id, std::optional<glm::vec3> worldPosition, PlayType playType) override;

	using SoundRequestInterface::PlayAnimEffect;
	AnimEffectPlay PlayAnimEffect(const std::string& bankName, std::span<const int32_t> keys, entt::entity owner,
	                              const glm::vec3& position, const std::optional<SoundEffectConditions>& heardUnder,
	                              AnimEffectMeasure measure) override;
	void EndAnimEffect(const std::string& bankName, std::span<const int32_t> keys, entt::entity owner,
	                   const glm::vec3& position, AnimEffectEnd end, AnimEffectMeasure measure) override;

	bool EmitterExists(entt::entity emitter) override;
	void PlayEmitter(entt::entity emitter) override;
	void StopEmitter(entt::entity emitter) override;
	void DestroyEmitter(entt::entity emitter) override;
	[[nodiscard]] float GetProgress(entt::entity emitter) override;
	[[nodiscard]] AudioStatus GetStatus(entt::entity emitter) override;
	void SetEmitterPosition(entt::entity emitter, const glm::vec3& position) override;
	void SetEmitterFollows(entt::entity emitter, Follows follows) override;
	void ReleaseEmitterLoop(entt::entity emitter) override;
	[[nodiscard]] bool IsEmitterLooping(entt::entity emitter) override;
	void SetEmitterVolume(entt::entity emitter, uint32_t volume) override;
	[[nodiscard]] uint32_t GetEmitterVolume(entt::entity emitter) override;
	void SetEmitterPitch(entt::entity emitter, uint32_t pitchPercent) override;
	void StopOwnedSounds(entt::entity owner) override;
	void ForgetSavedEmitters() override;

	void PlaceOwnedEmitters(const std::function<std::optional<glm::vec3>(entt::entity owner)>& placeOf) override;

	[[nodiscard]] Listener GetListener() const override;

	void StopAllSounds() override;
	void ProcessAtmosphere(bool active) override;

	bool MusicPlay(const std::string& bankPath, const MusicRequest& request) override;
	void MusicStop(bool fadeOut) override;
	void MusicStopBank(const std::string& bankPath, bool fadeOut) override;
	[[nodiscard]] std::optional<MusicBankInfo> GetMusicBankInfo(const std::string& bankPath) override;
	[[nodiscard]] std::vector<MusicChannelPlace> GetPlayingMusicChannels() const override;
	[[nodiscard]] CustomMusicPlayerInterface* GetCustomMusic() override;

	void AddMusicEntry(const std::string& name) override;
	void CreateSoundGroup(const std::string& name) override;
	void AddToSoundGroup(const std::string& name, entt::id_type id) override;
	void AddAnimEffects(const std::string& bankName, AnimEffectTable table) override;
	[[nodiscard]] const std::map<std::string, SoundGroup>& GetSoundGroups() override;

	[[nodiscard]] float GetSfxVolume() override;
	[[nodiscard]] float GetMusicVolume() override;

	[[nodiscard]] bool HasAudio() const override;
	void SetListenerOverride(std::optional<Listener> listener) override;
	[[nodiscard]] float GetGlobalVolume() override;
	[[nodiscard]] bool IsMusicActive() const override;
	[[nodiscard]] std::vector<MusicChannelState> GetMusicChannelStates() const override;
	void SimulateDeviceLoss(bool lost) override;
	[[nodiscard]] const ecs::Registry* GetEmitters() override;
};

/// The options of the music player for a music request of the world. The bank stays empty: the player finds the bank
/// from its path.
[[nodiscard]] MusicPlayOptions ToMusicPlayOptions(const MusicRequest& request);

} // namespace openblack::audio
