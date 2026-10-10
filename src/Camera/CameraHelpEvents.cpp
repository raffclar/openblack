/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CameraHelpEvents.h"

#include <cmath>
#include <cstdlib>

#include "CameraHelp.h"
#include "Help/HelpProfile.h"

namespace openblack::camera_help::events
{

namespace
{
namespace profile = help::profile;

/// Turning by no more than this counts as no turn
constexpr float k_TurnThreshold = 0.01f;
/// The mouse moving by more than this many pixels either way moves the camera
constexpr int k_MouseMoveThreshold = 2;

/// The grip, unless turning round the mouse or both buttons take the mouse
bool Gripping(const ControlsFrame& frame)
{
	return frame.landGripped && !frame.rotateAroundMouse && !frame.bothButtons;
}

/// Turning, tilting, zooming or turning round the mouse: these keep the keys' movement while gripping
bool TurnTiltOrZoom(const ControlsFrame& frame)
{
	return frame.zoom != 0.0f || frame.turn != 0.0f || frame.tilt != 0.0f || frame.rotateAroundMouse;
}

bool Allows(uint32_t features, uint32_t feature)
{
	return (features & feature) != 0;
}
} // namespace

void EventSet::Add(uint32_t event)
{
	if (event >= k_First && event < k_First + k_Span)
	{
		_bits |= 1u << (event - k_First);
	}
}

bool EventSet::Contains(uint32_t event) const
{
	return event >= k_First && event < k_First + k_Span && (_bits & (1u << (event - k_First))) != 0;
}

bool AnyInput(const ControlsFrame& frame)
{
	if (TurnTiltOrZoom(frame))
	{
		return true;
	}
	return !Gripping(frame) && (frame.move.x != 0.0f || frame.move.y != 0.0f);
}

bool GripDrags(const ControlsFrame& frame)
{
	return Gripping(frame) && !TurnTiltOrZoom(frame);
}

bool IsMoving(const ControlsFrame& frame)
{
	if (AnyInput(frame))
	{
		return true;
	}
	const bool mouseMoved =
	    std::abs(frame.mouseDelta.x) > k_MouseMoveThreshold || std::abs(frame.mouseDelta.y) > k_MouseMoveThreshold;
	return (Gripping(frame) || frame.rotateAroundMouse || frame.bothButtons) && mouseMoved;
}

EventSet InputEvents(const ControlsFrame& frame)
{
	EventSet events;
	if (frame.zoom != 0.0f && Allows(frame.features, feature::k_Zoom))
	{
		events.Add(profile::k_CameraZoom);
	}
	if (!AnyInput(frame))
	{
		return events;
	}
	if (Allows(frame.features, feature::k_Rotate) && std::fabs(frame.turn) > k_TurnThreshold)
	{
		events.Add(profile::k_CameraTurn);
		events.Add(frame.turn > 0.0f ? profile::k_CameraTurnPositive : profile::k_CameraTurnNegative);
	}
	// The self-tilting camera takes the tilt over unless the land is gripped, and its own tilting isn't counted
	const bool selfTilting = Allows(frame.features, feature::k_AutoPitch) && !Gripping(frame);
	if (frame.tilt != 0.0f && Allows(frame.features, feature::k_Pitch) && !selfTilting)
	{
		events.Add(profile::k_CameraTilt);
	}
	if (Allows(frame.features, feature::k_Strafe) && (frame.move.x != 0.0f || frame.move.y != 0.0f))
	{
		events.Add(profile::k_CameraDrag);
	}
	return events;
}

EventSet EdgeTurnEvents(const ControlsFrame& frame, float angle)
{
	EventSet events;
	if (!GripDrags(frame) || !Allows(frame.features, feature::k_Rotate))
	{
		return events;
	}
	if (IsMoving(frame))
	{
		events.Add(profile::k_CameraTurn);
	}
	events.Add(angle > k_TurnThreshold ? profile::k_CameraTurnPositive : profile::k_CameraTurnNegative);
	return events;
}

EventSet TiltDragEvents(const ControlsFrame& frame)
{
	EventSet events;
	if (GripDrags(frame) && Allows(frame.features, feature::k_Pitch) && IsMoving(frame))
	{
		events.Add(profile::k_CameraTilt);
	}
	return events;
}

EventSet LandDragEvents(const ControlsFrame& frame)
{
	EventSet events;
	if (GripDrags(frame) && Allows(frame.features, feature::k_Strafe) && IsMoving(frame))
	{
		events.Add(profile::k_CameraDrag);
	}
	return events;
}

EventSet DoubleClickEvents(uint32_t features, bool objectUnderHand, bool landUnderCursor)
{
	EventSet events;
	// The feature that lets a double click fly the camera
	if (!Allows(features, feature::k_DoubleClick))
	{
		return events;
	}
	if (objectUnderHand)
	{
		events.Add(profile::k_CameraDoubleClickObject);
	}
	else if (landUnderCursor)
	{
		events.Add(profile::k_CameraDoubleClickLand);
	}
	return events;
}

} // namespace openblack::camera_help::events
