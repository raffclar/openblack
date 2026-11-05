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

#include "ECS/Systems/OwnReportSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class PlayerCommandSystemInterface;

class OwnReportSystem final: public OwnReportSystemInterface
{
public:
	void ReportHand(uint32_t worldCameraIcons) final;
	void ReportHelp(const HelpInputs& inputs) final;

	[[nodiscard]] const OwnReportState& GetState() const final { return _state; }

	/// The rules that keep a report from going when it did not change. The reports above use them with the services of
	/// the game.

	/// The grip of the land in the view report, after this frame's drag of the land and the Move button
	bool StepLandGrip(bool dragging, bool moveHeld);
	/// The camera turn in the view report: a turn asked in this frame, or in an earlier frame of the same game turn
	bool StepCameraTurned(uint32_t turn, bool controlsTurned);
	/// A new hand, as on a new land, has no view report yet
	void ForgetViewReport();
	/// Sends the view report only when the view changed enough since the last report that went. Returns true when it went.
	bool SendViewReport(const player_commands::ViewReport& report, PlayerCommandSystemInterface& commands);
	/// The help report is due once a turn, and only when the help is there. Returns true and keeps the turn when it is due.
	bool TakeHelpTurn(uint32_t turn, bool helpThere);

private:
	void ReportView(uint32_t worldCameraIcons);

	OwnReportState _state;
};

} // namespace openblack::ecs::systems
