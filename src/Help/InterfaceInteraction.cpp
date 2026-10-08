/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "InterfaceInteraction.h"

#include <spdlog/spdlog.h>

#include "Camera/CameraHelp.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/ScriptStateInterface.h"
#include "Input/InterfaceActive.h"
#include "Locator.h"

namespace openblack::help::interface_interaction
{
namespace
{
/// What this module keeps between calls (Locator::scriptState)
struct InterfaceInteractionState
{
	int32_t level {0};
	bool cameraMovesAllowed {true};
	bool realmZoomsAllowed {true};
};

InterfaceInteractionState& InterfaceInteractionData()
{
	return openblack::Locator::scriptState::value().Get<InterfaceInteractionState>();
}

/// The two writes of every limiting level: | 2 then | 4 on the interface flags (bit 0, inactive, is kept)
void LimitInterface()
{
	interface_active::SetFlags(static_cast<uint8_t>(interface_active::GetFlags() | 2u));
	interface_active::SetFlags(static_cast<uint8_t>(interface_active::GetFlags() | 4u));
}

void Features(int32_t features)
{
	camera_help::EnableCameraFeatures(features, -1);
}

void TutorialAutoPitch()
{
	camera_help::SetAutoPitch(k_TutorialAutoPitchAngle, k_TutorialAutoPitchDistance, true);
}

void AllowCameraMovesAndRealmZooms(bool cameraMoves, bool realmZooms)
{
	auto& state = InterfaceInteractionData();
	state.cameraMovesAllowed = cameraMoves;
	state.realmZoomsAllowed = realmZooms;
}

/// The hand's reach, which the hand keeps. Nothing without a hand system (the unit tests)
void HandReach(int32_t level)
{
	const auto reach = LevelHandReach(level);
	if (!reach.has_value() || !Locator::handSystem::has_value())
	{
		return;
	}
	Locator::handSystem::value().SetHandReach(*reach);
}
} // namespace

void Set(int32_t level)
{
	InterfaceInteractionData().level = level; // before the range check
	switch (level)
	{
	case 0: // NORMAL: the whole byte = 0, so the interface is active again (reach 1800: HandReach below)
		interface_active::SetFlags(0);
		Features(camera_help::k_NormalFeatures);
		AllowCameraMovesAndRealmZooms(true, true);
		break;
	case 1: // JUST_GRAB (the reach 75)
		LimitInterface();
		Features(0x08);
		TutorialAutoPitch();
		AllowCameraMovesAndRealmZooms(true, false);
		break;
	case 2: // JUST_GRAB_FAR_TO_CITADEL
		LimitInterface();
		Features(0x08);
		TutorialAutoPitch();
		AllowCameraMovesAndRealmZooms(true, false);
		break;
	case 3: // JUST_ROTATE
		LimitInterface();
		Features(0x02);
		AllowCameraMovesAndRealmZooms(true, false);
		break;
	case 4: // JUST_DOUBLE_CLICK_AND_DRAG
		LimitInterface();
		Features(0x18);
		AllowCameraMovesAndRealmZooms(true, false);
		break;
	case 5: // JUST_ZOOM
		LimitInterface();
		Features(0x24);
		AllowCameraMovesAndRealmZooms(true, false);
		break;
	case 6: // JUST_ROTATE_INTERACT
		interface_active::SetFlags(0);
		Features(0x02);
		AllowCameraMovesAndRealmZooms(true, false);
		break;
	case 7: // JUST_ROTATE_INTERACT_AND_ZOOM
		interface_active::SetFlags(0);
		Features(0x26);
		AllowCameraMovesAndRealmZooms(true, false);
		break;
	case 8: // JUST_HAND_MOVE: the reach is not touched
		LimitInterface();
		Features(0);
		AllowCameraMovesAndRealmZooms(false, false);
		break;
	case 10: // JUST_ROTATE_AND_DRAG
		LimitInterface();
		Features(0x0A);
		TutorialAutoPitch();
		AllowCameraMovesAndRealmZooms(true, false);
		break;
	case 11: // JUST_PITCH
		LimitInterface();
		Features(0x01);
		AllowCameraMovesAndRealmZooms(true, false);
		break;
	case 12: // JUST_HAND_INTERACTION: only bit 2, the reach is not touched
		interface_active::SetFlags(static_cast<uint8_t>(interface_active::GetFlags() | 4u));
		Features(0);
		AllowCameraMovesAndRealmZooms(false, false);
		break;
	case 13: // JUST_GRAB_DOUBLE_CLICK_AND_ROTATE
		LimitInterface();
		Features(0x1A);
		AllowCameraMovesAndRealmZooms(true, false);
		break;
	case 14: // ..._AND_PITCH
		LimitInterface();
		Features(0x1B);
		AllowCameraMovesAndRealmZooms(true, false);
		break;
	case 15: // ..._PITCH_AND_ZOOM
		LimitInterface();
		Features(0x3F);
		AllowCameraMovesAndRealmZooms(true, false);
		break;
	default: // 9 and above 15 (unsigned)
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Unexpected interaction = {}", level);
		break;
	}
	// each level's reach. (inferred) done after the other writes: none of them reads the reach, so their order does
	// not matter
	HandReach(level);
}

std::optional<float> LevelHandReach(int32_t level)
{
	switch (level)
	{
	case 1: // JUST_GRAB: 75
		return k_JustGrabHandReach;
	case 8:  // JUST_HAND_MOVE
	case 12: // JUST_HAND_INTERACTION
		return std::nullopt;
	default:
		if (level < 0 || level > 15 || level == 9)
		{
			return std::nullopt;
		}
		return k_MaxHandReach;
	}
}

int32_t GetLevel()
{
	return InterfaceInteractionData().level;
}

bool CameraMovesAllowed()
{
	return InterfaceInteractionData().cameraMovesAllowed;
}

bool RealmZoomsAllowed()
{
	return InterfaceInteractionData().realmZoomsAllowed;
}

bool KeyShortcutsEnabled()
{
	auto& state = InterfaceInteractionData();
	return state.cameraMovesAllowed && state.realmZoomsAllowed;
}

bool IsActionBlocked(int32_t action)
{
	auto& state = InterfaceInteractionData();
	if (!state.cameraMovesAllowed)
	{
		// actions 3..19, all blocked but action 5
		return action >= 3 && action <= 19 && action != 5;
	}
	if (!state.realmZoomsAllowed)
	{
		return action >= 17 && action <= 19;
	}
	return false;
}

void Reset()
{
	InterfaceInteractionData().level = 0;
	AllowCameraMovesAndRealmZooms(true, true);
}

} // namespace openblack::help::interface_interaction
