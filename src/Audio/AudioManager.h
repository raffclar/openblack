/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "AtmosPlayer.h"
#include "AudioDecoderInterface.h"
#include "AudioManagerInterface.h"
#include "AudioPlayer.h"
#include "CustomMusicPlayer.h"
#include "MusicPlayer.h"
#include "MusicStreamBackend.h"
#include "SoundGroup.h"
#include "VoicePool.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack
{
class Game;
}

namespace openblack::ecs::components
{
struct AudioEmitter;
}

namespace openblack::audio
{

class AudioManager final: public AudioManagerInterface, private VoiceBackend
{
public:
	AudioManager();
	/// The manager plays its sounds through this player
	explicit AudioManager(std::unique_ptr<AudioPlayerInterface> player);
	~AudioManager();
	BufferId CreateBuffer(ChannelLayout layout, const std::vector<int16_t>& buffer, int sampleRate) override;
	void CreateBuffer(Sound& sound) override;
	void PlayEmitter(entt::entity emitter) override;
	void PauseEmitter(entt::entity emitter) override;
	void StopEmitter(entt::entity emitter) override;
	void DestroyEmitter(entt::entity emitter) override;
	entt::entity CreateEmitter(entt::id_type id, std::optional<glm::vec3> worldPosition, PlayType playType,
	                           entt::entity owner) override;
	[[nodiscard]] bool EmitterExists(entt::entity emitter) override;
	[[nodiscard]] float GetProgress(entt::entity entity) override;
	[[nodiscard]] AudioStatus GetStatus(entt::entity emitter) override;
	void PlayMusic(const std::string& packPath, PlayType type) override;
	void StopMusic() override;
	bool MusicPlay(const std::string& bankPath, const MusicPlayOptions& options) override;
	void MusicStop(bool fadeOut) override;
	void MusicStopBank(const std::string& bankPath, bool fadeOut) override;
	[[nodiscard]] bool MusicIsActive() const override;
	[[nodiscard]] std::optional<MusicBankInfo> GetMusicBankInfo(const std::string& bankPath) override;
	[[nodiscard]] const MusicPlayer* GetMusic() const override { return _musicPlayer.get(); }
	const Sound& GetSound(entt::id_type id) override;
	void PlaySound(entt::id_type id, PlayType type) override;
	void PlaySoundEffect(entt::id_type id, std::optional<glm::vec3> worldPosition) override;
	void StopSoundEffect(entt::id_type id) override;
	[[nodiscard]] bool IsSoundEffectPlaying(entt::id_type id) const override;
	[[nodiscard]] bool IsInstalled() const override { return true; }
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
	void SetGlobalVolume(float volume) override { _globalVolume = volume; }
	void SetSfxVolume(float volume) override { _sfxVolume = volume; }
	void SetMusicVolume(float volume) override { _musicVolume = volume; }
	[[nodiscard]] float GetGlobalVolume() override { return _globalVolume; }
	[[nodiscard]] float GetSfxVolume() override { return _sfxVolume; }
	[[nodiscard]] float GetMusicVolume() override { return _musicVolume; }
	void Stop() override;
	void Update() override;
	void StopOutOfRangeEmitters() override;
	[[nodiscard]] Listener GetListener() const override { return _listenerOverride.value_or(CameraListener()); }
	void SetListenerOverride(std::optional<Listener> listener) override { _listenerOverride = listener; }
	void CreateSoundGroup(const std::string& name) override;
	void AddMusicEntry(const std::string& name) override;
	[[nodiscard]] const std::vector<std::string>& GetMusicTracks() const override { return _music; }
	void AddToSoundGroup(const std::string& name, entt::id_type id) override;
	const SoundGroup& GetSoundGroup(const std::string& name) override;
	const std::map<std::string, SoundGroup>& GetSoundGroups() override;
	uint32_t AtmosRegisterBank(const std::string& bankName, const std::vector<pack::AudioBankSampleHeader>& headers,
	                           uint16_t atmosCount) override;
	void AtmosReleaseBank(uint32_t bank) override;
	void AtmosSetBankVolume(uint32_t bank, int32_t volume) override;
	void AtmosSetGroup(uint32_t bank, uint32_t group) override;
	void AtmosProcess(bool active) override;
	[[nodiscard]] const AtmosPlayer* GetAtmos() const override { return _atmos.get(); }
	[[nodiscard]] CustomMusicPlayerInterface* GetCustomMusic() override { return _customMusic.get(); }

private:
	// VoiceBackend
	[[nodiscard]] Handle Start(const VoiceStart& start) override;
	[[nodiscard]] bool IsPlaying(Handle handle) const override;
	void SetVolume(Handle handle, uint32_t volume) override;
	void Stop(Handle handle) override;

	struct AtmosVoice
	{
		SourceId source;
		float gain;
		/// The voice of the pool that the sample plays on
		size_t slot;
	};

