/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <entt/fwd.hpp>

#include "AudioDecoderInterface.h"
#include "AudioPlayerInterface.h"
#include "ECS/Components/AudioEmitter.h"
#include "MusicPlayer.h"
#include "Sound.h"
#include "Sound/AnimEffectTable.h"
#include "Sound/EmitterFollow.h"
#include "Sound/HearingRange.h"
#include "Sound/ListenerFrame.h"
#include "Sound/MusicRequest.h"
#include "Sound/ScriptSoundEffect.h"
#include "Sound/SoundGroup.h"
#include "Sound/SoundRequestInterface.h"
#include "Sound/VoiceOwner.h"

namespace openblack
{

namespace ecs
{
class Registry;
}

namespace pack
{
struct AudioBankSampleHeader;
}

namespace audio
{

class AtmosPlayer;
class CustomMusicPlayerInterface;

class AudioManagerInterface
{
public:
	virtual ~AudioManagerInterface() = default;

	/// The game has a working sound device; the fallback without one plays nothing
	[[nodiscard]] virtual bool IsInstalled() const = 0;
	/// The machine lost the sound device after the game opened it. The game then continues as if the device is still
	/// there, and plays no sound.
	[[nodiscard]] virtual bool IsDeviceLost() const { return false; }
	/// For tools only, not a rule of the game: the game acts as if the machine lost the sound device, or has it again
	virtual void SimulateDeviceLoss(bool /*lost*/) {}
	virtual void Stop() = 0;
	virtual void Update() = 0;
	/// Once a game turn, at its end: the listener takes the camera's place, and every placed sound is put where it is
	/// now, as the game does
	virtual void ProcessTurn() {}
	/// The game's audio turn. Each 3D sound that follows moves to the point of its owner or to the listener. It stops
	/// when its owner is gone or has no point, or when the camera is at or beyond the maximum distance of its voice.
	virtual void StopOutOfRangeEmitters() = 0;
	virtual BufferId CreateBuffer(ChannelLayout layout, const std::vector<int16_t>& buffer, int sampleRate) = 0;
	virtual void CreateBuffer(Sound& sound) = 0;
	virtual void PlayEmitter(entt::entity emitter) = 0;
	virtual void PauseEmitter(entt::entity emitter) = 0;
	virtual void StopEmitter(entt::entity emitter) = 0;
	virtual void DestroyEmitter(entt::entity emitter) = 0;
	/// A saved game has loaded. The sounds that played at the save do not come back: each emitter that the save kept
	/// goes, and no sound of its own is stopped, because the sounds stopped as the last land went.
	virtual void ForgetSavedEmitters() {}
	/// The entities of the sounds that play. A sound only shows the world on this computer, so its entity is not in the
	/// registry of the world: two computers, or a server with no sound, keep the same entities in the world. None when
	/// the manager plays no sound.
	[[nodiscard]] virtual ecs::Registry* Emitters() { return nullptr; }
	/// An emitter for a loaded sound, ready to play. With a world position it is 3D audio sounding from there,
	/// otherwise 2D audio centred on the listener. Repeat loops it forever, otherwise the bank header's loop count
	/// applies. It gets a voice by the restart play type for its owner, unless its bank header gives another.
	virtual entt::entity CreateEmitter(entt::id_type id, std::optional<glm::vec3> worldPosition, PlayType playType,
	                                   entt::entity owner) = 0;
	/// An emitter as CreateEmitter makes it, for a sound that no entity owns. The voice rules give it an owner that no
	/// other sound has, so it never finds the voice of another sound.
	virtual entt::entity CreateOwnSoundEmitter(entt::id_type id, std::optional<glm::vec3> worldPosition, PlayType playType)
	{
		return CreateEmitter(id, worldPosition, playType, entt::null);
	}
	virtual bool EmitterExists(entt::entity emitter) = 0;
	[[nodiscard]] virtual float GetProgress(entt::entity emitter) = 0;
	[[nodiscard]] virtual AudioStatus GetStatus(entt::entity emitter) = 0;
	virtual void SetGlobalVolume(float volume) = 0;
	virtual void SetSfxVolume(float volume) = 0;
	virtual void SetMusicVolume(float volume) = 0;
	[[nodiscard]] virtual float GetGlobalVolume() = 0;
	[[nodiscard]] virtual float GetSfxVolume() = 0;
	[[nodiscard]] virtual float GetMusicVolume() = 0;
	/// Plays a music bank on its own, from the start, stopping any other music at once
	virtual void PlayMusic(const std::string& packPath, PlayType type) = 0;
	virtual void StopMusic() = 0;
	/// Plays the music bank at a path, loading it if it is not playing already. options.bank is ignored.
	virtual bool MusicPlay(const std::string& bankPath, const MusicPlayOptions& options) = 0;
	/// Stops the music, fading it out if asked
	virtual void MusicStop(bool fadeOut) = 0;
	/// Stops the music bank at a path: at once, or when it is the music heard all the music fades out if asked to fade
	virtual void MusicStopBank(const std::string& bankPath, bool fadeOut) = 0;
	/// Whether music is playing
	[[nodiscard]] virtual bool MusicIsActive() const = 0;
	/// The music group and length of a bank, null if there is no such bank
	[[nodiscard]] virtual std::optional<MusicBankInfo> GetMusicBankInfo(const std::string& bankPath) = 0;
	/// Null when there is no audio device
	[[nodiscard]] virtual const MusicPlayer* GetMusic() const = 0;
	/// Plays a sound as 2D audio
	virtual void PlaySound(entt::id_type id, PlayType type) = 0;
	/// A one-shot sound effect played the way the game plays its sound effects: volume, pitch, loop and distances come
	/// from the bank header where it overrides them. With a world position it is a 3D sound anchored there, otherwise
	/// it is centred on the listener.
	virtual void PlaySoundEffect(entt::id_type id, std::optional<glm::vec3> worldPosition) = 0;
	/// Stops the sound effects of a sound that PlaySoundEffect started, as one that loops forever goes on until then
	virtual void StopSoundEffect(entt::id_type id) = 0;
	/// A sound effect PlaySoundEffect started is still playing
	[[nodiscard]] virtual bool IsSoundEffectPlaying(entt::id_type id) const = 0;
	/// A sound effect played with options: its emitter, null when nothing plays (not loaded, or too far away)
	virtual entt::entity StartSoundEffect(entt::id_type id, const SoundEffectOptions& options) = 0;
	/// Moves a 3D emitter, as a loop follows its owner
	virtual void SetEmitterPosition(entt::entity emitter, const glm::vec3& position) = 0;
	/// Sets what a 3D emitter does in each audio turn (see StopOutOfRangeEmitters)
	virtual void SetEmitterFollows(entt::entity emitter, Follows follows) = 0;
	/// A looping emitter stops looping: the pass playing finishes and the emitter stops
	virtual void ReleaseEmitterLoop(entt::entity emitter) = 0;
	/// Whether an emitter loops until stopped
	[[nodiscard]] virtual bool IsEmitterLooping(entt::entity emitter) = 0;
	/// An emitter's volume, 0 to 127, as a fade sets it
	virtual void SetEmitterVolume(entt::entity emitter, uint32_t volume) = 0;
	/// Its playback rate in percent, as the mixer sets a playing sample's pitch
	virtual void SetEmitterPitch(entt::entity emitter, uint32_t pitchPercent) = 0;
	[[nodiscard]] virtual uint32_t GetEmitterVolume(entt::entity emitter) = 0;
	/// Stops every sound played for an owner
	virtual void StopOwnedSounds(entt::entity owner) = 0;
	/// Stops every sound effect playing (not the music)
	virtual void StopAllSoundEffects() = 0;
	/// The animation effects of a loaded sound bank, named as its sounds are ("<bank>/<sample id>")
	virtual void AddAnimEffects(const std::string& bankName, AnimEffectTable table) = 0;
	/// One of the samples a bank's animation effects pick for keys, chosen at random, as
	/// a one-shot 3D sound at position on behalf of owner. Nothing plays when the camera is further than the hearing
	/// limit or the sample's maximum distance (measured as the measure tells), or when the bank header plays the
	/// sample once and owner is playing it, or another of its voice group, already. Given conditions, nothing plays
	/// when the picked sample may not be heard in them (by its user parameter, as the scripts' sound effects).
	virtual AnimEffectPlay PlayAnimEffect(const std::string& bankName, std::span<const int32_t> keys, entt::entity owner,
	                                      const glm::vec3& position, const std::optional<SoundEffectConditions>& heardUnder,
	                                      AnimEffectMeasure measure) = 0;
	/// The same, at the straight distance
	AnimEffectPlay PlayAnimEffect(const std::string& bankName, std::span<const int32_t> keys, entt::entity owner,
	                              const glm::vec3& position, const std::optional<SoundEffectConditions>& heardUnder)
	{
		return PlayAnimEffect(bankName, keys, owner, position, heardUnder, AnimEffectMeasure::Straight);
	}
	/// The same, whatever the conditions
	AnimEffectPlay PlayAnimEffect(const std::string& bankName, std::span<const int32_t> keys, entt::entity owner,
	                              const glm::vec3& position, AnimEffectMeasure measure)
	{
		return PlayAnimEffect(bankName, keys, owner, position, std::nullopt, measure);
	}
	/// The same, whatever the conditions, at the straight distance
	AnimEffectPlay PlayAnimEffect(const std::string& bankName, std::span<const int32_t> keys, entt::entity owner,
	                              const glm::vec3& position)
	{
		return PlayAnimEffect(bankName, keys, owner, position, std::nullopt, AnimEffectMeasure::Straight);
	}
	/// Ends the samples that a bank's animation effects pick for keys, if owner plays them. For each of these samples,
	/// the first emitter of owner that plays it stops or releases its loop. Nothing ends when the camera is further
	/// than the hearing limit. The maximum distance of the sample has no effect.
	virtual void EndAnimEffect(const std::string& bankName, std::span<const int32_t> keys, entt::entity owner,
	                           const glm::vec3& position, AnimEffectEnd end, AnimEffectMeasure measure) = 0;
	virtual const Sound& GetSound(entt::id_type id) = 0;
	virtual void CreateSoundGroup(const std::string& name) = 0;
	virtual void AddToSoundGroup(const std::string& name, entt::id_type id) = 0;
	virtual const SoundGroup& GetSoundGroup(const std::string& name) = 0;
	virtual const std::map<std::string, SoundGroup>& GetSoundGroups() = 0;
	virtual void AddMusicEntry(const std::string& name) = 0;
	[[nodiscard]] virtual const std::vector<std::string>& GetMusicTracks() const = 0;

