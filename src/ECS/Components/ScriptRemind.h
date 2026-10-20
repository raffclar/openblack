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

#include <optional>

#include <glm/vec2.hpp>

#include "ECS/Components/ScriptAnimation.h"
#include "ECS/Components/ScriptStateArguments.h"
#include "Enums.h"

namespace openblack::ecs::components
{

/// What a villager keeps of its script's work when something takes it out of the script's hands, so that it can go
/// back to that work later. When it next goes into the kept state, it walks back to the kept place first.
struct ScriptRemind
{
	/// The clip a remind keeps when no script forced a clip
	static constexpr int32_t k_NoClip = -2;

	/// The script state it goes back to; none when it has nothing to go back to
	VillagerStates returnTo {VillagerStates::InvalidState};
	/// Where its walk was going when it left the script's hands, or where it stood, on the ground in metres
	glm::vec2 goal {0.0f};
	/// What the script last gave it for its script states, and the clip it was to play
	ScriptStateArguments arguments;
	std::optional<ScriptAnimation> animation;
	/// The clip a script forced in place of its state's own, to play again when it comes back to a clip state
	int32_t clip {k_NoClip};
};

} // namespace openblack::ecs::components
