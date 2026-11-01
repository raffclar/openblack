/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>
#include <entt/fwd.hpp>
#include <glm/vec3.hpp>

#include "Sound/AnimEffectTable.h"
#include "Sound/EmitterFollow.h"
#include "Sound/HearingRange.h"
#include "Sound/ListenerFrame.h"
#include "Sound/MusicRequest.h"
#include "Sound/ScriptSoundEffect.h"
#include "Sound/SoundGroup.h"
#include "Sound/SoundId.h"
#include "Sound/VoiceOwner.h"

namespace openblack::pack
{
enum class AudioBankLoop : uint16_t;
}

namespace openblack::ecs
{
class Registry;
}

namespace openblack::audio
{

class CustomMusicPlayerInterface;

/// What became of an animation effect asked for
struct AnimEffectPlay
{
	enum class Outcome : uint8_t
	{
		Played,
		/// The bank is not loaded or has no effect for the keys
		NoEffect,
		/// The sample the effect picked is not loaded
		NotLoaded,
		/// The listener is beyond the sample's maximum distance
		TooFar,
		/// The bank header plays the sample once and the owner is playing it or another of its voice group
		AlreadyPlaying,
		/// The sample the effect picked may not be heard in the conditions it was asked under
		NotHeard,
		/// All the voices play samples of the same or a higher priority
		NoVoice,
	};
	Outcome outcome {Outcome::NoEffect};
	/// The emitter playing it, null when nothing plays
	entt::entity emitter {entt::null};
	/// The sample the effect picked, none when the bank has no effect for the keys
	std::optional<int32_t> sample;
};

/// How a sound effect is played, beyond what its bank header says
struct SoundEffectOptions
{
	/// A 3D sound anchored there, otherwise centred on the listener
	std::optional<glm::vec3> position;
	/// Its playback rate in percent, before the bank header's random deviation. The bank header's rate wins, if it has
	/// one.
	std::optional<uint32_t> pitchPercent;
	/// Its volume, 0 to 127. The bank header's volume wins, if it has one.
	std::optional<uint32_t> volume;
	/// The pan of a sound that is not in the world, from the left (0) to the right (180), 90 the centre. The bank header's
	/// pan wins, if it has one.
	std::optional<uint32_t> pan;
	/// Repeat loops it until it is stopped or released; Once plays it the bank header's number of times
	PlayType playType {PlayType::Once};
	/// What it does about the voices that play already, unless its bank header gives another play type. Empty uses
	/// restart, the play type of a sound effect when its caller gives no other.
	std::optional<pack::AudioBankLoop> voicePlayType;
	/// What it is played for: a loop tied to its owner is moved with SetEmitterPosition and stopped with
	/// StopOwnedSounds
	entt::entity owner {entt::null};
	/// Whose sound it is to the voice rules
	VoiceOwner voiceOwner {VoiceOwner::Entity};
	/// The owner number when the voice owner is a number
	uint32_t ownerNumber {0};
	/// The distances it is heard at fully and no further. The bank header's distances win, if it has them, unless
	/// callerWins has them.
	std::optional<float> minDistance;
	std::optional<float> maxDistance;
	/// The settings (bits of pack::AudioBankOverride) for which the values above win over the bank header
	uint32_t callerWins {0};
	/// What a 3D sound does in each audio turn: it follows its owner or the listener, or it stays where it started
	Follows follows {Follows::Nothing};
};

/// The sounds that the world asks for. The world uses this interface and does not know the audio library. The audio
/// library plays each sound at the time of the request, so a request does not change what plays, when or where. A
/// computer with no audio device (the dedicated server) has an implementation that plays nothing.
class SoundRequestInterface
{
public:
	virtual ~SoundRequestInterface() = default;

	/// The computer has a working sound device
	[[nodiscard]] virtual bool IsInstalled() const = 0;
	/// The computer lost the sound device after the game opened it
	[[nodiscard]] virtual bool IsDeviceLost() const = 0;

	/// Whether a loaded sound, played as one of the game's sound effects, is heard in these conditions
	[[nodiscard]] virtual bool IsGameEffectHeard(const SoundEffectConditions& conditions, entt::id_type sound) const = 0;
	/// Plays one of the game's sound effects once, if the game's conditions let the player hear it
	virtual void PlayGameEffect(entt::id_type sound, std::optional<glm::vec3> worldPosition) = 0;
	/// Starts one of the game's sound effects, if the game's conditions let the player hear it. It gives the emitter,
	/// or null when nothing plays.
	virtual entt::entity StartGameEffect(entt::id_type sound, const SoundEffectOptions& options) = 0;