	// Atmosphere banks, as the game's audio library keeps them. Bank 0 is no bank and is ignored.
	virtual uint32_t AtmosRegisterBank(const std::string& bankName, const std::vector<pack::AudioBankSampleHeader>& headers,
	                                   uint16_t atmosCount) = 0;
	virtual void AtmosReleaseBank(uint32_t bank) = 0;
	virtual void AtmosSetBankVolume(uint32_t bank, int32_t volume) = 0;
	virtual void AtmosSetGroup(uint32_t bank, uint32_t group) = 0;
	virtual void AtmosProcess(bool active) = 0;
	/// Null when there is no audio device
	[[nodiscard]] virtual const AtmosPlayer* GetAtmos() const = 0;

	/// Where the placed sounds are heard from, which decides where they sound and whether they start: the camera, or
	/// the place the listener is put while it is put elsewhere
	[[nodiscard]] virtual Listener GetListener() const = 0;
	/// Puts the listener somewhere other than the camera (the inspector's view shown over the camera), or gives it back
	/// to the camera with none
	virtual void SetListenerOverride(std::optional<Listener> listener) = 0;

	/// The player of the custom music folder, null when there is no audio device
	[[nodiscard]] virtual CustomMusicPlayerInterface* GetCustomMusic() { return nullptr; }
};

/// The listener at the camera, as the camera is now; a listener at the world's origin when there is no camera
[[nodiscard]] Listener CameraListener();
} // namespace audio
} // namespace openblack