	/// What plays on a voice of the pool
	struct VoiceOccupant
	{
		entt::entity emitter {entt::null};
		Handle atmos {k_InvalidHandle};
		/// The voice forgets its sample when the sample stops
		bool forgetSample {false};
	};

	/// The voice for a sample by the voice rules. When the sample starts, the voice stops the sample that played on it.
	/// The bank header gives the play type when it overrides it, else the caller does.
	[[nodiscard]] VoiceChoice AcquireVoice(const Sound& sound, uint64_t ownerKey, pack::AudioBankLoop callerPlayType);
	/// Puts what plays now on the voice
	void BindVoice(size_t slot, const Sound& sound, entt::entity emitter, Handle atmos);
	/// Frees the voice of what stopped
	void ReleaseVoice(size_t slot);
	/// The voice of an emitter, empty when the emitter has no voice
	[[nodiscard]] std::optional<size_t> VoiceOf(entt::entity emitter) const;
	/// The key of an owner in the voice pool
	[[nodiscard]] static uint64_t OwnerKey(entt::entity owner);
	/// The key of the owner of a sound effect in the voice pool, by whose sound it is
	[[nodiscard]] uint64_t OwnerKey(const SoundEffectOptions& options);
	/// The bank of a loaded sound, 0 when the sound is not loaded
	[[nodiscard]] static uint32_t BankOf(entt::id_type id);
	/// An emitter for a sound on a voice of the pool, null when the sound does not start
	entt::entity CreateVoiceEmitter(entt::id_type id, std::optional<glm::vec3> worldPosition, PlayType playType,
	                                entt::entity owner, pack::AudioBankLoop callerPlayType, VoiceChoice& choice);

	/// A source for a voice of a loaded sound, decoding the sound first if needed
	SourceId CreateSource(Sound& sound, const VoiceStart& start);
	/// Places a source in LHAudio's listener frame, or centres it
	void PositionSource(SourceId source, std::optional<glm::vec3> listenerPosition);
	/// The volume of an emitter's source: its gain with the volume settings, or none past its maximum distance
	[[nodiscard]] float EmitterVolume(const ecs::components::AudioEmitter& emitter) const;
	/// LHSamplePlay's play parameters for a sample, with the bank header overriding LH_SamplePlayOptions' defaults
	[[nodiscard]] static VoiceStart MakeVoiceStart(const Sound& sound, std::optional<glm::vec3> worldPosition,
	                                               PlayType playType);
	/// An emitter playing for owner a sound of bank that is id or else in group, null if there is none
	[[nodiscard]] entt::entity FindPlaying(entt::entity owner, entt::id_type bank, entt::id_type id, uint16_t group) const;
	/// A music bank by its path relative to the game: its headers are read the first time, and it is kept from then on,
	/// its chunks left in its file. Null if it couldn't be read.
	[[nodiscard]] std::shared_ptr<const MusicBank> LoadMusicBank(const std::string& bankPath);
	/// LHAudio's listener frame (x right, y forward, z up) for a world position
	[[nodiscard]] static glm::vec3 ToListenerFrame(glm::vec3 worldPosition);

	std::unique_ptr<AudioPlayerInterface> _audioPlayer;
	/// The listener while it is put somewhere other than the camera
	std::optional<Listener> _listenerOverride;
	/// All sounds are loaded
	std::map<std::string, SoundGroup> _soundGroups;
	/// Animation effects of the sound banks, by bank name
	std::map<std::string, AnimEffectTable> _animEffects;
	/// Music resources are loaded on demand to avoid storing large audio buffers. There are no resource IDs yet
	std::vector<std::string> _music;
	float _globalVolume {1.0f};
	float _musicVolume {1.0f};
	float _sfxVolume {1.0f};
	std::unique_ptr<MusicStreamBackend> _musicStreams;
	std::unique_ptr<MusicPlayer> _musicPlayer;
	/// The music banks registered, by path, null where one couldn't be read
	std::map<std::string, std::shared_ptr<const MusicBank>> _musicBanks;
	std::chrono::steady_clock::time_point _lastMusicUpdate {std::chrono::steady_clock::now()};
	std::map<Handle, AtmosVoice> _atmosVoices;
	/// Decoded samples, kept for the lifetime of the audio manager
	std::vector<BufferId> _decodedBuffers;
	Handle _nextAtmosVoice {1};
	std::unique_ptr<AtmosPlayer> _atmos;
	std::mt19937 _customMusicRandom;
	std::unique_ptr<CustomMusicPlayer> _customMusic;
	/// The voices that all the bank samples share, and what plays on each voice
	VoicePool _voicePool;
	std::array<VoiceOccupant, VoicePool::k_VoiceCount> _voiceOccupants {};
	/// Counts the sounds that have an owner of their own
	uint64_t _ownSoundSerial {0};
};

} // namespace openblack::audio
