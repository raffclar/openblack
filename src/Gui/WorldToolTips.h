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
#include <vector>

#include <entt/entity/entity.hpp>

#include "Gui/ToolTips.h"
#include "Gui/WorldToolTipRules.h"

namespace openblack::gui::world_tooltip
{

/// A tooltip the hand submits for what it is over
struct Submission
{
	Numbered numbers;
	ToolTipAction action {ToolTipAction::None};
	/// It shows at once, over any other
	bool force {false};
	/// It has no number to write into its words: a tooltip of words only
	bool wordsOnly {false};
};

/// What the hand shows over a thing of the world, in the order the game submits them: a temple's health, a village
/// centre's belief and population, the way into the player's own temple, a village store's food and wood, a pile's
/// amount. Nothing for anything else.
[[nodiscard]] std::vector<Submission> Over(entt::entity object);

} // namespace openblack::gui::world_tooltip
