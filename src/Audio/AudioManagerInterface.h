/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Audio/Audio.h"

namespace openblack::audio
{

/// The audio service (Locator::audio): the functions of Audio.h, which the game calls through it. Our engine is the
/// implementation (AudioManager); a test injects a fake through the slot (AudioManagerNoOp is a base for one)
class AudioManagerInterface
{
public:
	virtual ~AudioManagerInterface() = default;

	[[nodiscard]] virtual BankId CreatureBank(std::string_view species) = 0;
	[[nodiscard]] virtual float MaxDistance(Sample sample) = 0;
	[[nodiscard]] virtual std::optional<Sample> FindSample(BankId bank, std::string_view wavName) = 0;
	virtual void RegisterObject(uint32_t id, ObjectPositionFn position) = 0;
	virtual void UnregisterObject(uint32_t id) = 0;
	[[nodiscard]] virtual uint32_t NewObjectId() = 0;
	virtual Channel PlaySoundEffect(const PlayOptions& options) = 0;
	virtual Channel PlaySoundEffect(Owner owner, int sample, int mode, int loops, bool extra3DFlag, bool is3D,
	                                SfxBank bank) = 0;
	virtual Channel PlaySoundEffect(Owner owner, int sample, int mode, int loops, bool extra3DFlag, bool is3D, BankId bank) = 0;
	virtual Channel PlaySoundEffectAt(Owner owner, glm::vec3 position, int sample, int mode, int loops, bool extra3DFlag,
	                                  bool is3D, SfxBank bank) = 0;
	virtual Channel PlaySoundEffectAt(Owner owner, glm::vec3 position, int sample, int mode, int loops, bool extra3DFlag,
	                                  bool is3D, BankId bank) = 0;
	virtual Channel PlaySoundEffectAt(Owner owner, glm::vec3 position, glm::vec3 offset, int sample, bool track, int mode,
	                                  int loops, bool extra3DFlag, bool is3D, BankId bank) = 0;
	virtual void StopSoundEffect(int sample, Owner owner, SfxBank bank) = 0;
	virtual void StopSoundEffect(int sample, Owner owner, BankId bank) = 0;
	virtual void StopAllSoundEffects() = 0;
	virtual void ReleaseLoop(Owner owner, int sample, SfxBank bank) = 0;
	virtual void ReleaseLoop(Owner owner, int sample, BankId bank) = 0;
	[[nodiscard]] virtual bool IsPlaying(Owner owner, int sample, SfxBank bank) = 0;
	[[nodiscard]] virtual bool IsPlaying(Owner owner, int sample, BankId bank) = 0;
	[[nodiscard]] virtual bool IsPlaying(Owner owner, SfxBank bank) = 0;
	virtual void SetPitch(BankId bank, Owner owner, int sample, int percent) = 0;
	virtual void SetVolume(Channel channel, int volume) = 0;
	[[nodiscard]] virtual bool IsPlaying(Channel channel) = 0;
	[[nodiscard]] virtual Channel PlayingChannel(Owner owner, BankId bank) = 0;
	[[nodiscard]] virtual int Volume(Channel channel) = 0;
	[[nodiscard]] virtual int NextCounter(Counter counter) = 0;
	[[nodiscard]] virtual uint32_t TickCount() = 0;
	[[nodiscard]] virtual bool SfxTrace() = 0;
	virtual void MusicStop(int fade) = 0;
	virtual void LeaveCitadel() = 0;
	virtual void Init(GameQueries queries) = 0;
	virtual void Shutdown() = 0;
	virtual void ProcessTurn() = 0;
	virtual void ProcessCitadelTurn() = 0;
	virtual void Paused() = 0;
	virtual void UpdateFrame() = 0;
	virtual void OnThingDeleted(entt::entity thing) = 0;
	virtual void ClearMap() = 0;
	virtual void OnFocus(bool active) = 0;
	virtual void SetSampleMainVolume(int volume) = 0;
	[[nodiscard]] virtual int SampleMainVolume() = 0;
	virtual Channel PlayAnimationEffect(Owner owner, float distance, const AnimKey& key, AnimAction action, BankId bank,
	                                    bool track, float minDistance, float maxDistance) = 0;
	[[nodiscard]] virtual bool SoundExists() = 0;
};

} // namespace openblack::audio
