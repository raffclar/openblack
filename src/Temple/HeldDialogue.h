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

#include "Help/DialogueText.h"

namespace openblack::temple_help
{

/// The world's dialogue that the temple keeps while the player is inside
struct HeldDialogue
{
	uint32_t owner {0};
	help::DialogueText::Held text;
};

/// The kept dialogue that its owner gets back when the player leaves the temple or a saved game loads. The owner gets
/// it back only if nobody has the dialogue: no task has it and no task has the cinema bars in. Else the kept dialogue
/// goes.
[[nodiscard]] inline std::optional<HeldDialogue> ComesBack(const std::optional<HeldDialogue>& held, bool controlled)
{
	return controlled ? std::nullopt : held;
}

} // namespace openblack::temple_help
