/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TreeAlignment.h"

#include "ECS/Components/AlignmentHistory.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "Magic/AreaEffect.h"

using namespace openblack;
using namespace openblack::ecs;

void tree_alignment::TreeMoved(systems::AlignmentSystemInterface& alignment, PlayerNames player, float treePullPutChange,
                               bool planted)
{
	const float change = planted ? treePullPutChange : -treePullPutChange;
	const float damped = magic::DampAlignmentChange(change, alignment.GetPlayerAlignment(player));
	alignment.AddToHistory(player, components::AlignmentCause::Tree, damped);
	alignment.AddPendingAlignment(player, damped);
}
