/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include "Help/AdvisorVoices.h"

namespace openblack::ecs::systems
{

/// The voices of the good and the evil advisor that this computer plays: the sound of each line, its gestures and the
/// lip sync of the advisors' mouths. The scripts' dialogue (HelpTextSystemInterface) gives the lines and times them on
/// the scripts' clock. The turns and the scripts do not read these voices, and the dedicated server does not make them.
class AdvisorVoiceSystemInterface
{
public:
	/// The good advisor and the evil advisor, as the voices number them
	static constexpr int k_GoodAdvisor = 0;
	static constexpr int k_EvilAdvisor = 1;

	virtual ~AdvisorVoiceSystemInterface() = default;

	/// The advisor stops its line and no longer counts as given one
	virtual void Stop(int advisor) = 0;
	/// The advisor says a line of the advisors' bank after the delay for where it hovers (-1 the left edge, 1 the right)
	virtual void Say(int advisor, uint32_t line, float hoverX) = 0;
	/// The advisor is cut short
	virtual void Interrupt(int advisor, help::AdvisorVoices::InterruptedBy by) = 0;
	/// Once a frame, before the scripts' dialogue: a line whose delay is over starts
	virtual void Update() = 0;

	/// The voices, for the advisors' bodies and the inspector
	[[nodiscard]] virtual help::AdvisorVoices& GetVoices() = 0;
	[[nodiscard]] virtual const help::AdvisorVoices& GetVoices() const = 0;
};

} // namespace openblack::ecs::systems
