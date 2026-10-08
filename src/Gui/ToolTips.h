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

#include "Help/ToolTips.h"

namespace openblack::gui
{

/// The arrows a tooltip shows about its mouse, for the ways the mouse can be dragged
namespace ToolTipArrows
{
constexpr uint32_t k_None = 0;
constexpr uint32_t k_Up = 0x100;
constexpr uint32_t k_Down = 0x200;
constexpr uint32_t k_Left = 0x400;
constexpr uint32_t k_Right = 0x800;
constexpr uint32_t k_UpDown = k_Up | k_Down;
constexpr uint32_t k_All = k_UpDown | k_Left | k_Right;
} // namespace ToolTipArrows

/// The action a tooltip's mouse shows the button of, or none for a tooltip of just words
enum class ToolTipAction : uint8_t
{
	None,
	/// The select action, the left button
	Select,
	/// The apply action, the right button
	Apply,
};

/// The words, mouse and arrows the hand shows for what it is over: the help system's tooltip (Help/ToolTips.h), which
/// keeps one tooltip for the whole game, as a handle the interface hands out.
///
/// What the hand is over submits a tooltip every turn it is over it, and once a turn the help system lets the tooltip
/// live on or ends it.
class ToolTips
{
public:
	/// The tooltips, the help system's texts from HELP_TEXT_TOOLTIP_01 onwards
	static constexpr uint32_t k_Count = help::tooltips::k_Count;

	/// The help system's text of a tooltip
	[[nodiscard]] static constexpr uint32_t TextOf(uint32_t index) { return help::tooltips::k_First + index; }
	/// The key binding whose button the tooltip's mouse shows for an action: the left button's and the right
	/// button's, or none
	[[nodiscard]] static constexpr int32_t BindingOf(ToolTipAction action)
	{
		switch (action)
		{
		case ToolTipAction::Select:
			return 1;
		case ToolTipAction::Apply:
			return 2;
		case ToolTipAction::None:
			break;
		}
		return -1;
	}

	/// The tooltip of an index for this turn. A forced one shows at once, even over one still being shown.
	void Submit(uint32_t index, ToolTipAction action, uint32_t arrows, bool force = false);
	/// Ends the turn: keeps the tooltip submitted, or one lingering after it was, and ends any other
	void ProcessTurn();
	/// No tooltip is shown any more, as at a new land; the counts of how often each was shown stay
	void Clear();
};

} // namespace openblack::gui
