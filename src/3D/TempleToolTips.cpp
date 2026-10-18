/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleToolTips.h"

#include <algorithm>
#include <array>
#include <vector>

using namespace openblack;
using gui::ToolTipAction;
namespace Arrows = gui::ToolTipArrows;

namespace
{
/// The temple's tooltips, by their text less 0xE73
/// The first tooltip's help text
constexpr uint32_t k_FirstText = 0xE73;
constexpr uint32_t k_ZoomIn = 0xE75 - 0xE73;
constexpr uint32_t k_Move = 0xE7E - 0xE73;
constexpr uint32_t k_ZoomOut = 0xE8B - 0xE73;
constexpr uint32_t k_WorldRoom = 0xE9B - 0xE73;
constexpr uint32_t k_ExitTemple = 0xE98 - 0xE73;
constexpr uint32_t k_TattooCreature = 0xEDF - 0xE73;
constexpr uint32_t k_Scroll = 0xEE2 - 0xE73;
constexpr uint32_t k_WorldStats = 0xEE3 - 0xE73;
constexpr uint32_t k_SaveGameStats = 0xEE4 - 0xE73;
constexpr uint32_t k_ChallengeStats = 0xEE5 - 0xE73;
constexpr uint32_t k_ReplayChallenge = 0xE9F - 0xE73;
constexpr uint32_t k_TiltRotate = 0xEED - 0xE73;
/// The main room's doors, in the game's order: the creature, options, exit, future, library, save game and challenge
/// rooms. The scroll's wall, door 6, leaves the tooltip be.
constexpr std::array<std::optional<uint32_t>, 8> k_Doors = {
    0xE95 - 0xE73, 0xE99 - 0xE73, 0xE98 - 0xE73, 0xE9A - 0xE73, 0xE97 - 0xE73, 0xE96 - 0xE73, std::nullopt, 0xE94 - 0xE73,
};

bool IsZoomed(const TempleToolTipInput& input)
{
	return input.zoom == 1.0f;
}

/// The main, challenge and save game rooms' scrolls. Close up, a scroll shows it can be dragged; from afar what it
/// tells of; and looking at it, the way back.
void UpdateRoomScroll(TempleToolTip& toolTip, const TempleToolTipInput& input, const TempleScrolls::Control& scroll,
                      uint32_t stats)
{
	if (input.hoveredSubMesh == scroll.subMesh)
	{
		if (IsZoomed(input))
		{
			// Close up to one of the challenge room's pictures, its scroll says nothing
			if (!input.pictureLookedAt)
			{
				toolTip = {.index = k_Scroll, .action = ToolTipAction::Select, .arrows = Arrows::k_UpDown};
			}
		}
		// The world's scroll tells of the world whenever it isn't close, the others only once the camera is back
		else if (input.room == TempleRoom::Main || input.zoom == 0.0f)
		{
			toolTip = {.index = stats, .action = ToolTipAction::Select, .arrows = Arrows::k_None};
		}
	}
	else if (input.lookingAtScroll && IsZoomed(input) && !input.pictureLookedAt)
	{
		toolTip = {.index = k_ZoomOut, .action = ToolTipAction::Select, .arrows = Arrows::k_None};
	}
}

/// The challenge room's picture or replay button. A button hovered says it replays its challenge, and keeps the
/// pictures after it in the mesh's order from choosing. A picture hovered from afar zooms in; the one looked at close up
/// replays its challenge if it isn't done (and says nothing if it is), and while it isn't hovered, zooms back out.
void UpdateChallengeControl(TempleToolTip& toolTip, const TempleToolTipInput& input, const TempleChallengeControl& control,
                            bool& buttonHovered)
{
	const bool hovered = input.hoveredSubMesh == control.subMesh;
	if (control.button)
	{
		if (!control.done && hovered)
		{
			toolTip = {.index = k_ReplayChallenge, .action = ToolTipAction::Select, .arrows = Arrows::k_None};
			buttonHovered = true;
		}
		return;
	}
	const bool closeUp = IsZoomed(input) && input.challengeHighDetail == control.place && !buttonHovered;
	if (hovered && input.zoom == 0.0f)
	{
		toolTip = {.index = k_ZoomIn, .action = ToolTipAction::Select, .arrows = Arrows::k_None};
	}
	else if (hovered && closeUp)
	{
		toolTip = control.done
		              ? TempleToolTip {.index = std::nullopt, .action = ToolTipAction::None, .arrows = Arrows::k_None}
		              : TempleToolTip {.index = k_ReplayChallenge, .action = ToolTipAction::Select, .arrows = Arrows::k_None};
	}
	else if (!hovered && closeUp)
	{
		toolTip = {.index = k_ZoomOut, .action = ToolTipAction::Select, .arrows = Arrows::k_None};
	}
}
/// The save game room: moving about and the way back, then its controls' callbacks in the order of their submeshes: the
/// pictures, the buttons and the scroll. A button hovered keeps the pictures after it from choosing. A picture that
/// zooms in keeps the scroll from zooming out.
void UpdateSaveGameRoom(TempleToolTip& toolTip, const TempleToolTipInput& input, bool overWayBack,
                        std::u16string* savedGameName)
{
	namespace rules = temple::save_game_room;
	toolTip = {.index = k_Move, .action = ToolTipAction::Select, .arrows = Arrows::k_All};
	if (overWayBack)
	{
		toolTip = {.index = k_WorldRoom, .action = ToolTipAction::Select, .arrows = Arrows::k_None};
	}
	std::vector<uint32_t> controls;
	for (const auto& control : input.savedGames)
	{
		controls.push_back(control.subMesh);
	}
	for (const auto& scroll : input.scrolls)
	{
		controls.push_back(scroll.subMesh);
	}
	std::ranges::sort(controls);
	bool buttonHovered = false;
	bool pictureZoomsIn = false;
	for (const auto subMesh : controls)
	{
		const bool hovered = input.hoveredSubMesh == subMesh;
		const auto control = std::ranges::find(input.savedGames, subMesh, &TempleSaveGameControl::subMesh);
		if (control == input.savedGames.end())
		{
			// The scroll: close up it can be dragged, from afar it tells of the saves; looked at, the way back, unless a
			// picture zooms in
			if (hovered)
			{
				if (IsZoomed(input) && !input.pictureLookedAt)
				{
					toolTip = {.index = k_Scroll, .action = ToolTipAction::Select, .arrows = Arrows::k_UpDown};
				}
				else if (input.zoom == 0.0f)
				{
					toolTip = {.index = k_SaveGameStats, .action = ToolTipAction::Select, .arrows = Arrows::k_None};
				}
			}
			else if (input.lookingAtScroll && IsZoomed(input) && !input.pictureLookedAt && !pictureZoomsIn)
			{
				toolTip = {.index = k_ZoomOut, .action = ToolTipAction::Select, .arrows = Arrows::k_None};
			}
			continue;
		}
		switch (control->control.part)
		{
		case rules::Part::Picture:
		{
			const rules::PictureToolTipInput picture {
			    .slot = control->control.slot,
			    .hovered = hovered,
			    .zoom = input.zoom,
			    .pictureLookedAt = input.pictureLookedAt,
			    .highDetail = input.savedGameHighDetail,
			    .buttonHovered = buttonHovered,
			    .saveName =
			        control->saveName.has_value() ? std::optional<std::u16string_view>(*control->saveName) : std::nullopt,
			    .multiplayer = input.multiplayer,
			};
			pictureZoomsIn = pictureZoomsIn || rules::PictureZoomsIn(picture);
			if (const auto chosen = rules::PictureToolTipOf(picture); chosen.has_value())
			{
				toolTip = {.index = chosen->text - k_FirstText, .action = ToolTipAction::Select, .arrows = Arrows::k_None};
				if (chosen->text == rules::k_ZoomInNamedText && savedGameName != nullptr)
				{
					*savedGameName = chosen->name;
				}
			}
			break;
		}
		case rules::Part::Save:
		case rules::Part::Delete:
			if (hovered)
			{
				const auto text = control->control.part == rules::Part::Save ? rules::k_SaveText : rules::k_DeleteText;
				toolTip = {.index = text - k_FirstText, .action = ToolTipAction::Select, .arrows = Arrows::k_None};
				buttonHovered = true;
			}
			break;
		case rules::Part::Load:
			// The load button leaves the arrows of the tooltip before it
			if (hovered)
			{
				toolTip.index = rules::k_LoadText - k_FirstText;
				toolTip.action = ToolTipAction::Select;
				buttonHovered = true;
			}
			break;
		}
	}
}
} // namespace

