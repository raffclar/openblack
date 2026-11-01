/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec3.hpp>

#include "Commands/PlayerCommands.h"
#include "Hand/HandReportRules.h"

/// The rule that decides when the computer of a player sends the report of its view. The eye and the focus use the same
/// move test as the camera part of the hand report. Thus a camera that moves by very small values at each frame sends no
/// view report.
namespace openblack::view_report
{

/// Whether the camera turned in the game turn that the report counts: a turn asked in this frame, or in an earlier frame
/// of the same game turn
[[nodiscard]] constexpr bool CameraTurnedInTurn(bool turnedEarlier, bool sameTurn, bool turnedNow)
{
	return turnedNow || (sameTurn && turnedEarlier);
}

/// The view changed enough since the last report that went:
/// - the eye or the focus moved by more than the move limit on an axis;
/// - or a different field of the report changed.
[[nodiscard]] inline bool ChangedEnough(const player_commands::ViewReport& last, const player_commands::ViewReport& now)
{
	if (hand_report::PointMoved(last.eye, now.eye) || hand_report::PointMoved(last.focus, now.focus))
	{
		return true;
	}
	const auto withoutCamera = [](player_commands::ViewReport report) {
		report.eye = glm::vec3(0.0f);
		report.focus = glm::vec3(0.0f);
		return report;
	};
	return withoutCamera(last) != withoutCamera(now);
}

} // namespace openblack::view_report
