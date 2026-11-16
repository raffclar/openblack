/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string>
#include <unordered_set>

#include "ECS/Systems/CreatureAudioSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::creature
{
struct CreatureRig;
} // namespace openblack::creature

namespace openblack::ecs::components
{
struct Creature;
} // namespace openblack::ecs::components

namespace openblack::ecs::systems
{

class CreatureAudioSystem final: public CreatureAudioSystemInterface
{
public:
	void SaveState(reflection::BinaryWriter& out) const override;
	bool LoadState(reflection::BinaryReader& in) override;
	void Update(std::chrono::duration<float, std::milli> gameTime) override;
	[[nodiscard]] bool IsMuted() const override { return _muted; }
	void SetMuted(bool muted) override { _muted = muted; }
	[[nodiscard]] bool AreOtherVoicesEnabled() const override { return _otherVoices; }
	void SetOtherVoicesEnabled(bool enabled) override { _otherVoices = enabled; }
	void Play(entt::entity creature, const creature_audio::SoundEvent& event) override;
	void PlayAction(entt::entity creature, const creature_audio::SoundEvent& event) override;
	void AskForVoiceBanks() override;

private:
	/// Asks the loading threads for the samples of the voice bank of a creature, once for each bank
	void AskForVoiceBank(const components::Creature& creature, const creature::CreatureRig& rig);

	/// The rules that decide whether the creature's sounds are asked for
	[[nodiscard]] creature_audio::Gate GateOf(entt::entity creature) const;

	bool _muted {false};
	bool _otherVoices {true};
	/// The voice banks that the system asked for. This is not a part of the game state.
	std::unordered_set<std::string> _askedBanks;
};

} // namespace openblack::ecs::systems
