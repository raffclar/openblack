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
#include <span>
#include <variant>

#include <entt/entity/entity.hpp>

#include "Commands/PlayerCommands.h"
#include "ECS/Components/ReportedHand.h"

/// The reports of this computer continue after a load of a world. The loaded world keeps the last hand report, the last
/// pick and the last view report of each player (components::ReportedHand). The reports of this computer start from
/// these values, so that the first report after the load goes only when the hand or the view changed since the save.
namespace openblack::hand_report
{

/// What the reports of this computer last sent, as the loaded world keeps it
struct LoadedReports
{
	player_commands::HandReport hand;
	/// The thing under the cursor of the last pick, null for none
	entt::entity pick {entt::null};
	/// The last view report, none before the first view report
	std::optional<player_commands::ViewReport> view;
	/// The turns that the hand report waits after the load
	uint8_t wait {0};
};

/// The last reports of a player, from the values that the loaded world keeps of its hand
[[nodiscard]] inline LoadedReports FromLoadedWorld(const ecs::components::ReportedHand& reported)
{
	LoadedReports loaded {
	    .hand =
	        {
	            .collidePoint = reported.collidePoint,
	            .handPoint = reported.handPoint,
	            .camera = reported.camera,
	            .focus = reported.focus,
	        },
	    .pick = reported.pickedObject,
	    .view = std::nullopt,
	    .wait = 0,
	};
	if (reported.viewReported)
	{
		loaded.view = player_commands::ViewReport {
		    .eye = reported.viewEye,
		    .focus = reported.viewFocus,
		    .lens = reported.viewLens,
		    .nearClip = reported.viewNearClip,
		    .screenWidth = reported.viewScreenWidth,
		    .screenHeight = reported.viewScreenHeight,
		    .aspect = reported.viewAspect,
		    .inclusionDistance = reported.inclusionDistance,
		    .cameraOrders = reported.cameraOrders,
		    .cameraArrived = reported.cameraArrived,
		    .gripsLand = reported.gripsLand,
		    .otherButton = reported.otherButton,
		    .mouseWheel = reported.mouseWheel,
		    .cameraTurned = reported.cameraTurned,
		    .cameraIcons = reported.cameraIcons,
		    .keysHeld = reported.keysHeld,
		    .mouseButtons = reported.mouseButtons,
		    .insideTemple = reported.insideTemple,
		};
	}
	return loaded;
}

/// The reports that the player sent before the load and that wait for the first turn after the load. The load keeps
/// only the open block of the game before the load. These reports are the last reports of the player, in place of the
/// values of the loaded world. A hand report among them also starts the wait after a send.
[[nodiscard]] inline LoadedReports WithWaiting(LoadedReports loaded, std::span<const player_commands::Command> sent,
                                               PlayerNames player, uint8_t waitAfterSend)
{
	for (const auto& command : sent)
	{
		if (command.player != player)
		{
			continue;
		}
		if (const auto* hand = std::get_if<player_commands::HandReport>(&command.action); hand != nullptr)
		{
			loaded.hand = *hand;
			loaded.wait = waitAfterSend;
		}
		else if (const auto* pick = std::get_if<player_commands::HandPick>(&command.action); pick != nullptr)
		{
			loaded.pick = pick->object;
		}
		else if (const auto* view = std::get_if<player_commands::ViewReport>(&command.action); view != nullptr)
		{
			loaded.view = *view;
		}
	}
	return loaded;
}

} // namespace openblack::hand_report
