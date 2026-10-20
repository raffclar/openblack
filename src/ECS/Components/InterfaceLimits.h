/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs::components
{

/// What the interface levels of the tutorial keep away from the Action button of a player. The levels that the lessons
/// use set these limits, and only the levels that open the interface again remove them.
struct InterfaceLimits
{
	/// With an empty hand, the button does something only on a highlight that a script made
	bool onlyHighlights {false};
	/// The button does not take hold of a creature or a town's totem
	bool noHolding {false};
};

} // namespace openblack::ecs::components
