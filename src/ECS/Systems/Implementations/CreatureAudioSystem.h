/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/CreatureAudioSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class CreatureAudioSystem final: public CreatureAudioSystemInterface
{
public:
	void Update(std::chrono::duration<float, std::milli> gameTime) override;
	[[nodiscard]] bool IsMuted() const override { return _muted; }
	void SetMuted(bool muted) override { _muted = muted; }
	/// The script's creature sound switch (SET_CREATURE_SOUND), which the script's audio state keeps
	[[nodiscard]] bool AreOtherVoicesEnabled() const override;
	void SetOtherVoicesEnabled(bool enabled) override;
	void Play(entt::entity creature, const creature_audio::SoundEvent& event) override;

private:
	bool _muted {false};
};

} // namespace openblack::ecs::systems
