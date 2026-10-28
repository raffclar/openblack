/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>
#include <source_location>
#include <string_view>

#include "AudioManagerInterface.h"

namespace openblack
{
class MainThreadQueue;
}

namespace openblack::audio
{

/// The audio as the game uses it: each call goes to the audio of the machine through the queue of the main thread.
///
/// The turn holds the audio while it runs (see HeldByTurn): each call of the turn goes to the audio at once, on the
/// thread of the turn, in the order of the calls, also when the turn runs on another thread. A call that gives back
/// nothing goes through the queue, so it waits only under a deferral of the main thread. While a turn runs on another
/// thread, the main thread must not use the audio, and the check of the turn's calls counts such a use.
///
/// Every function of the interface is passed on here, also the ones that have a default. A new function of the
/// interface must be passed on here too, or the audio of the machine does not get it.
class QueuedAudioManager final: public AudioManagerInterface
{
public:
	QueuedAudioManager(std::unique_ptr<AudioManagerInterface> audio, MainThreadQueue& queue);
	QueuedAudioManager(const QueuedAudioManager&) = delete;
	QueuedAudioManager& operator=(const QueuedAudioManager&) = delete;
	~QueuedAudioManager() override;

	/// The audio of the machine that the calls go to
	[[nodiscard]] AudioManagerInterface& GetAudio() { return *_audio; }

	[[nodiscard]] bool IsInstalled() const override;
	[[nodiscard]] bool IsDeviceLost() const override;
	void SimulateDeviceLoss(bool lost) override;
	void Stop() override;
	void Update() override;
	void ProcessTurn() override;
	void StopOutOfRangeEmitters() override;
	BufferId CreateBuffer(ChannelLayout layout, const std::vector<int16_t>& buffer, int sampleRate) override;
	void CreateBuffer(Sound& sound) override;
	void PlayEmitter(entt::entity emitter) override;
	void PauseEmitter(entt::entity emitter) override;
	void StopEmitter(entt::entity emitter) override;
	void DestroyEmitter(entt::entity emitter) override;
	void ForgetSavedEmitters() override;
	[[nodiscard]] ecs::Registry* Emitters() override;
	entt::entity CreateEmitter(entt::id_type id, std::optional<glm::vec3> worldPosition, PlayType playType,
	                           entt::entity owner) override;
	entt::entity CreateOwnSoundEmitter(entt::id_type id, std::optional<glm::vec3> worldPosition, PlayType playType) override;
	bool EmitterExists(entt::entity emitter) override;
	[[nodiscard]] float GetProgress(entt::entity emitter) override;
	[[nodiscard]] AudioStatus GetStatus(entt::entity emitter) override;
	void SetGlobalVolume(float volume) override;
	void SetSfxVolume(float volume) override;
	void SetMusicVolume(float volume) override;
	[[nodiscard]] float GetGlobalVolume() override;
	[[nodiscard]] float GetSfxVolume() override;
	[[nodiscard]] float GetMusicVolume() override;
	void PlayMusic(const std::string& packPath, PlayType type) override;
	void StopMusic() override;
	bool MusicPlay(const std::string& bankPath, const MusicPlayOptions& options) override;
	void MusicStop(bool fadeOut) override;
	void MusicStopBank(const std::string& bankPath, bool fadeOut) override;
	[[nodiscard]] bool MusicIsActive() const override;
	[[nodiscard]] std::optional<MusicBankInfo> GetMusicBankInfo(const std::string& bankPath) override;
	[[nodiscard]] const MusicPlayer* GetMusic() const override;
	void PlaySound(entt::id_type id, PlayType type) override;
	void PlaySoundEffect(entt::id_type id, std::optional<glm::vec3> worldPosition) override;
	void StopSoundEffect(entt::id_type id) override;
	[[nodiscard]] bool IsSoundEffectPlaying(entt::id_type id) const override;
	entt::entity StartSoundEffect(entt::id_type id, const SoundEffectOptions& options) override;
	void SetEmitterPosition(entt::entity emitter, const glm::vec3& position) override;
	void SetEmitterFollows(entt::entity emitter, Follows follows) override;
	void ReleaseEmitterLoop(entt::entity emitter) override;
	[[nodiscard]] bool IsEmitterLooping(entt::entity emitter) override;
	void SetEmitterVolume(entt::entity emitter, uint32_t volume) override;
	void SetEmitterPitch(entt::entity emitter, uint32_t pitchPercent) override;
	[[nodiscard]] uint32_t GetEmitterVolume(entt::entity emitter) override;
	void StopOwnedSounds(entt::entity owner) override;
	void StopAllSoundEffects() override;
	void AddAnimEffects(const std::string& bankName, AnimEffectTable table) override;
	using AudioManagerInterface::PlayAnimEffect;
	AnimEffectPlay PlayAnimEffect(const std::string& bankName, std::span<const int32_t> keys, entt::entity owner,
	                              const glm::vec3& position, const std::optional<SoundEffectConditions>& heardUnder,
	                              AnimEffectMeasure measure) override;
	void EndAnimEffect(const std::string& bankName, std::span<const int32_t> keys, entt::entity owner,
	                   const glm::vec3& position, AnimEffectEnd end, AnimEffectMeasure measure) override;
	const Sound& GetSound(entt::id_type id) override;
	void CreateSoundGroup(const std::string& name) override;
	void AddToSoundGroup(const std::string& name, entt::id_type id) override;
	const SoundGroup& GetSoundGroup(const std::string& name) override;
	const std::map<std::string, SoundGroup>& GetSoundGroups() override;
	void AddMusicEntry(const std::string& name) override;
	[[nodiscard]] const std::vector<std::string>& GetMusicTracks() const override;
	uint32_t AtmosRegisterBank(const std::string& bankName, const std::vector<pack::AudioBankSampleHeader>& headers,
	                           uint16_t atmosCount) override;
	void AtmosReleaseBank(uint32_t bank) override;
	void AtmosSetBankVolume(uint32_t bank, int32_t volume) override;
	void AtmosSetGroup(uint32_t bank, uint32_t group) override;
	void AtmosProcess(bool active) override;
	[[nodiscard]] const AtmosPlayer* GetAtmos() const override;
	[[nodiscard]] Listener GetListener() const override;
	void SetListenerOverride(std::optional<Listener> listener) override;
	[[nodiscard]] CustomMusicPlayerInterface* GetCustomMusic() override;

private:
	/// Gives a call that gives back nothing to the queue of the main thread
	template <typename Call>
	void Post(Call&& call, std::source_location where = std::source_location::current());
	/// A call that gives back a value goes to the audio at once
	void Direct(std::string_view what, std::source_location where = std::source_location::current()) const;

	std::unique_ptr<AudioManagerInterface> _audio;
	MainThreadQueue& _queue;
};

} // namespace openblack::audio