	/// A sound effect played with options and no condition of the game: its emitter, or null when nothing plays
	virtual entt::entity StartSoundEffect(entt::id_type id, const SoundEffectOptions& options) = 0;
	/// Stops the sound effects of a sound that a request started
	virtual void StopSoundEffect(entt::id_type id) = 0;
	/// Stops the sample that a caller started with an owner number. Only the first voice that keeps the sample for that
	/// owner stops, and only if it plays.
	virtual void StopNumberedSoundEffect(entt::id_type id, uint32_t ownerNumber) = 0;
	/// True when the first voice that keeps the sample for the owner number plays
	[[nodiscard]] virtual bool IsNumberedSoundEffectPlaying(entt::id_type id, uint32_t ownerNumber) const = 0;
	/// Stops every sound effect that plays (not the music)
	virtual void StopAllSoundEffects() = 0;

	/// An emitter for a loaded sound, ready to play (see AudioManagerInterface::CreateEmitter)
	virtual entt::entity CreateEmitter(entt::id_type id, std::optional<glm::vec3> worldPosition, PlayType playType,
	                                   entt::entity owner) = 0;
	/// An emitter for a sound that no entity owns (see AudioManagerInterface::CreateOwnSoundEmitter)
	virtual entt::entity CreateOwnSoundEmitter(entt::id_type id, std::optional<glm::vec3> worldPosition, PlayType playType) = 0;

	/// One of the samples of a bank's animation effects for the keys (see AudioManagerInterface::PlayAnimEffect)
	virtual AnimEffectPlay PlayAnimEffect(const std::string& bankName, std::span<const int32_t> keys, entt::entity owner,
	                                      const glm::vec3& position, const std::optional<SoundEffectConditions>& heardUnder,
	                                      AnimEffectMeasure measure) = 0;
	/// The same, at the straight distance
	AnimEffectPlay PlayAnimEffect(const std::string& bankName, std::span<const int32_t> keys, entt::entity owner,
	                              const glm::vec3& position, const std::optional<SoundEffectConditions>& heardUnder)
	{
		return PlayAnimEffect(bankName, keys, owner, position, heardUnder, AnimEffectMeasure::Straight);
	}
	/// The same, in all conditions
	AnimEffectPlay PlayAnimEffect(const std::string& bankName, std::span<const int32_t> keys, entt::entity owner,
	                              const glm::vec3& position, AnimEffectMeasure measure)
	{
		return PlayAnimEffect(bankName, keys, owner, position, std::nullopt, measure);
	}
	/// The same, in all conditions, at the straight distance
	AnimEffectPlay PlayAnimEffect(const std::string& bankName, std::span<const int32_t> keys, entt::entity owner,
	                              const glm::vec3& position)
	{
		return PlayAnimEffect(bankName, keys, owner, position, std::nullopt, AnimEffectMeasure::Straight);
	}
	/// Ends the samples of a bank's animation effects for the keys, if the owner plays them (see
	/// AudioManagerInterface::EndAnimEffect)
	virtual void EndAnimEffect(const std::string& bankName, std::span<const int32_t> keys, entt::entity owner,
	                           const glm::vec3& position, AnimEffectEnd end, AnimEffectMeasure measure) = 0;

	virtual bool EmitterExists(entt::entity emitter) = 0;
	virtual void PlayEmitter(entt::entity emitter) = 0;
	virtual void StopEmitter(entt::entity emitter) = 0;
	virtual void DestroyEmitter(entt::entity emitter) = 0;
	[[nodiscard]] virtual float GetProgress(entt::entity emitter) = 0;
	[[nodiscard]] virtual AudioStatus GetStatus(entt::entity emitter) = 0;
	/// Moves a 3D emitter, as a loop follows its owner
	virtual void SetEmitterPosition(entt::entity emitter, const glm::vec3& position) = 0;
	/// Sets what a 3D emitter does in each audio turn
	virtual void SetEmitterFollows(entt::entity emitter, Follows follows) = 0;
	/// A looping emitter stops its loop: the pass that plays finishes and the emitter stops
	virtual void ReleaseEmitterLoop(entt::entity emitter) = 0;
	/// Whether an emitter loops until it stops
	[[nodiscard]] virtual bool IsEmitterLooping(entt::entity emitter) = 0;
	/// The volume of an emitter, 0 to 127
	virtual void SetEmitterVolume(entt::entity emitter, uint32_t volume) = 0;
	[[nodiscard]] virtual uint32_t GetEmitterVolume(entt::entity emitter) = 0;
	/// The playback rate of an emitter in percent
	virtual void SetEmitterPitch(entt::entity emitter, uint32_t pitchPercent) = 0;
	/// Stops every sound that plays for an owner
	virtual void StopOwnedSounds(entt::entity owner) = 0;
	/// A saved game loaded. Each emitter that the save kept goes (see AudioManagerInterface::ForgetSavedEmitters).
	virtual void ForgetSavedEmitters() = 0;

