/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include "Audio/AudioManagerInterface.h"
#include "Audio/GameQueries.h"

namespace openblack::audio
{

/// Plays nothing and answers as an empty engine does (no channel, no bank, 0, false): the base of a test's fake.
/// Not a stand-in for a missing device: our engine runs without one, and what it answers reaches the game
class AudioManagerNoOp: public AudioManagerInterface
{
public:
	[[nodiscard]] BankId CreatureBank(std::string_view) override { return k_NoBank; }
	[[nodiscard]] float MaxDistance(Sample) override { return 0.0f; }
	[[nodiscard]] std::optional<Sample> FindSample(BankId, std::string_view) override { return std::nullopt; }
	void RegisterObject(uint32_t, ObjectPositionFn) override {}
	void UnregisterObject(uint32_t) override {}
	[[nodiscard]] uint32_t NewObjectId() override { return 0; }
	Channel PlaySoundEffect(const PlayOptions&) override { return k_NoChannel; }
	Channel PlaySoundEffect(Owner, int, int, int, bool, bool, SfxBank) override { return k_NoChannel; }
	Channel PlaySoundEffect(Owner, int, int, int, bool, bool, BankId) override { return k_NoChannel; }
	Channel PlaySoundEffectAt(Owner, glm::vec3, int, int, int, bool, bool, SfxBank) override { return k_NoChannel; }
	Channel PlaySoundEffectAt(Owner, glm::vec3, int, int, int, bool, bool, BankId) override { return k_NoChannel; }
	Channel PlaySoundEffectAt(Owner, glm::vec3, glm::vec3, int, bool, int, int, bool, bool, BankId) override
	{
		return k_NoChannel;
	}
	void StopSoundEffect(int, Owner, SfxBank) override {}
	void StopSoundEffect(int, Owner, BankId) override {}
	void StopAllSoundEffects() override {}
	void ReleaseLoop(Owner, int, SfxBank) override {}
	void ReleaseLoop(Owner, int, BankId) override {}
	[[nodiscard]] bool IsPlaying(Owner, int, SfxBank) override { return false; }
	[[nodiscard]] bool IsPlaying(Owner, int, BankId) override { return false; }
	[[nodiscard]] bool IsPlaying(Owner, SfxBank) override { return false; }
	void SetPitch(BankId, Owner, int, int) override {}
	void SetVolume(Channel, int) override {}
	[[nodiscard]] bool IsPlaying(Channel) override { return false; }
	[[nodiscard]] Channel PlayingChannel(Owner, BankId) override { return k_NoChannel; }
	[[nodiscard]] int Volume(Channel) override { return 0; }
	[[nodiscard]] int NextCounter(Counter) override { return 0; }
	[[nodiscard]] uint32_t TickCount() override { return 0; }
	[[nodiscard]] bool SfxTrace() override { return false; }
	void MusicStop(int) override {}
	void LeaveCitadel() override {}
	void Init(GameQueries) override {}
	void Shutdown() override {}
	void ProcessTurn() override {}
	void ProcessCitadelTurn() override {}
	void Paused() override {}
	void UpdateFrame() override {}
	void OnThingDeleted(entt::entity) override {}
	void ClearMap() override {}
	void OnFocus(bool) override {}
	void SetSampleMainVolume(int) override {}
	[[nodiscard]] int SampleMainVolume() override { return 0; }
	Channel PlayAnimationEffect(Owner, float, const AnimKey&, AnimAction, BankId, bool, float, float) override
	{
		return k_NoChannel;
	}
	[[nodiscard]] bool SoundExists() override { return false; }
};

} // namespace openblack::audio
