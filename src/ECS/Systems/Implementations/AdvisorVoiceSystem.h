/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

#include "ECS/Systems/AdvisorVoiceSystemInterface.h"
#include "Help/AdvisorVoices.h"
#include "Sound/SilentPlayback.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class AdvisorVoiceSystem final: public AdvisorVoiceSystemInterface
{
public:
	AdvisorVoiceSystem();

	void Stop(int advisor) override { _voices.Stop(advisor); }
	void Say(int advisor, uint32_t line, float hoverX) override { _voices.Say(advisor, line, false, hoverX); }
	void Interrupt(int advisor, help::AdvisorVoices::InterruptedBy by) override { _voices.Interrupt(advisor, by); }
	void Update() override { _voices.Update(); }
	[[nodiscard]] help::AdvisorVoices& GetVoices() override { return _voices; }
	[[nodiscard]] const help::AdvisorVoices& GetVoices() const override { return _voices; }

private:
	/// The voices' sounds and clock, from the game's audio
	[[nodiscard]] help::AdvisorVoices::Audio VoiceAudio();
	/// The advisors' line plays: as the sound device tells, or by the length of its sample once the device is lost
	[[nodiscard]] bool AdvisorLinePlays() const;
	/// The voices' gestures, given to the advisors' bodies
	[[nodiscard]] static help::AdvisorVoices::Hooks VoiceHooks();

	help::AdvisorVoices _voices;
	/// The sound that says the advisors' line
	entt::entity _advisorLine {entt::null};
	/// When that sound started and its length. When the machine loses the sound device, the line lasts this long with no
	/// sound.
	audio::SilentPlayback _advisorPlayback;
};

} // namespace openblack::ecs::systems