void openblack::UpdateTempleToolTip(TempleToolTip& toolTip, const TempleToolTipInput& input, std::u16string* savedGameName)
{
	const bool overWayBack = input.overWayBack && !input.controlHeld;
	switch (input.room)
	{
	case TempleRoom::Main:
		// In the main room: moving about, the pool zooms in and once close tilts and turns the map, and the
		// doors lead to their rooms
		toolTip = {.index = k_Move, .action = ToolTipAction::Select, .arrows = Arrows::k_All};
		if (input.overPool)
		{
			if (input.pressingPool && IsZoomed(input))
			{
				toolTip.index = k_TiltRotate;
			}
			else if (!IsZoomed(input))
			{
				toolTip = {.index = k_ZoomIn, .action = ToolTipAction::Select, .arrows = Arrows::k_None};
			}
		}
		if (input.hoveredDoor.has_value() && !input.controlHeld && *input.hoveredDoor < k_Doors.size())
		{
			if (const auto door = k_Doors.at(*input.hoveredDoor); door.has_value())
			{
				toolTip = {.index = door, .action = ToolTipAction::Select, .arrows = Arrows::k_None};
			}
		}
		// The controls' draw callbacks, in the order of their submeshes (as the game sorts them on loading): the
		// buttons of what the map shows say what they show when hovered, and the scroll as the others' rooms do
		{
			std::vector<uint32_t> controls;
			for (const auto& toggle : input.toggles)
			{
				controls.push_back(toggle.subMesh);
			}
			for (const auto& scroll : input.scrolls)
			{
				controls.push_back(scroll.subMesh);
			}
			std::ranges::sort(controls);
			for (const auto subMesh : controls)
			{
				if (const auto toggle = std::ranges::find(input.toggles, subMesh, &TempleToggles::Control::subMesh);
				    toggle != input.toggles.end())
				{
					if (input.hoveredSubMesh == subMesh)
					{
						toolTip = {.index = toggle->toolTip, .action = ToolTipAction::Select, .arrows = Arrows::k_None};
					}
				}
				else
				{
					UpdateRoomScroll(toolTip, input,
					                 *std::ranges::find(input.scrolls, subMesh, &TempleScrolls::Control::subMesh),
					                 k_WorldStats);
				}
			}
		}
		break;
	case TempleRoom::Challenge:
	{
		// The challenge room: moving about and the way back, then its controls' draw callbacks in the order of their
		// submeshes: the pictures, the replay buttons and the scroll
		toolTip = {.index = k_Move, .action = ToolTipAction::Select, .arrows = Arrows::k_All};
		if (overWayBack)
		{
			toolTip = {.index = k_WorldRoom, .action = ToolTipAction::Select, .arrows = Arrows::k_None};
		}
		std::vector<uint32_t> controls;
		for (const auto& control : input.challenges)
		{
			controls.push_back(control.subMesh);
		}
		for (const auto& scroll : input.scrolls)
		{
			controls.push_back(scroll.subMesh);
		}
		std::ranges::sort(controls);
		bool buttonHovered = false;
		for (const auto subMesh : controls)
		{
			if (const auto control = std::ranges::find(input.challenges, subMesh, &TempleChallengeControl::subMesh);
			    control != input.challenges.end())
			{
				UpdateChallengeControl(toolTip, input, *control, buttonHovered);
			}
			else
			{
				UpdateRoomScroll(toolTip, input, *std::ranges::find(input.scrolls, subMesh, &TempleScrolls::Control::subMesh),
				                 k_ChallengeStats);
			}
		}
		break;
	}
	case TempleRoom::SaveGame:
		UpdateSaveGameRoom(toolTip, input, overWayBack, savedGameName);
		break;
	case TempleRoom::Credits:
		// The credits room: moving about, and the way back
		toolTip = {.index = k_Move, .action = ToolTipAction::Select, .arrows = Arrows::k_All};
		if (overWayBack)
		{
			toolTip = {.index = k_WorldRoom, .action = ToolTipAction::Select, .arrows = Arrows::k_None};
		}
		break;
	case TempleRoom::CreatureCave:
	{
		// The creature's room leaves the arrows as they were, so after a scroll's they stay up and down
		toolTip.index = k_Move;
		toolTip.action = ToolTipAction::Select;
		if (overWayBack)
		{
			toolTip.index = k_WorldRoom;
		}
		// Zoomed, or zooming, to a target, the camera zooms back; the targets say what clicking them does
		if (input.zoomingToCaveTarget)
		{
			toolTip.index = k_ZoomOut;
		}
		if (input.caveTarget.has_value())
		{
			switch (*input.caveTarget)
			{
			case CreatureCaveTargets::Target::Creature:
				toolTip.index = k_TattooCreature;
				break;
			case CreatureCaveTargets::Target::Exit:
				toolTip.index = k_ExitTemple;
				break;
			default:
				toolTip.index = input.zoomingToCaveTarget ? k_ZoomOut : k_ZoomIn;
				break;
			}
		}
		// The scrolls' callbacks, in turn: hovered, a scroll sets its title, which isn't a tooltip, so none shows unless
		// it is close and the one looked at; looked at, another lets the way back show unless one before was hovered
		bool scrollHovered = false;
		for (const auto& scroll : input.scrolls)
		{
			const bool hovered = input.hoveredSubMesh == scroll.subMesh;
			if (hovered)
			{
				toolTip.index = std::nullopt;
				toolTip.action = ToolTipAction::Select;
				scrollHovered = true;
				if (IsZoomed(input) && scroll.focused)
				{
					toolTip = {.index = k_Scroll, .action = ToolTipAction::Select, .arrows = Arrows::k_UpDown};
				}
			}
			else if (IsZoomed(input) && !scrollHovered)
			{
				toolTip.index = k_ZoomOut;
				toolTip.action = ToolTipAction::Select;
			}
		}
		break;
	}
	default:
		// The options and future rooms set no tooltip
		break;
	}

	// In every room: none on the way in, or on the way through a door
	if (!input.inControl)
	{
		toolTip = {.index = std::nullopt, .action = ToolTipAction::None, .arrows = Arrows::k_None};
	}
}
