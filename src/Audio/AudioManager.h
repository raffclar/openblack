/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

#include "Audio/AudioManagerInterface.h"

namespace openblack::audio
{

/// Our audio engine behind the slot: the members are defined where the engine's game side is (Game/AudioSystem.cpp,
/// Game/GameSfx.cpp)
class AudioManager final: public AudioManagerInterface
{
public:
	[[nodiscard]] BankId CreatureBank(std::string_view species) override;
	[[nodiscard]] float MaxDistance(Sample sample) override;
	[[nodiscard]] std::optional<Sample> FindSample(BankId bank, std::string_view wavName) override;
	void RegisterObject(uint32_t id, ObjectPositionFn position) override;
	void UnregisterObject(uint32_t id) override;
	[[nodiscard]] uint32_t NewObjectId() override;
	Channel PlaySoundEffect(const PlayOptions& options) override;
	Channel PlaySoundEffect(Owner owner, int sample, int mode, int loops, bool extra3DFlag, bool is3D, SfxBank bank) override;
	Channel PlaySoundEffect(Owner owner, int sample, int mode, int loops, bool extra3DFlag, bool is3D, BankId bank) override;
	Channel PlaySoundEffectAt(Owner owner, glm::vec3 position, int sample, int mode, int loops, bool extra3DFlag, bool is3D,
	                          SfxBank bank) override;
	Channel PlaySoundEffectAt(Owner owner, glm::vec3 position, int sample, int mode, int loops, bool extra3DFlag, bool is3D,
	                          BankId bank) override;
	Channel PlaySoundEffectAt(Owner owner, glm::vec3 position, glm::vec3 offset, int sample, bool track, int mode, int loops,
	                          bool extra3DFlag, bool is3D, BankId bank) override;
	void StopSoundEffect(int sample, Owner owner, SfxBank bank) override;
	void StopSoundEffect(int sample, Owner owner, BankId bank) override;
	void StopAllSoundEffects() override;
	void ReleaseLoop(Owner owner, int sample, SfxBank bank) override;
	void ReleaseLoop(Owner owner, int sample, BankId bank) override;
	[[nodiscard]] bool IsPlaying(Owner owner, int sample, SfxBank bank) override;
	[[nodiscard]] bool IsPlaying(Owner owner, int sample, BankId bank) override;
	[[nodiscard]] bool IsPlaying(Owner owner, SfxBank bank) override;
	void SetPitch(BankId bank, Owner owner, int sample, int percent) override;
	void SetVolume(Channel channel, int volume) override;
	[[nodiscard]] bool IsPlaying(Channel channel) override;
	[[nodiscard]] Channel PlayingChannel(Owner owner, BankId bank) override;
	[[nodiscard]] int Volume(Channel channel) override;
	[[nodiscard]] int NextCounter(Counter counter) override;
	[[nodiscard]] uint32_t TickCount() override;
	[[nodiscard]] bool SfxTrace() override;
	void MusicStop(int fade) override;
	void LeaveCitadel() override;
	void Init(GameQueries queries) override;
	void Shutdown() override;
	void ProcessTurn() override;
	void ProcessCitadelTurn() override;
	void Paused() override;
	void UpdateFrame() override;
	void OnThingDeleted(entt::entity thing) override;
	void ClearMap() override;
	void OnFocus(bool active) override;
	void SetSampleMainVolume(int volume) override;
	[[nodiscard]] int SampleMainVolume() override;
	Channel PlayAnimationEffect(Owner owner, float distance, const AnimKey& key, AnimAction action, BankId bank, bool track,
	                            float minDistance, float maxDistance) override;
	[[nodiscard]] bool SoundExists() override;
};

} // namespace openblack::audio
