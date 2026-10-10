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

#include <glm/vec2.hpp>

/// What the help's profile of the player counts of the world camera's controls each frame: turning, tilting, zooming,
/// double clicks and dragging the land. The tutorials read these counts to judge whether the player has learnt a
/// control. Pure, tested on its own.
namespace openblack::camera_help::events
{

/// A few of the help profile's camera events, at most once each
class EventSet
{
public:
	void Add(uint32_t event);
	void Add(EventSet other) { _bits |= other._bits; }
	[[nodiscard]] bool Contains(uint32_t event) const;
	[[nodiscard]] bool Empty() const { return _bits == 0; }
	/// Each event in the set, lowest first
	template <typename F>
	void ForEach(F&& f) const
	{
		for (uint32_t bit = 0; bit < k_Span; ++bit)
		{
			if ((_bits & (1u << bit)) != 0)
			{
				f(k_First + bit);
			}
		}
	}

	bool operator==(const EventSet&) const = default;

private:
	static constexpr uint32_t k_First = 25;
	static constexpr uint32_t k_Span = 9;
	uint32_t _bits {0};
};

/// The player's camera controls in a frame
struct ControlsFrame
{
	/// What the camera lets the player do this frame
	uint32_t features {0};
	/// The turning, tilting and zooming asked for by the keys, the wheel, the mouse turning round itself and both buttons,
	/// and the keys' movement across the land, before the features take any of them away
	float turn {0.0f};
	float tilt {0.0f};
	float zoom {0.0f};
	glm::vec2 move {0.0f, 0.0f};
	/// The hand grips the land, turning round the mouse, or zooming with both buttons
	bool landGripped {false};
	bool rotateAroundMouse {false};
	bool bothButtons {false};
	/// How far the mouse moved this frame, in pixels
	glm::ivec2 mouseDelta {0, 0};
};

/// Whether any turning, tilting, zooming or keys' movement is asked for. Gripping the land without turning, tilting or
/// zooming gives the keys' movement up.
[[nodiscard]] bool AnyInput(const ControlsFrame& frame);
/// Whether the land's grip drags the camera this frame: the land is gripped with no turning, tilting or zooming
[[nodiscard]] bool GripDrags(const ControlsFrame& frame);
/// Whether the player is moving the camera this frame: anything asked for, or gripping the land, turning round the mouse
/// or holding both buttons with the mouse moved more than 2 pixels either way
[[nodiscard]] bool IsMoving(const ControlsFrame& frame);

/// The events of the keys, wheel and buttons: zooming whenever allowed; while not dragging with the grip, turning by
/// more than a hundredth, tilting unless the camera tilts itself, and moving across the land
[[nodiscard]] EventSet InputEvents(const ControlsFrame& frame);
/// The events of a drag round the screen's edge turning the camera by an angle, in radians. Which way counts every
/// frame, moving or not, and a turn of no more than a hundredth counts as negative.
[[nodiscard]] EventSet EdgeTurnEvents(const ControlsFrame& frame, float angle);
/// The events of a drag tilting the camera
[[nodiscard]] EventSet TiltDragEvents(const ControlsFrame& frame);
/// The events of a drag of the land, its grip and the cursor both on the land
[[nodiscard]] EventSet LandDragEvents(const ControlsFrame& frame);
/// The events of a double click that flies the camera: on the object under the hand, else on the land under the cursor
[[nodiscard]] EventSet DoubleClickEvents(uint32_t features, bool objectUnderHand, bool landUnderCursor);

} // namespace openblack::camera_help::events
