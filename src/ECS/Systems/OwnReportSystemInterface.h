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

#include <functional>
#include <optional>

#include "Commands/PlayerCommands.h"
#include "Help/HelpProfile.h"

namespace openblack::ecs::systems
{

/// What the reports of this computer keep from turn to turn
struct OwnReportState
{
	/// The view report says that the hand grips the land: from a drag of the land until the Move button is let go
	bool landGrip {false};
	/// The world camera's controls asked for a turn in a frame of the game turn that the view report counts
	bool cameraTurned {false};
	/// The game turn of the last view report, which tells if the camera turn is in the same game turn
	std::optional<uint32_t> cameraTurnTurn;
	/// The last report of the view that this computer sent, none before the first and none after a new hand
	std::optional<player_commands::ViewReport> lastViewReport;
	/// The turn of the last report of the help that this computer sent
	std::optional<uint32_t> helpReportTurn;
};

/// The reports that this computer sends once a turn, before the block of commands goes: the hand, the view and the help.
/// Each report goes only when it changed. The turns of the world do not read this system, and the dedicated server does
/// not make it.
class OwnReportSystemInterface
{
public:
	/// What the help report reads from the game's screen. The report calls them only when it is due.
	struct HelpInputs
	{
		/// A box of the menu shows on the screen
		std::function<bool()> frontEndBoxShows;
		/// The view of the world camera, none from a different camera
		std::function<std::optional<help::profile::WorldCameraView>()> worldCameraView;
	};

	virtual ~OwnReportSystemInterface() = default;

	/// Reports where the hand and the camera are, when they changed, and with them the view that the scripts and the
	/// advisors' remarks read. The view report uses the camera icons of the world camera when the draw kept none.
	virtual void ReportHand(uint32_t worldCameraIcons) = 0;
	/// Reports the help events of the player's input and the view of the world camera, when they changed
	virtual void ReportHelp(const HelpInputs& inputs) = 0;

	[[nodiscard]] virtual const OwnReportState& GetState() const = 0;
};

} // namespace openblack::ecs::systems