	/// Each emitter that has an owner goes to the point that placeOf gives for the owner. An emitter stays where it is
	/// when placeOf gives no point.
	virtual void PlaceOwnedEmitters(const std::function<std::optional<glm::vec3>(entt::entity owner)>& placeOf) = 0;

	/// The place from which the player hears the placed sounds
	[[nodiscard]] virtual Listener GetListener() const = 0;
	/// The distance from the place from which the player hears to a point. The audio reads its own listener for it.
	/// None when the computer has no audio manager.
	[[nodiscard]] virtual std::optional<float> ListenerDistance(const glm::vec3& point) const = 0;

	/// A new owner of sounds among the entities of the audio, not of the world. It tells the voice rules which sounds
	/// are the same. Null when the audio has no entities.
	[[nodiscard]] virtual entt::entity CreateSoundOwner() = 0;
	/// The owner of sounds that CreateSoundOwner gave goes. Nothing happens when it is not there.
	virtual void DestroySoundOwner(entt::entity owner) = 0;

	/// Stops every sound: the music at once, with no fade, the atmosphere, and each sound and voice that plays
	virtual void StopAllSounds() = 0;
	/// One step of the atmosphere sounds. When active is false, the atmosphere is silent.
	virtual void ProcessAtmosphere(bool active) = 0;

	/// Plays the music bank at a path. The bank loads if it does not play already.
	virtual bool MusicPlay(const std::string& bankPath, const MusicRequest& request) = 0;
	/// Stops the music, with a fade if fadeOut is true
	virtual void MusicStop(bool fadeOut) = 0;
	/// Stops the music bank at a path (see AudioManagerInterface::MusicStopBank)
	virtual void MusicStopBank(const std::string& bankPath, bool fadeOut) = 0;
	/// The music group and the length of a bank, none if the bank is not there
	[[nodiscard]] virtual std::optional<MusicBankInfo> GetMusicBankInfo(const std::string& bankPath) = 0;
	/// The music channels that play now, in the order of the channels. Empty when there is no music player.
	[[nodiscard]] virtual std::vector<MusicChannelPlace> GetPlayingMusicChannels() const = 0;
	/// The player of the custom music folder, null when there is no audio device
	[[nodiscard]] virtual CustomMusicPlayerInterface* GetCustomMusic() = 0;

	/// The music banks, the sound groups and the animation effects that the world loads at its start
	virtual void AddMusicEntry(const std::string& name) = 0;
	virtual void CreateSoundGroup(const std::string& name) = 0;
	virtual void AddToSoundGroup(const std::string& name, entt::id_type id) = 0;
	virtual void AddAnimEffects(const std::string& bankName, AnimEffectTable table) = 0;
	/// The loaded sound groups by name. Empty when there is no audio.
	[[nodiscard]] virtual const std::map<std::string, SoundGroup>& GetSoundGroups() = 0;

	/// The volumes of the sound effects and of the music, as the audio keeps them
	[[nodiscard]] virtual float GetSfxVolume() = 0;
	[[nodiscard]] virtual float GetMusicVolume() = 0;

	// The audio as the debug tools see and change it

	/// Whether the computer has an audio manager. A manager that plays nothing counts.
	[[nodiscard]] virtual bool HasAudio() const = 0;
	/// The place from which the player hears the placed sounds, in place of the camera, or none to use the camera again
	virtual void SetListenerOverride(std::optional<Listener> listener) = 0;
	/// The volume of all the sounds, as the audio keeps it
	[[nodiscard]] virtual float GetGlobalVolume() = 0;
	/// Whether a music bank plays
	[[nodiscard]] virtual bool IsMusicActive() const = 0;
	/// The music channels that play now, with their banks, volumes and places. Empty when there is no music player.
	[[nodiscard]] virtual std::vector<MusicChannelState> GetMusicChannelStates() const = 0;
	/// Acts as if the audio device stops (true) or comes back (false)
	virtual void SimulateDeviceLoss(bool lost) = 0;
	/// The registry of the audio's emitters, null when the audio has none
	[[nodiscard]] virtual const ecs::Registry* GetEmitters() = 0;
};

} // namespace openblack::audio
