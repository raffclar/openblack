/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ToolTips.h"

using namespace openblack::gui;

void ToolTips::Submit(uint32_t index, ToolTipAction action, uint32_t arrows, bool force)
{
	// An index outside the tooltips submits nothing, as the help system takes only its own texts
	if (index >= k_Count)
	{
		return;
	}
	help::tooltips::Submit(TextOf(index), BindingOf(action), arrows, force);
}

void ToolTips::ProcessTurn()
{
	help::tooltips::ProcessTurn();
}

void ToolTips::Clear()
{
	help::tooltips::Reset();
}
